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
#include <windows.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define INI_MAX_LINE_LENGTH     4096
#define INI_MAX_SECTION_LENGTH  256
#define INI_MAX_KEY_LENGTH      256
#define INI_MAX_VALUE_LENGTH    2048
#define INI_INITIAL_CAPACITY    64

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
    struct IniSection* next;
} IniSection;

/**
 * @brief Complete INI file representation in memory
 */
typedef struct {
    IniSection* sections;
    char filePath[MAX_PATH];
    BOOL dirty;                 /* TRUE if modified since last save */
    FILETIME lastWriteTime;     /* For change detection */
} IniFile;

/* ============================================================================
 * Global state
 * ============================================================================ */

static IniFile* g_ConfigIni = NULL;
static CRITICAL_SECTION g_IniCriticalSection;
static volatile LONG g_IniCriticalSectionInitialized = 0;

/* ============================================================================
 * Thread safety
 * ============================================================================ */

static void EnsureCriticalSectionInitialized(void) {
    if (InterlockedCompareExchange(&g_IniCriticalSectionInitialized, 1, 0) == 0) {
        InitializeCriticalSection(&g_IniCriticalSection);
    }
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
    static HANDLE hMutex = NULL;
    if (hMutex == NULL) {
        hMutex = CreateMutexW(NULL, FALSE, L"CatimeConfigWriteMutex");
    }
    return hMutex;
}

static void AcquireConfigWriteLock(void) {
    HANDLE h = GetConfigWriteMutex();
    if (h) {
        WaitForSingleObject(h, INFINITE);
    }
}

static void ReleaseConfigWriteLock(void) {
    HANDLE h = GetConfigWriteMutex();
    if (h) {
        ReleaseMutex(h);
    }
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

static FILE* OpenFileUtf8(const char* utf8Path, const wchar_t* mode) {
    if (!utf8Path || !mode) return NULL;

    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH);

    return _wfopen(wPath, mode);
}

static BOOL GetFileTimeUtf8(const char* utf8Path, FILETIME* ft) {
    if (!utf8Path || !ft) return FALSE;

    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH);

    HANDLE hFile = CreateFileW(wPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    BOOL result = GetFileTime(hFile, NULL, NULL, ft);
    CloseHandle(hFile);
    return result;
}

static BOOL DeleteFileUtf8(const char* utf8Path) {
    if (!utf8Path) return FALSE;

    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH);

    return DeleteFileW(wPath);
}

static BOOL MoveFileUtf8(const char* utf8From, const char* utf8To) {
    if (!utf8From || !utf8To) return FALSE;

    wchar_t wFrom[MAX_PATH] = {0}, wTo[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8From, -1, wFrom, MAX_PATH);
    MultiByteToWideChar(CP_UTF8, 0, utf8To, -1, wTo, MAX_PATH);

    return MoveFileExW(wFrom, wTo, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

static BOOL FileExistsUtf8(const char* utf8Path) {
    if (!utf8Path) return FALSE;

    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH);

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
    } else {
        IniSection* last = ini->sections;
        while (last->next) last = last->next;
        last->next = section;
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
    } else {
        IniEntry* last = section->entries;
        while (last->next) last = last->next;
        last->next = entry;
    }

    return entry;
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

/**
 * @brief Parse INI file from disk into memory structure
 */
static IniFile* ParseIniFile(const char* filePath) {
    if (!filePath) return NULL;

    IniFile* ini = (IniFile*)calloc(1, sizeof(IniFile));
    if (!ini) return NULL;

    strncpy(ini->filePath, filePath, MAX_PATH - 1);

    FILE* f = OpenFileUtf8(filePath, L"rb");
    if (!f) {
        /* File doesn't exist - return empty INI */
        return ini;
    }

    SkipBOM(f);
    GetFileTimeUtf8(filePath, &ini->lastWriteTime);

    char line[INI_MAX_LINE_LENGTH];
    IniSection* currentSection = NULL;

    while (fgets(line, sizeof(line), f)) {
        /* Remove newline */
        size_t len = strlen(line);
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
                char* sectionName = TrimWhitespace(trimmed + 1);
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
            char* key = TrimWhitespace(trimmed);
            char* value = TrimWhitespace(eq + 1);

            IniEntry* entry = FindEntry(currentSection, key);
            if (entry) {
                /* Update existing */
                free(entry->value);
                entry->value = StrDup(value);
            } else {
                /* Create new */
                CreateEntry(currentSection, key, value);
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
static BOOL WriteIniToFile(IniFile* ini, const char* filePath) {
    if (!ini || !filePath) return FALSE;

    FILE* f = OpenFileUtf8(filePath, L"wb");
    if (!f) return FALSE;

    for (IniSection* section = ini->sections; section; section = section->next) {
        /* Write section header */
        fprintf(f, "[%s]\n", section->name);

        /* Write entries */
        for (IniEntry* entry = section->entries; entry; entry = entry->next) {
            fprintf(f, "%s=%s\n", entry->key, entry->value);
        }

        /* Blank line between sections */
        if (section->next) {
            fprintf(f, "\n");
        }
    }

    fclose(f);
    return TRUE;
}

/**
 * @brief Atomic write: write to temp file, then rename
 */
static BOOL WriteIniAtomically(IniFile* ini) {
    if (!ini || !ini->filePath[0]) return FALSE;

    char tempPath[MAX_PATH];
    snprintf(tempPath, sizeof(tempPath), "%s.tmp", ini->filePath);

    if (!WriteIniToFile(ini, tempPath)) {
        return FALSE;
    }

    if (!MoveFileUtf8(tempPath, ini->filePath)) {
        DeleteFileUtf8(tempPath);
        return FALSE;
    }

    ini->dirty = FALSE;
    GetFileTimeUtf8(ini->filePath, &ini->lastWriteTime);

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
static BOOL SetIniValue(const char* section, const char* key, const char* value, const char* filePath) {
    IniFile* ini = EnsureIniLoaded(filePath);
    if (!ini) return FALSE;

    IniSection* sec = FindSection(ini, section);
    if (!sec) {
        sec = CreateSection(ini, section);
        if (!sec) return FALSE;
    }

    IniEntry* entry = FindEntry(sec, key);
    if (entry) {
        free(entry->value);
        entry->value = StrDup(value ? value : "");
    } else {
        entry = CreateEntry(sec, key, value);
    }

    if (entry) {
        ini->dirty = TRUE;
    }

    return entry != NULL;
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

    const char* value = GetIniValue(section, key, filePath);
    const char* result = value ? value : (defaultValue ? defaultValue : "");

    strncpy(returnValue, result, returnSize - 1);
    returnValue[returnSize - 1] = '\0';

    DWORD len = (DWORD)strlen(returnValue);

    ReleaseIniLock();

    return len;
}

/**
 * @brief Write string value to INI file
 */
BOOL WriteIniString(const char* section, const char* key, const char* value,
                    const char* filePath) {
    if (!section || !key || !filePath) return FALSE;

    AcquireIniLock();
    AcquireConfigWriteLock();

    BOOL result = SetIniValue(section, key, value, filePath);

    /* Immediately flush to disk for compatibility with existing behavior */
    if (result && g_ConfigIni) {
        result = WriteIniAtomically(g_ConfigIni);
    }

    ReleaseConfigWriteLock();
    ReleaseIniLock();

    return result;
}

/**
 * @brief Read integer value from INI file
 */
int ReadIniInt(const char* section, const char* key, int defaultValue,
               const char* filePath) {
    char buffer[64];

    AcquireIniLock();

    const char* value = GetIniValue(section, key, filePath);

    int result = defaultValue;
    if (value && *value) {
        char* end;
        long parsed = strtol(value, &end, 10);
        if (end != value) {
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

    const char* value = GetIniValue(section, key, filePath);

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

    AcquireIniLock();
    AcquireConfigWriteLock();

    IniFile* ini = EnsureIniLoaded(filePath);
    if (!ini) {
        ReleaseConfigWriteLock();
        ReleaseIniLock();
        return FALSE;
    }

    /* Apply all updates */
    for (size_t i = 0; i < count; i++) {
        if (updates[i].section && updates[i].key && updates[i].value) {
            SetIniValue(updates[i].section, updates[i].key, updates[i].value, filePath);
        }
    }

    /* Single atomic write */
    BOOL result = WriteIniAtomically(ini);

    ReleaseConfigWriteLock();
    ReleaseIniLock();

    return result;
}

/**
 * @brief Force flush any cached changes to disk
 */
void FlushConfigToDisk(void) {
    AcquireIniLock();
    AcquireConfigWriteLock();

    if (g_ConfigIni && g_ConfigIni->dirty) {
        WriteIniAtomically(g_ConfigIni);
    }

    ReleaseConfigWriteLock();
    ReleaseIniLock();
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
