/**
 * @file config_ini.c
 * @brief Custom UTF-8 INI file parser and writer
 *
 * Pure C implementation without Windows INI API dependencies.
 * Features:
 * - Full UTF-8 support (no encoding conversion needed)
 * - Memory-first design with lazy file I/O
 * - Atomic writes via temp file + rename
 * - Thread-safe with mutex protection
 * - Line-start comments only (;# at line beginning)
 */

#include "config.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <windows.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define INI_MAX_LINE_LENGTH     4096
#define INI_MAX_SECTION_LENGTH  256
#define INI_MAX_KEY_LENGTH      256
#define INI_MAX_VALUE_LENGTH    2048
#define INI_INITIAL_CAPACITY    64
#define INI_MAX_PARSE_LINES     8192
#define INI_MAX_PARSE_ENTRIES   2048
#define INI_CACHE_STAT_THROTTLE_MS 100
#define INI_MAX_FILE_BYTES      (1024ull * 1024ull)

/* ============================================================================
 * Internal data structures
 * ============================================================================ */

/**
 * @brief Single key-value entry in the INI file
 */
typedef struct IniEntry {
    char* key;
    char* value;
    struct IniEntry* next;
} IniEntry;

/**
 * @brief Section containing multiple entries
 */
typedef struct IniSection {
    char* name;
    IniEntry* entries;
    IniEntry* lastEntry;
    struct IniSection* next;
} IniSection;

/**
 * @brief Complete INI file representation in memory
 */
typedef struct {
    IniSection* sections;
    IniSection* lastSection;
    char filePath[MAX_PATH];
    BOOL dirty;                 /* TRUE if modified since last save */
    FILETIME lastWriteTime;     /* For change detection */
    ULONGLONG lastStatCheckTick;/* Last file timestamp check */
} IniFile;

/* ============================================================================
 * Global state
 * ============================================================================ */

static IniFile* g_ConfigIni = NULL;
static CRITICAL_SECTION g_IniCriticalSection;
static volatile LONG g_IniCriticalSectionInitialized = 0;
static HANDLE g_ConfigWriteMutex = NULL;

#define INI_CS_UNINITIALIZED 0
#define INI_CS_INITIALIZING  1
#define INI_CS_INITIALIZED   2
#define INI_WAIT_SPIN_LIMIT 64
#define CONFIG_WRITE_LOCK_TIMEOUT_MS 2000

/* ============================================================================
 * Thread safety
 * ============================================================================ */

static void WaitWhileIniCSInitializing(void) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(&g_IniCriticalSectionInitialized, 0, 0) ==
           INI_CS_INITIALIZING) {
        Sleep(spins++ < INI_WAIT_SPIN_LIMIT ? 0 : 1);
    }
}

static void EnsureCriticalSectionInitialized(void) {
    if (InterlockedCompareExchange(&g_IniCriticalSectionInitialized,
                                   INI_CS_INITIALIZING,
                                   INI_CS_UNINITIALIZED) == INI_CS_UNINITIALIZED) {
        InitializeCriticalSection(&g_IniCriticalSection);
        InterlockedExchange(&g_IniCriticalSectionInitialized, INI_CS_INITIALIZED);
    }

    WaitWhileIniCSInitializing();
}

static void AcquireIniLock(void) {
    EnsureCriticalSectionInitialized();
    EnterCriticalSection(&g_IniCriticalSection);
}

static void ReleaseIniLock(void) {
    LeaveCriticalSection(&g_IniCriticalSection);
}

/* Global mutex for cross-process synchronization */
static HANDLE GetConfigWriteMutex(void) {
    HANDLE mutex = (HANDLE)InterlockedCompareExchangePointer(
        (PVOID volatile*)&g_ConfigWriteMutex, NULL, NULL);
    if (mutex) {
        return mutex;
    }

    HANDLE newMutex = CreateMutexW(NULL, FALSE, L"CatimeConfigWriteMutex");
    if (!newMutex) {
        return NULL;
    }

    HANDLE existing = (HANDLE)InterlockedCompareExchangePointer(
        (PVOID volatile*)&g_ConfigWriteMutex, newMutex, NULL);
    if (existing) {
        CloseHandle(newMutex);
        return existing;
    }

    return newMutex;
}

static BOOL AcquireConfigWriteLock(void) {
    HANDLE h = GetConfigWriteMutex();
    if (!h) {
        LOG_WARNING("Config write mutex unavailable");
        return FALSE;
    }

    DWORD waitResult = WaitForSingleObject(h, CONFIG_WRITE_LOCK_TIMEOUT_MS);
    if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED) {
        return TRUE;
    }

    if (waitResult == WAIT_TIMEOUT) {
        LOG_WARNING("Timed out waiting for config write mutex after %lu ms",
                    (DWORD)CONFIG_WRITE_LOCK_TIMEOUT_MS);
    } else {
        LOG_WARNING("Failed waiting for config write mutex (result=%lu, error=%lu)",
                    waitResult, GetLastError());
    }
    return FALSE;
}

static void ReleaseConfigWriteLock(void) {
    HANDLE h = GetConfigWriteMutex();
    if (h) {
        ReleaseMutex(h);
    }
}

static void ReleaseConfigWriteAndIniLocks(void) {
    ReleaseIniLock();
    ReleaseConfigWriteLock();
}

/* ============================================================================
 * String utilities
 * ============================================================================ */

static char* StrDup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)malloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

static char* TrimWhitespace(char* s) {
    if (!s) return NULL;

    /* Trim leading */
    while (*s && isspace((unsigned char)*s)) s++;

    if (*s == '\0') return s;

    /* Trim trailing */
    char* end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return s;
}

static BOOL StrEqualNoCase(const char* a, const char* b) {
    if (!a || !b) return (a == b);
    return _stricmp(a, b) == 0;
}

/* ============================================================================
 * File utilities with UTF-8 path support
 * ============================================================================ */

static BOOL Utf8PathToWide(const char* utf8Path, wchar_t* wPath, size_t wPathSize) {
    if (!wPath || wPathSize == 0) return FALSE;

    wPath[0] = L'\0';
    if (!utf8Path || wPathSize > INT_MAX) return FALSE;

    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1,
                            wPath, (int)wPathSize) <= 0) {
        wPath[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL WidePathToUtf8(const wchar_t* wPath, char* utf8Path, size_t utf8PathSize) {
    if (!utf8Path || utf8PathSize == 0) return FALSE;

    utf8Path[0] = '\0';
    if (!wPath || utf8PathSize > INT_MAX) return FALSE;

    if (WideCharToMultiByte(CP_UTF8, 0, wPath, -1,
                            utf8Path, (int)utf8PathSize, NULL, NULL) <= 0) {
        utf8Path[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL CreateTempFilePathForTargetUtf8(const char* targetPath, char* tempPath, size_t tempPathSize) {
    if (!targetPath || !tempPath || tempPathSize == 0) return FALSE;
    tempPath[0] = '\0';

    wchar_t wTarget[MAX_PATH] = {0};
    if (!Utf8PathToWide(targetPath, wTarget, MAX_PATH)) {
        return FALSE;
    }

    wchar_t wDir[MAX_PATH] = {0};
    wcsncpy(wDir, wTarget, MAX_PATH - 1);
    wDir[MAX_PATH - 1] = L'\0';

    wchar_t* lastSlash = wcsrchr(wDir, L'\\');
    wchar_t* lastForwardSlash = wcsrchr(wDir, L'/');
    if (!lastSlash || (lastForwardSlash && lastForwardSlash > lastSlash)) {
        lastSlash = lastForwardSlash;
    }

    if (lastSlash) {
        *lastSlash = L'\0';
    } else {
        wcscpy_s(wDir, MAX_PATH, L".");
    }

    wchar_t wTemp[MAX_PATH] = {0};
    if (GetTempFileNameW(wDir, L"cti", 0, wTemp) == 0) {
        LOG_ERROR("Failed to create config temp file in directory for: %s (error=%lu)",
                  targetPath, GetLastError());
        return FALSE;
    }

    if (!WidePathToUtf8(wTemp, tempPath, tempPathSize)) {
        DeleteFileW(wTemp);
        return FALSE;
    }

    return TRUE;
}

static FILE* OpenFileUtf8(const char* utf8Path, const wchar_t* mode) {
    if (!utf8Path || !mode) return NULL;

    wchar_t wPath[MAX_PATH] = {0};
    if (!Utf8PathToWide(utf8Path, wPath, MAX_PATH)) return NULL;

    return _wfopen(wPath, mode);
}

static BOOL GetFileTimeUtf8(const char* utf8Path, FILETIME* ft) {
    if (!utf8Path || !ft) return FALSE;

    wchar_t wPath[MAX_PATH] = {0};
    if (!Utf8PathToWide(utf8Path, wPath, MAX_PATH)) return FALSE;

    HANDLE hFile = CreateFileW(wPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    BOOL result = GetFileTime(hFile, NULL, NULL, ft);
    CloseHandle(hFile);
    return result;
}

static BOOL GetFileSizeUtf8(const char* utf8Path, ULONGLONG* outSize) {
    if (!utf8Path || !outSize) return FALSE;
    *outSize = 0;

    wchar_t wPath[MAX_PATH] = {0};
    if (!Utf8PathToWide(utf8Path, wPath, MAX_PATH)) return FALSE;

    HANDLE hFile = CreateFileW(wPath, GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    LARGE_INTEGER size;
    BOOL result = GetFileSizeEx(hFile, &size);
    CloseHandle(hFile);
    if (!result || size.QuadPart < 0) return FALSE;

    *outSize = (ULONGLONG)size.QuadPart;
    return TRUE;
}

static ULONGLONG GetIniCacheTickMs(void) {
    return GetTickCount64();
}

static BOOL DeleteFileUtf8(const char* utf8Path) {
    if (!utf8Path) return FALSE;

    wchar_t wPath[MAX_PATH] = {0};
    if (!Utf8PathToWide(utf8Path, wPath, MAX_PATH)) return FALSE;

    return DeleteFileW(wPath);
}

static BOOL MoveFileUtf8(const char* utf8From, const char* utf8To) {
    if (!utf8From || !utf8To) return FALSE;

    wchar_t wFrom[MAX_PATH] = {0}, wTo[MAX_PATH] = {0};
    if (!Utf8PathToWide(utf8From, wFrom, MAX_PATH) ||
        !Utf8PathToWide(utf8To, wTo, MAX_PATH)) {
        return FALSE;
    }

    return MoveFileExW(wFrom, wTo, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

static BOOL FileExistsUtf8(const char* utf8Path) {
    if (!utf8Path) return FALSE;

    wchar_t wPath[MAX_PATH] = {0};
    if (!Utf8PathToWide(utf8Path, wPath, MAX_PATH)) return FALSE;

    return GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES;
}

/* ============================================================================
 * INI structure management
 * ============================================================================ */

static void FreeEntry(IniEntry* entry) {
    if (!entry) return;
    free(entry->key);
    free(entry->value);
    free(entry);
}

static void FreeSection(IniSection* section) {
    if (!section) return;

    IniEntry* entry = section->entries;
    while (entry) {
        IniEntry* next = entry->next;
        FreeEntry(entry);
        entry = next;
    }

    free(section->name);
    free(section);
}

static void FreeIniFile(IniFile* ini) {
    if (!ini) return;

    IniSection* section = ini->sections;
    while (section) {
        IniSection* next = section->next;
        FreeSection(section);
        section = next;
    }

    free(ini);
}

static IniSection* FindSection(IniFile* ini, const char* name) {
    if (!ini || !name) return NULL;

    for (IniSection* s = ini->sections; s; s = s->next) {
        if (StrEqualNoCase(s->name, name)) {
            return s;
        }
    }
    return NULL;
}

static IniSection* CreateSection(IniFile* ini, const char* name) {
    if (!ini || !name) return NULL;

    IniSection* section = (IniSection*)calloc(1, sizeof(IniSection));
    if (!section) return NULL;

    section->name = StrDup(name);
    if (!section->name) {
        free(section);
        return NULL;
    }

    /* Add to end of list to preserve order */
    if (!ini->sections) {
        ini->sections = section;
        ini->lastSection = section;
    } else {
        IniSection* last = ini->lastSection;
        if (!last) {
            last = ini->sections;
            while (last->next) last = last->next;
        }
        last->next = section;
        ini->lastSection = section;
    }

    return section;
}

static IniEntry* FindEntry(IniSection* section, const char* key) {
    if (!section || !key) return NULL;

    for (IniEntry* e = section->entries; e; e = e->next) {
        if (StrEqualNoCase(e->key, key)) {
            return e;
        }
    }
    return NULL;
}

static IniEntry* CreateEntry(IniSection* section, const char* key, const char* value) {
    if (!section || !key) return NULL;

    IniEntry* entry = (IniEntry*)calloc(1, sizeof(IniEntry));
    if (!entry) return NULL;

    entry->key = StrDup(key);
    entry->value = StrDup(value ? value : "");

    if (!entry->key || !entry->value) {
        FreeEntry(entry);
        return NULL;
    }

    /* Add to end of list to preserve order */
    if (!section->entries) {
        section->entries = entry;
        section->lastEntry = entry;
    } else {
        IniEntry* last = section->lastEntry;
        if (!last) {
            last = section->entries;
            while (last->next) last = last->next;
        }
        last->next = entry;
        section->lastEntry = entry;
    }

    return entry;
}

static IniFile* CloneIniFile(const IniFile* src) {
    if (!src) return NULL;

    IniFile* clone = (IniFile*)calloc(1, sizeof(IniFile));
    if (!clone) return NULL;

    strncpy(clone->filePath, src->filePath, MAX_PATH - 1);
    clone->dirty = src->dirty;
    clone->lastWriteTime = src->lastWriteTime;
    clone->lastStatCheckTick = src->lastStatCheckTick;

    for (const IniSection* section = src->sections; section; section = section->next) {
        IniSection* newSection = CreateSection(clone, section->name);
        if (!newSection) {
            FreeIniFile(clone);
            return NULL;
        }

        for (const IniEntry* entry = section->entries; entry; entry = entry->next) {
            if (!CreateEntry(newSection, entry->key, entry->value)) {
                FreeIniFile(clone);
                return NULL;
            }
        }
    }

    return clone;
}

/* ============================================================================
 * INI file parsing
 * ============================================================================ */

/**
 * @brief Skip UTF-8 BOM if present
 */
static void SkipBOM(FILE* f) {
    int c1 = fgetc(f);
    int c2 = fgetc(f);
    int c3 = fgetc(f);

    /* UTF-8 BOM: EF BB BF */
    if (c1 == 0xEF && c2 == 0xBB && c3 == 0xBF) {
        return; /* BOM skipped */
    }

    /* Not a BOM, rewind */
    if (c3 != EOF) ungetc(c3, f);
    if (c2 != EOF) ungetc(c2, f);
    if (c1 != EOF) ungetc(c1, f);
}

static void DiscardRestOfLine(FILE* f) {
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        /* Discard an overlong physical line so partial keys are not parsed. */
    }
}

/**
 * @brief Parse INI file from disk into memory structure
 */
static IniFile* ParseIniFile(const char* filePath) {
    if (!filePath) return NULL;

    IniFile* ini = (IniFile*)calloc(1, sizeof(IniFile));
    if (!ini) return NULL;

    strncpy(ini->filePath, filePath, MAX_PATH - 1);
    ini->lastStatCheckTick = GetIniCacheTickMs();

    ULONGLONG fileSize = 0;
    if (GetFileSizeUtf8(filePath, &fileSize) && fileSize > INI_MAX_FILE_BYTES) {
        LOG_WARNING("INI file too large, ignoring %s (%llu bytes, limit %llu)",
                    filePath, fileSize, (ULONGLONG)INI_MAX_FILE_BYTES);
        return ini;
    }

    FILE* f = OpenFileUtf8(filePath, L"rb");
    if (!f) {
        /* File doesn't exist - return empty INI */
        return ini;
    }

    SkipBOM(f);
    GetFileTimeUtf8(filePath, &ini->lastWriteTime);

    char line[INI_MAX_LINE_LENGTH];
    IniSection* currentSection = NULL;
    DWORD parsedLines = 0;
    DWORD parsedEntries = 0;

    while (fgets(line, sizeof(line), f)) {
        if (++parsedLines > INI_MAX_PARSE_LINES) {
            LOG_WARNING("INI parse line limit reached for %s (%lu lines)",
                        filePath, parsedLines - 1);
            break;
        }

        /* Reject overlong physical lines instead of parsing truncated prefixes. */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] != '\n' && !feof(f)) {
            DiscardRestOfLine(f);
            LOG_WARNING("INI line too long in %s at line %lu; skipped",
                        filePath, parsedLines);
            continue;
        }

        /* Remove newline */
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        char* trimmed = TrimWhitespace(line);

        /* Skip empty lines and comments */
        if (*trimmed == '\0' || *trimmed == ';' || *trimmed == '#') {
            continue;
        }

        /* Section header */
        if (*trimmed == '[') {
            char* end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                const char* sectionName = TrimWhitespace(trimmed + 1);
                currentSection = FindSection(ini, sectionName);
                if (!currentSection) {
                    currentSection = CreateSection(ini, sectionName);
                }
            }
            continue;
        }

        /* Key=Value */
        char* eq = strchr(trimmed, '=');
        if (eq && currentSection) {
            *eq = '\0';
            const char* key = TrimWhitespace(trimmed);
            const char* value = TrimWhitespace(eq + 1);

            IniEntry* entry = FindEntry(currentSection, key);
            if (entry) {
                /* Update existing */
                char* newValue = StrDup(value);
                if (!newValue) {
                    continue;
                }
                free(entry->value);
                entry->value = newValue;
            } else {
                if (parsedEntries >= INI_MAX_PARSE_ENTRIES) {
                    LOG_WARNING("INI parse entry limit reached for %s (%lu entries)",
                                filePath, parsedEntries);
                    break;
                }

                /* Create new */
                if (CreateEntry(currentSection, key, value)) {
                    parsedEntries++;
                }
            }
        }
    }

    fclose(f);
    return ini;
}

/* ============================================================================
 * INI file writing
 * ============================================================================ */

/**
 * @brief Write INI structure to file (UTF-8, no BOM)
 */
static BOOL WriteIniToFile(const IniFile* ini, const char* filePath) {
    if (!ini || !filePath) return FALSE;

    FILE* f = OpenFileUtf8(filePath, L"wb");
    if (!f) return FALSE;

    BOOL success = TRUE;
    for (const IniSection* section = ini->sections; section; section = section->next) {
        /* Write section header */
        if (fprintf(f, "[%s]\n", section->name) < 0) {
            success = FALSE;
            break;
        }

        /* Write entries */
        for (const IniEntry* entry = section->entries; entry; entry = entry->next) {
            if (fprintf(f, "%s=%s\n", entry->key, entry->value) < 0) {
                success = FALSE;
                break;
            }
        }
        if (!success) {
            break;
        }

        /* Blank line between sections */
        if (section->next) {
            if (fprintf(f, "\n") < 0) {
                success = FALSE;
                break;
            }
        }
    }

    if (ferror(f)) {
        success = FALSE;
    }
    if (fclose(f) != 0) {
        success = FALSE;
    }

    if (!success) {
        LOG_ERROR("Failed to write config file: %s", filePath);
    }
    return success;
}

/**
 * @brief Atomic write: write to temp file, then rename
 */
static BOOL WriteIniAtomically(IniFile* ini) {
    if (!ini || !ini->filePath[0]) return FALSE;

    char tempPath[MAX_PATH];
    if (!CreateTempFilePathForTargetUtf8(ini->filePath, tempPath, sizeof(tempPath))) {
        return FALSE;
    }

    if (!WriteIniToFile(ini, tempPath)) {
        DeleteFileUtf8(tempPath);
        return FALSE;
    }

    if (!MoveFileUtf8(tempPath, ini->filePath)) {
        DeleteFileUtf8(tempPath);
        return FALSE;
    }

    ini->dirty = FALSE;
    GetFileTimeUtf8(ini->filePath, &ini->lastWriteTime);
    ini->lastStatCheckTick = GetIniCacheTickMs();

    return TRUE;
}

/* ============================================================================
 * INI cache management
 * ============================================================================ */

/**
 * @brief Ensure global INI is loaded for the given file
 */
static IniFile* EnsureIniLoaded(const char* filePath) {
    if (!filePath) return NULL;

    /* If already loaded for this file, return it */
    if (g_ConfigIni && StrEqualNoCase(g_ConfigIni->filePath, filePath)) {
        return g_ConfigIni;
    }

    /* Free old and load new */
    if (g_ConfigIni) {
        FreeIniFile(g_ConfigIni);
    }

    g_ConfigIni = ParseIniFile(filePath);
    return g_ConfigIni;
}

static BOOL IsZeroFileTime(const FILETIME* ft) {
    return ft && ft->dwLowDateTime == 0 && ft->dwHighDateTime == 0;
}

static BOOL ReloadIniCacheFromDisk(const char* filePath) {
    IniFile* reloaded = ParseIniFile(filePath);
    if (!reloaded) {
        return FALSE;
    }

    if (g_ConfigIni) {
        FreeIniFile(g_ConfigIni);
    }
    g_ConfigIni = reloaded;
    return TRUE;
}

static BOOL RefreshCleanIniCacheIfChangedInternal(const char* filePath, BOOL forceStatCheck) {
    IniFile* ini = EnsureIniLoaded(filePath);
    if (!ini) {
        return FALSE;
    }

    if (ini->dirty) {
        return TRUE;
    }

    ULONGLONG now = GetIniCacheTickMs();
    if (!forceStatCheck &&
        ini->lastStatCheckTick != 0 &&
        now - ini->lastStatCheckTick < INI_CACHE_STAT_THROTTLE_MS) {
        return TRUE;
    }

    FILETIME currentWriteTime;
    if (!GetFileTimeUtf8(filePath, &currentWriteTime)) {
        if (IsZeroFileTime(&ini->lastWriteTime)) {
            ini->lastStatCheckTick = now;
            return TRUE;
        }
        return ReloadIniCacheFromDisk(filePath);
    }

    if (CompareFileTime(&ini->lastWriteTime, &currentWriteTime) == 0) {
        ini->lastStatCheckTick = now;
        return TRUE;
    }

    return ReloadIniCacheFromDisk(filePath);
}

static BOOL RefreshCleanIniCacheIfChanged(const char* filePath) {
    return RefreshCleanIniCacheIfChangedInternal(filePath, FALSE);
}

static BOOL RefreshCleanIniCacheForWrite(const char* filePath) {
    return RefreshCleanIniCacheIfChangedInternal(filePath, TRUE);
}

/**
 * @brief Get value from INI (internal, must hold lock)
 */
static const char* GetIniValue(const char* section, const char* key, const char* filePath) {
    IniFile* ini = EnsureIniLoaded(filePath);
    if (!ini) return NULL;

    IniSection* sec = FindSection(ini, section);
    if (!sec) return NULL;

    IniEntry* entry = FindEntry(sec, key);
    return entry ? entry->value : NULL;
}

/**
 * @brief Set value in INI (internal, must hold lock)
 */
static BOOL SetIniValueInMemory(IniFile* ini, const char* section, const char* key, const char* value) {
    if (!ini || !section || !key) return FALSE;
    IniSection* sec = FindSection(ini, section);
    if (!sec) {
        sec = CreateSection(ini, section);
        if (!sec) return FALSE;
    }

    IniEntry* entry = FindEntry(sec, key);
    if (entry) {
        const char* newValue = value ? value : "";
        if (entry->value && strcmp(entry->value, newValue) == 0) {
            return TRUE;
        }
        char* newValueCopy = StrDup(newValue);
        if (!newValueCopy) {
            return FALSE;
        }
        free(entry->value);
        entry->value = newValueCopy;
    } else {
        entry = CreateEntry(sec, key, value);
    }

    if (entry) {
        ini->dirty = TRUE;
    }

    return entry != NULL;
}

static BOOL IniValueMatches(IniFile* ini, const char* section,
                            const char* key, const char* value) {
    if (!ini || !section || !key) return FALSE;

    IniSection* sec = FindSection(ini, section);
    IniEntry* entry = sec ? FindEntry(sec, key) : NULL;
    const char* currentValue = entry ? entry->value : NULL;
    return currentValue && strcmp(currentValue, value ? value : "") == 0;
}

static BOOL IniUpdatesMatch(IniFile* ini, const IniKeyValue* updates,
                            size_t count) {
    if (!ini || !updates) return FALSE;

    for (size_t i = 0; i < count; i++) {
        if (!updates[i].section || !updates[i].key || !updates[i].value) {
            continue;
        }

        if (!IniValueMatches(ini, updates[i].section,
                             updates[i].key, updates[i].value)) {
            return FALSE;
        }
    }

    return TRUE;
}

/* ============================================================================
 * Public API - Compatible with existing interface
 * ============================================================================ */

/**
 * @brief Check if file exists with Unicode support
 */
BOOL FileExists(const char* filePath) {
    return FileExistsUtf8(filePath);
}

/**
 * @brief Read string value from INI file
 */
DWORD ReadIniString(const char* section, const char* key, const char* defaultValue,
                    char* returnValue, DWORD returnSize, const char* filePath) {
    if (!returnValue || returnSize == 0) return 0;

    AcquireIniLock();

    const char* value = RefreshCleanIniCacheIfChanged(filePath)
        ? GetIniValue(section, key, filePath)
        : NULL;
    const char* result = value ? value : (defaultValue ? defaultValue : "");

    strncpy(returnValue, result, returnSize - 1);
    returnValue[returnSize - 1] = '\0';

    DWORD len = (DWORD)strlen(returnValue);

    ReleaseIniLock();

    return len;
}

BOOL ReadIniStringExact(const char* section, const char* key, const char* defaultValue,
                        char* returnValue, DWORD returnSize, const char* filePath) {
    if (!returnValue || returnSize == 0) return FALSE;

    AcquireIniLock();

    const char* value = RefreshCleanIniCacheIfChanged(filePath)
        ? GetIniValue(section, key, filePath)
        : NULL;
    const char* result = value ? value : (defaultValue ? defaultValue : "");
    size_t resultLen = strlen(result);

    if (resultLen >= returnSize) {
        returnValue[0] = '\0';
        ReleaseIniLock();
        return FALSE;
    }

    memcpy(returnValue, result, resultLen + 1);

    ReleaseIniLock();
    return TRUE;
}

/**
 * @brief Write string value to INI file
 */
BOOL WriteIniString(const char* section, const char* key, const char* value,
                    const char* filePath) {
    if (!section || !key || !filePath) return FALSE;

    /*
     * Avoid taking the cross-process write mutex for clean no-op writes.
     * If the cache is dirty, fall through so the existing flush path can run.
     */
    AcquireIniLock();
    IniFile* currentIni = RefreshCleanIniCacheForWrite(filePath) ? g_ConfigIni : NULL;
    if (currentIni && !currentIni->dirty) {
        if (IniValueMatches(currentIni, section, key, value)) {
            ReleaseIniLock();
            return TRUE;
        }
    }
    ReleaseIniLock();

    if (!AcquireConfigWriteLock()) {
        return FALSE;
    }
    AcquireIniLock();

    if (!RefreshCleanIniCacheForWrite(filePath)) {
        ReleaseConfigWriteAndIniLocks();
        return FALSE;
    }

    if (g_ConfigIni && !g_ConfigIni->dirty &&
        IniValueMatches(g_ConfigIni, section, key, value)) {
        ReleaseConfigWriteAndIniLocks();
        return TRUE;
    }

    IniFile* pendingIni = CloneIniFile(g_ConfigIni);
    if (!pendingIni) {
        ReleaseConfigWriteAndIniLocks();
        return FALSE;
    }

    BOOL result = SetIniValueInMemory(pendingIni, section, key, value);

    /* Flush only when SetIniValue actually changed the cached INI. */
    if (result && pendingIni->dirty) {
        result = WriteIniAtomically(pendingIni);
    }

    if (result) {
        FreeIniFile(g_ConfigIni);
        g_ConfigIni = pendingIni;
        pendingIni = NULL;
    }

    FreeIniFile(pendingIni);

    ReleaseConfigWriteAndIniLocks();

    return result;
}

/**
 * @brief Read integer value from INI file
 */
int ReadIniInt(const char* section, const char* key, int defaultValue,
               const char* filePath) {
    AcquireIniLock();

    const char* value = RefreshCleanIniCacheIfChanged(filePath)
        ? GetIniValue(section, key, filePath)
        : NULL;

    int result = defaultValue;
    if (value && *value) {
        char* end;
        errno = 0;
        long parsed = strtol(value, &end, 10);
        if (end != value && errno != ERANGE &&
            parsed >= (long)INT_MIN && parsed <= (long)INT_MAX) {
            result = (int)parsed;
        }
    }

    ReleaseIniLock();

    return result;
}

/**
 * @brief Write integer value to INI file
 */
BOOL WriteIniInt(const char* section, const char* key, int value,
                 const char* filePath) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return WriteIniString(section, key, buffer, filePath);
}

/**
 * @brief Read boolean value from INI file
 */
BOOL ReadIniBool(const char* section, const char* key, BOOL defaultValue,
                 const char* filePath) {
    AcquireIniLock();

    const char* value = RefreshCleanIniCacheIfChanged(filePath)
        ? GetIniValue(section, key, filePath)
        : NULL;

    BOOL result = defaultValue;
    if (value && *value) {
        result = (StrEqualNoCase(value, "TRUE") ||
                  StrEqualNoCase(value, "1") ||
                  StrEqualNoCase(value, "YES"));
    }

    ReleaseIniLock();

    return result;
}

/**
 * @brief Write boolean value to INI file
 */
BOOL WriteIniBool(const char* section, const char* key, BOOL value,
                  const char* filePath) {
    return WriteIniString(section, key, value ? "TRUE" : "FALSE", filePath);
}

/**
 * @brief Atomic update of single key-value
 */
BOOL UpdateConfigKeyValueAtomic(const char* section, const char* key, const char* value) {
    if (!section || !key || !value) return FALSE;

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    return WriteIniString(section, key, value, config_path);
}

/**
 * @brief Atomic update of integer value
 */
BOOL UpdateConfigIntAtomic(const char* section, const char* key, int value) {
    if (!section || !key) return FALSE;

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    return WriteIniInt(section, key, value, config_path);
}

/**
 * @brief Atomic update of boolean value
 */
BOOL UpdateConfigBoolAtomic(const char* section, const char* key, BOOL value) {
    if (!section || !key) return FALSE;

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    return WriteIniBool(section, key, value, config_path);
}

/**
 * @brief Batch write multiple INI values atomically
 */
BOOL WriteIniMultipleAtomic(const char* filePath, const IniKeyValue* updates, size_t count) {
    if (!filePath || !updates || count == 0) return FALSE;

    /*
     * Fast path for clean no-op batches. This avoids taking the cross-process
     * write mutex when a full/section save is replaying values already in cache.
     */
    AcquireIniLock();
    IniFile* currentIni = RefreshCleanIniCacheForWrite(filePath) ? g_ConfigIni : NULL;
    if (currentIni && !currentIni->dirty) {
        if (IniUpdatesMatch(currentIni, updates, count)) {
            ReleaseIniLock();
            return TRUE;
        }
    }
    ReleaseIniLock();

    if (!AcquireConfigWriteLock()) {
        return FALSE;
    }
    AcquireIniLock();

    if (!RefreshCleanIniCacheForWrite(filePath)) {
        ReleaseConfigWriteAndIniLocks();
        return FALSE;
    }
    if (g_ConfigIni && !g_ConfigIni->dirty &&
        IniUpdatesMatch(g_ConfigIni, updates, count)) {
        ReleaseConfigWriteAndIniLocks();
        return TRUE;
    }

    IniFile* pendingIni = CloneIniFile(g_ConfigIni);
    if (!pendingIni) {
        ReleaseConfigWriteAndIniLocks();
        return FALSE;
    }

    /* Apply all updates */
    BOOL result = TRUE;
    for (size_t i = 0; i < count; i++) {
        if (updates[i].section && updates[i].key && updates[i].value) {
            if (!SetIniValueInMemory(pendingIni, updates[i].section, updates[i].key, updates[i].value)) {
                result = FALSE;
                break;
            }
        }
    }

    if (result && pendingIni->dirty) {
        result = WriteIniAtomically(pendingIni);
    }

    if (result) {
        FreeIniFile(g_ConfigIni);
        g_ConfigIni = pendingIni;
        pendingIni = NULL;
    }

    FreeIniFile(pendingIni);

    ReleaseConfigWriteAndIniLocks();

    return result;
}

/**
 * @brief Force flush any cached changes to disk
 */
BOOL FlushConfigToDisk(void) {
    if (!AcquireConfigWriteLock()) {
        return FALSE;
    }
    AcquireIniLock();

    BOOL result = TRUE;
    if (g_ConfigIni && g_ConfigIni->dirty) {
        result = WriteIniAtomically(g_ConfigIni);
        if (!result) {
            LOG_ERROR("Failed to flush config cache to disk: %s", g_ConfigIni->filePath);
        }
    }

    ReleaseConfigWriteAndIniLocks();
    return result;
}

/**
 * @brief Invalidate cache (call when external changes detected)
 */
void InvalidateIniCache(void) {
    AcquireIniLock();

    if (g_ConfigIni) {
        FreeIniFile(g_ConfigIni);
        g_ConfigIni = NULL;
    }

    ReleaseIniLock();
}

void ShutdownIniCache(void) {
    WaitWhileIniCSInitializing();

    if (InterlockedCompareExchange(&g_IniCriticalSectionInitialized, 0, 0) ==
        INI_CS_INITIALIZED) {
        AcquireIniLock();
        if (g_ConfigIni) {
            FreeIniFile(g_ConfigIni);
            g_ConfigIni = NULL;
        }
        ReleaseIniLock();
        DeleteCriticalSection(&g_IniCriticalSection);
        g_IniCriticalSectionInitialized = INI_CS_UNINITIALIZED;
    } else if (g_ConfigIni) {
        FreeIniFile(g_ConfigIni);
        g_ConfigIni = NULL;
    }

    HANDLE mutex = (HANDLE)InterlockedExchangePointer(
        (PVOID volatile*)&g_ConfigWriteMutex, NULL);
    if (mutex) {
        CloseHandle(mutex);
    }
}
