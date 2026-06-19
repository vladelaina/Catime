/**
 * @file tray_menu_font.c
 * @brief Font scanning and menu construction logic (async scan cache)
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>
#include <limits.h>
#include "log.h"
#include "language.h"
#include "tray/tray_menu.h"
#include "tray/tray_menu_font.h"
#include "config.h"
#include "../resource/resource.h"
#include "utils/string_convert.h"
#include "utils/natural_sort.h"
#include "utils/string_format.h"
#include "font/font_path_manager.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_FONT_ENTRIES FONT_MENU_MAX_ENTRIES
#define MAX_RECURSION_DEPTH 10
#define MAX_FONT_NAME_LENGTH 260
#define MAX_FONT_SCAN_ENTRIES 4096
#define ASYNC_FONT_SCAN_STOP_TIMEOUT_MS 2000
#define FONT_MENU_SCAN_REFRESH_COOLDOWN_MS 10000
#define FONT_MENU_SCAN_FAILED (-1)

#if CMD_FONT_SELECTION_BASE + MAX_FONT_ENTRIES > CLOCK_IDM_ANIMATIONS_MENU
#error "Font menu command range overlaps animation menu identifiers"
#endif

#if CLOCK_IDM_SYSTEM_FONT_PICKER >= CLOCK_IDM_ANIMATIONS_BASE && CLOCK_IDM_SYSTEM_FONT_PICKER < CLOCK_IDM_ANIMATIONS_END
#error "System font picker menu ID overlaps dynamic animation menu command range"
#endif

/* ============================================================================
 * External dependencies
 * ============================================================================ */

extern char FONT_FILE_NAME[MAX_PATH];
extern void GetConfigPath(char* path, size_t size);
extern BOOL NeedsFontLicenseVersionAcceptance(void);
extern BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance);

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/**
 * @brief Single font entry for menu building
 */
typedef struct {
    wchar_t fileName[MAX_FONT_NAME_LENGTH];
    wchar_t relativePath[MAX_PATH];
    wchar_t displayName[MAX_FONT_NAME_LENGTH];
} FontEntry;

/**
 * @brief Font scan context
 */
typedef struct {
    FontEntry* entries;
    int count;
    int capacity;
    int scannedEntries;
    BOOL truncated;
    BOOL full;
    BOOL failed;
} FontScanContext;

typedef struct {
    UINT id;
    wchar_t relativePath[MAX_PATH];
} FontMenuIdMapEntry;

static FontMenuIdMapEntry g_fontMenuIdMap[MAX_FONT_ENTRIES];
static int g_fontMenuIdMapCount = 0;
static FontEntry g_fontMenuCache[MAX_FONT_ENTRIES];
static int g_fontMenuCacheCount = 0;
static BOOL g_fontMenuCacheReady = FALSE;
static BOOL g_fontMenuCacheFailed = FALSE;
static SRWLOCK g_fontMenuCacheLock = SRWLOCK_INIT;
static SRWLOCK g_fontScanThreadLock = SRWLOCK_INIT;
static HANDLE g_hFontScanThread = NULL;
static HANDLE g_hRetiredFontScanThread = NULL;
static volatile LONG g_fontScanShuttingDown = 0;
static volatile LONG g_fontScanGeneration = 0;
static volatile LONG g_fontMenuLastScanTick = 0;

static BOOL IsFontMenuScanShuttingDown(void) {
    return InterlockedCompareExchange(&g_fontScanShuttingDown, 0, 0) != 0;
}

static BOOL IsFontMenuScanCanceled(LONG generation) {
    return IsFontMenuScanShuttingDown() ||
           InterlockedCompareExchange(&g_fontScanGeneration, 0, 0) != generation;
}

static BOOL IsFontMenuCacheRecentlyScanned(DWORD now) {
    DWORD lastScanTick = (DWORD)InterlockedCompareExchange(&g_fontMenuLastScanTick, 0, 0);
    if (lastScanTick == 0 ||
        (DWORD)(now - lastScanTick) >= FONT_MENU_SCAN_REFRESH_COOLDOWN_MS) {
        return FALSE;
    }

    AcquireSRWLockShared(&g_fontMenuCacheLock);
    BOOL recentlyScanned = g_fontMenuCacheReady || g_fontMenuCacheFailed;
    ReleaseSRWLockShared(&g_fontMenuCacheLock);
    return recentlyScanned;
}

static void MarkFontMenuScanStartFailure(DWORD now) {
    AcquireSRWLockExclusive(&g_fontMenuCacheLock);
    ZeroMemory(g_fontMenuCache, sizeof(g_fontMenuCache));
    g_fontMenuCacheCount = 0;
    g_fontMenuCacheReady = FALSE;
    g_fontMenuCacheFailed = TRUE;
    InterlockedExchange(&g_fontMenuLastScanTick, (LONG)now);
    ReleaseSRWLockExclusive(&g_fontMenuCacheLock);
}

static BOOL CleanupRetiredFontScanThreadLocked(DWORD waitMs) {
    if (!g_hRetiredFontScanThread) {
        return TRUE;
    }

    DWORD wait = WaitForSingleObject(g_hRetiredFontScanThread, waitMs);
    if (wait != WAIT_OBJECT_0) {
        if (wait == WAIT_FAILED) {
            LOG_WARNING("Retired font menu scan wait failed: %lu", GetLastError());
        }
        return FALSE;
    }

    CloseHandle(g_hRetiredFontScanThread);
    g_hRetiredFontScanThread = NULL;
    return TRUE;
}

static BOOL CopyStringExactW(const wchar_t* src, wchar_t* out, size_t outSize) {
    if (!out || outSize == 0) return FALSE;
    out[0] = L'\0';
    if (!src) return FALSE;

    size_t srcLen = wcslen(src);
    if (srcLen >= outSize) return FALSE;

    memcpy(out, src, (srcLen + 1) * sizeof(wchar_t));
    return TRUE;
}

static void ResetFontMenuIdMap(void) {
    ZeroMemory(g_fontMenuIdMap, sizeof(g_fontMenuIdMap));
    g_fontMenuIdMapCount = 0;
}

static BOOL RememberFontMenuId(UINT id, const wchar_t* relativePath) {
    if (!relativePath || g_fontMenuIdMapCount >= MAX_FONT_ENTRIES) return FALSE;

    FontMenuIdMapEntry entry = {0};
    entry.id = id;
    if (!CopyStringExactW(relativePath, entry.relativePath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_WARNING, "Font menu path is too long: %ls", relativePath);
        return FALSE;
    }

    g_fontMenuIdMap[g_fontMenuIdMapCount] = entry;
    g_fontMenuIdMapCount++;
    return TRUE;
}

static void ForgetLastFontMenuId(UINT id) {
    if (g_fontMenuIdMapCount <= 0) return;
    if (g_fontMenuIdMap[g_fontMenuIdMapCount - 1].id != id) return;

    g_fontMenuIdMapCount--;
    ZeroMemory(&g_fontMenuIdMap[g_fontMenuIdMapCount], sizeof(g_fontMenuIdMap[0]));
}

/* ============================================================================
 * Path Helpers
 * ============================================================================ */

/**
 * @brief Get fonts folder path
 */
static BOOL GetFontsFolderPath(wchar_t* outPath, size_t size) {
    return GetFontsFolderW(outPath, size, FALSE);
}

/**
 * @brief Get current font relative path for matching
 */
static void GetCurrentFontRelativePath(wchar_t* outPath, size_t size) {
    if (!outPath || size == 0 || size > INT_MAX) return;
    outPath[0] = L'\0';

    const char* prefix = FONTS_PATH_PREFIX;
    size_t prefixLen = strlen(prefix);
    const char* source = NULL;

    if (_strnicmp(FONT_FILE_NAME, prefix, prefixLen) == 0) {
        /* Custom font - extract relative path */
        source = FONT_FILE_NAME + prefixLen;
    } else if (strchr(FONT_FILE_NAME, ':') == NULL &&
               (strchr(FONT_FILE_NAME, '\\') != NULL || strchr(FONT_FILE_NAME, '/') != NULL)) {
        /* Relative path without prefix */
        source = FONT_FILE_NAME;
    }

    if (source && MultiByteToWideChar(CP_UTF8, 0, source, -1, outPath, (int)size) <= 0) {
        outPath[0] = L'\0';
    }
    /* System fonts or just filename - leave empty */
}

static const wchar_t* GetPathBaseNameW(const wchar_t* path) {
    if (!path) return L"";

    const wchar_t* slash = wcsrchr(path, L'\\');
    const wchar_t* forwardSlash = wcsrchr(path, L'/');
    if (!slash || (forwardSlash && forwardSlash > slash)) {
        slash = forwardSlash;
    }

    return slash ? slash + 1 : path;
}

/* ============================================================================
 * Font Entry Management
 * ============================================================================ */

/**
 * @brief Add font entry to context
 */
static BOOL AddFontEntry(FontScanContext* ctx, const wchar_t* fileName,
                         const wchar_t* relativePath) {
    if (ctx->count >= ctx->capacity) {
        if (!ctx->full) {
            WriteLog(LOG_LEVEL_WARNING, "Font list capacity reached (%d), stopping scan",
                     ctx->capacity);
        }
        ctx->full = TRUE;
        return FALSE;
    }

    FontEntry* entry = &ctx->entries[ctx->count];
    wcsncpy(entry->fileName, fileName, MAX_FONT_NAME_LENGTH - 1);
    entry->fileName[MAX_FONT_NAME_LENGTH - 1] = L'\0';

    wcsncpy(entry->relativePath, relativePath, MAX_PATH - 1);
    entry->relativePath[MAX_PATH - 1] = L'\0';

    /* Display name = filename without extension */
    wcsncpy(entry->displayName, fileName, MAX_FONT_NAME_LENGTH - 1);
    entry->displayName[MAX_FONT_NAME_LENGTH - 1] = L'\0';
    wchar_t* dotPos = wcsrchr(entry->displayName, L'.');
    if (dotPos) *dotPos = L'\0';

    ctx->count++;
    return TRUE;
}

/* ============================================================================
 * Folder Scanning
 * ============================================================================ */

/**
 * @brief Recursively scan font folder
 */
static void ScanFontFolderRecursive(const wchar_t* folderPath,
                                    const wchar_t* relativePath,
                                    FontScanContext* ctx,
                                    int depth,
                                    LONG generation) {
    if (ctx->full || IsFontMenuScanCanceled(generation)) return;

    if (depth >= MAX_RECURSION_DEPTH) {
        WriteLog(LOG_LEVEL_WARNING, "Max recursion depth reached at: %ls", folderPath);
        return;
    }

    wchar_t searchPath[MAX_PATH];
    int written = _snwprintf_s(searchPath, MAX_PATH, _TRUNCATE, L"%s\\*", folderPath);
    if (written < 0 || written >= MAX_PATH) {
        WriteLog(LOG_LEVEL_WARNING, "Font scan path is too long: %ls", folderPath);
        ctx->failed = TRUE;
        return;
    }

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND &&
            error != ERROR_PATH_NOT_FOUND &&
            error != ERROR_NO_MORE_FILES) {
            WriteLog(LOG_LEVEL_WARNING, "Font folder scan failed: %ls (error=%lu)",
                     folderPath, error);
            ctx->failed = TRUE;
        }
        return;
    }

    BOOL stoppedEarly = FALSE;
    do {
        if (ctx->full || IsFontMenuScanCanceled(generation)) {
            stoppedEarly = TRUE;
            break;
        }

        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        if (ctx->scannedEntries >= MAX_FONT_SCAN_ENTRIES) {
            if (!ctx->truncated) {
                WriteLog(LOG_LEVEL_WARNING, "Font scan entry limit reached (%d)",
                         MAX_FONT_SCAN_ENTRIES);
            }
            ctx->truncated = TRUE;
            ctx->full = TRUE;
            stoppedEarly = TRUE;
            break;
        }
        ctx->scannedEntries++;

        wchar_t fullPath[MAX_PATH];
        int len1 = _snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPath, findData.cFileName);
        if (len1 < 0 || len1 >= MAX_PATH) continue;

        wchar_t newRelativePath[MAX_PATH];
        if (!relativePath || relativePath[0] == L'\0') {
            wcsncpy(newRelativePath, findData.cFileName, MAX_PATH - 1);
            newRelativePath[MAX_PATH - 1] = L'\0';
        } else {
            int len2 = _snwprintf_s(newRelativePath, MAX_PATH, _TRUNCATE, L"%s\\%s", relativePath, findData.cFileName);
            if (len2 < 0 || len2 >= MAX_PATH) continue;
        }

        BOOL isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDirectory) {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                ScanFontFolderRecursive(fullPath, newRelativePath, ctx, depth + 1, generation);
                if (ctx->failed) {
                    stoppedEarly = TRUE;
                    break;
                }
            }
        } else {
            const wchar_t* ext = wcsrchr(findData.cFileName, L'.');
            if (ext && (_wcsicmp(ext, L".ttf") == 0 || _wcsicmp(ext, L".otf") == 0)) {
                if (!AddFontEntry(ctx, findData.cFileName, newRelativePath)) {
                    stoppedEarly = TRUE;
                    break;
                }
            }
        }
    } while (!ctx->full && FindNextFileW(hFind, &findData));

    DWORD findError = stoppedEarly ? ERROR_SUCCESS : GetLastError();
    FindClose(hFind);
    if (!stoppedEarly && findError != ERROR_NO_MORE_FILES) {
        WriteLog(LOG_LEVEL_WARNING, "Font folder enumeration failed: %ls (error=%lu)",
                 folderPath, findError);
        ctx->failed = TRUE;
    }
}

/**
 * @brief Scan fonts folder and return entries
 */
static int ScanFontsFolder(FontEntry* entries, int capacity, LONG generation) {
    FontScanContext ctx = {0};
    ctx.entries = entries;
    ctx.count = 0;
    ctx.capacity = capacity;

    wchar_t fontsPath[MAX_PATH];
    if (!GetFontsFolderPath(fontsPath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_WARNING, "Failed to get fonts folder path");
        return FONT_MENU_SCAN_FAILED;
    }

    DWORD attribs = GetFileAttributesW(fontsPath);
    if (attribs == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return 0;
        }
        WriteLog(LOG_LEVEL_WARNING, "Failed to stat fonts folder: %ls (error=%lu)",
                 fontsPath, error);
        return FONT_MENU_SCAN_FAILED;
    }
    if (!(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        WriteLog(LOG_LEVEL_WARNING, "Fonts path is not a folder: %ls", fontsPath);
        return FONT_MENU_SCAN_FAILED;
    }

    ScanFontFolderRecursive(fontsPath, L"", &ctx, 0, generation);
    if (ctx.failed || IsFontMenuScanCanceled(generation)) {
        return FONT_MENU_SCAN_FAILED;
    }

    WriteLog(LOG_LEVEL_INFO, "Font scan complete: %d fonts found%s",
             ctx.count, ctx.truncated ? " (truncated)" : "");
    return ctx.count;
}

static DWORD WINAPI FontScanThread(LPVOID lpParam) {
    LONG generation = (LONG)(INT_PTR)lpParam;

    FontEntry* entries = (FontEntry*)malloc((size_t)MAX_FONT_ENTRIES * sizeof(*entries));
    int count = FONT_MENU_SCAN_FAILED;
    if (entries) {
        ZeroMemory(entries, (size_t)MAX_FONT_ENTRIES * sizeof(*entries));
        count = ScanFontsFolder(entries, MAX_FONT_ENTRIES, generation);
    } else {
        LOG_WARNING("Failed to allocate font menu scan buffer");
    }
    BOOL scanFailed = (count < 0);

    if (!IsFontMenuScanCanceled(generation)) {
        AcquireSRWLockExclusive(&g_fontMenuCacheLock);
        if (!IsFontMenuScanCanceled(generation)) {
            if (!scanFailed && count > 0 && entries) {
                memcpy(g_fontMenuCache, entries, (size_t)count * sizeof(FontEntry));
            }
            if (scanFailed) {
                count = 0;
            }
            if (count < MAX_FONT_ENTRIES) {
                ZeroMemory(&g_fontMenuCache[count],
                           (size_t)(MAX_FONT_ENTRIES - count) * sizeof(FontEntry));
            }
            g_fontMenuCacheCount = count;
            g_fontMenuCacheReady = !scanFailed;
            g_fontMenuCacheFailed = scanFailed;
            InterlockedExchange(&g_fontMenuLastScanTick, (LONG)GetTickCount());
        }
        ReleaseSRWLockExclusive(&g_fontMenuCacheLock);
    }

    free(entries);
    return 0;
}

void FontMenu_RequestScanAsync(void) {
    AcquireSRWLockExclusive(&g_fontScanThreadLock);

    if (!CleanupRetiredFontScanThreadLocked(0)) {
        ReleaseSRWLockExclusive(&g_fontScanThreadLock);
        return;
    }

    if (!g_hRetiredFontScanThread &&
        InterlockedCompareExchange(&g_fontScanShuttingDown, 0, 0) != 0) {
        InterlockedExchange(&g_fontScanShuttingDown, 0);
    }

    if (IsFontMenuScanShuttingDown()) {
        ReleaseSRWLockExclusive(&g_fontScanThreadLock);
        return;
    }

    if (g_hFontScanThread) {
        DWORD wait = WaitForSingleObject(g_hFontScanThread, 0);
        if (wait == WAIT_OBJECT_0) {
            CloseHandle(g_hFontScanThread);
            g_hFontScanThread = NULL;
        } else {
            ReleaseSRWLockExclusive(&g_fontScanThreadLock);
            return;
        }
    }

    DWORD now = GetTickCount();
    if (IsFontMenuCacheRecentlyScanned(now)) {
        ReleaseSRWLockExclusive(&g_fontScanThreadLock);
        return;
    }

    LONG generation = InterlockedCompareExchange(&g_fontScanGeneration, 0, 0);
    HANDLE hThread = CreateThread(NULL, 0, FontScanThread,
                                  (LPVOID)(INT_PTR)generation, 0, NULL);
    if (hThread) {
        g_hFontScanThread = hThread;
    } else {
        LOG_WARNING("Failed to start font menu scan thread");
        MarkFontMenuScanStartFailure(now);
    }

    ReleaseSRWLockExclusive(&g_fontScanThreadLock);
}

void FontMenu_Initialize(void) {
    AcquireSRWLockExclusive(&g_fontScanThreadLock);
    if (!CleanupRetiredFontScanThreadLocked(ASYNC_FONT_SCAN_STOP_TIMEOUT_MS)) {
        ReleaseSRWLockExclusive(&g_fontScanThreadLock);
        return;
    }

    if (g_hFontScanThread) {
        DWORD wait = WaitForSingleObject(g_hFontScanThread, 0);
        if (wait == WAIT_OBJECT_0) {
            CloseHandle(g_hFontScanThread);
            g_hFontScanThread = NULL;
        }
    }
    ReleaseSRWLockExclusive(&g_fontScanThreadLock);

    InterlockedIncrement(&g_fontScanGeneration);
    InterlockedExchange(&g_fontScanShuttingDown, 0);
}

void FontMenu_Shutdown(void) {
    HANDLE hThread = NULL;

    AcquireSRWLockExclusive(&g_fontScanThreadLock);
    InterlockedIncrement(&g_fontScanGeneration);
    InterlockedExchange(&g_fontScanShuttingDown, 1);
    hThread = g_hFontScanThread;
    ReleaseSRWLockExclusive(&g_fontScanThreadLock);

    if (hThread) {
        DWORD wait = WaitForSingleObject(hThread, ASYNC_FONT_SCAN_STOP_TIMEOUT_MS);
        if (wait != WAIT_OBJECT_0) {
            LOG_WARNING("Font menu scan stop timed out after %lu ms (wait=%lu, error=%lu)",
                        (DWORD)ASYNC_FONT_SCAN_STOP_TIMEOUT_MS,
                        wait,
                        GetLastError());
            if (wait == WAIT_TIMEOUT) {
                AcquireSRWLockExclusive(&g_fontScanThreadLock);
                if (g_hFontScanThread == hThread) {
                    g_hFontScanThread = NULL;
                    if (CleanupRetiredFontScanThreadLocked(0)) {
                        g_hRetiredFontScanThread = hThread;
                    } else {
                        CloseHandle(hThread);
                    }
                }
                ReleaseSRWLockExclusive(&g_fontScanThreadLock);
            }
        } else {
            AcquireSRWLockExclusive(&g_fontScanThreadLock);
            if (g_hFontScanThread == hThread) {
                CloseHandle(g_hFontScanThread);
                g_hFontScanThread = NULL;
            } else {
                CloseHandle(hThread);
            }
            ReleaseSRWLockExclusive(&g_fontScanThreadLock);
        }
    }

    AcquireSRWLockExclusive(&g_fontMenuCacheLock);
    ZeroMemory(g_fontMenuCache, sizeof(g_fontMenuCache));
    g_fontMenuCacheCount = 0;
    g_fontMenuCacheReady = FALSE;
    g_fontMenuCacheFailed = FALSE;
    ReleaseSRWLockExclusive(&g_fontMenuCacheLock);
    InterlockedExchange(&g_fontMenuLastScanTick, 0);

    ResetFontMenuIdMap();
}

/* ============================================================================
 * Menu Building
 * ============================================================================ */

/**
 * @brief Comparator for font entries (by relative path)
 */
typedef struct {
    const FontEntry* entry;
} FontEntrySortItem;

static int CompareFontEntries(const void* a, const void* b) {
    const FontEntrySortItem* ea = a;
    const FontEntrySortItem* eb = b;
    return NaturalPathCompareW(ea->entry->relativePath, eb->entry->relativePath);
}

static BOOL IsFontEntryCurrent(const FontEntry* entry, const wchar_t* currentFontRelPath) {
    return entry &&
           currentFontRelPath &&
           currentFontRelPath[0] != L'\0' &&
           _wcsicmp(entry->relativePath, currentFontRelPath) == 0;
}

/**
 * @brief Find or create a submenu with the given name
 */
static HMENU EnsureSubMenu(HMENU hParent, const wchar_t* name, BOOL shouldCheck) {
    int count = GetMenuItemCount(hParent);
    MENUITEMINFOW mii = {0};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING | MIIM_SUBMENU | MIIM_STATE;
    wchar_t buffer[MAX_PATH] = {0};

    for (int i = 0; i < count; i++) {
        mii.dwTypeData = buffer;
        mii.cch = MAX_PATH;
        if (GetMenuItemInfoW(hParent, i, TRUE, &mii)) {
            if (mii.hSubMenu && _wcsicmp(buffer, name) == 0) {
                if (shouldCheck && !(mii.fState & MFS_CHECKED)) {
                    mii.fState |= MFS_CHECKED;
                    SetMenuItemInfoW(hParent, i, TRUE, &mii);
                }
                return mii.hSubMenu;
            }
        }
    }

    HMENU hSub = CreatePopupMenu();
    if (!hSub) return NULL;

    UINT flags = MF_POPUP;
    if (shouldCheck) flags |= MF_CHECKED;
    if (!AppendMenuW(hParent, flags, (UINT_PTR)hSub, name)) {
        DestroyMenu(hSub);
        return NULL;
    }
    return hSub;
}

/**
 * @brief Build font menu from scanned entries
 */
static void BuildFontMenuFromEntries(HMENU hRootMenu, const FontEntry* entries, int count,
                                     const wchar_t* currentFontRelPath, int* fontId) {
    if (!entries || count <= 0 || count > MAX_FONT_ENTRIES) return;

    /* Sort entries for consistent display */
    FontEntrySortItem sortedEntries[MAX_FONT_ENTRIES];

    for (int i = 0; i < count; i++) {
        sortedEntries[i].entry = &entries[i];
    }

    qsort(sortedEntries, count, sizeof(sortedEntries[0]), CompareFontEntries);

    /* Collect parent directories of selected fonts */
    enum { MAX_PARENT_DIRS = 100 };
    wchar_t (*parentDirs)[MAX_PATH] = NULL;
    int parentDirCount = 0;

    for (int i = 0; i < count; i++) {
        if (IsFontEntryCurrent(sortedEntries[i].entry, currentFontRelPath)) {
            if (!parentDirs) {
                parentDirs = (wchar_t (*)[MAX_PATH])calloc(MAX_PARENT_DIRS,
                                                           sizeof(*parentDirs));
                if (!parentDirs) {
                    LOG_WARNING("Failed to allocate font menu parent directory buffer");
                    break;
                }
            }

            wchar_t pathCopy[MAX_PATH];
            wcsncpy(pathCopy, sortedEntries[i].entry->relativePath, MAX_PATH - 1);
            pathCopy[MAX_PATH - 1] = L'\0';

            wchar_t currentPath[MAX_PATH] = L"";
            wchar_t* context = NULL;
            wchar_t* token = wcstok_s(pathCopy, L"\\/", &context);

            while (token) {
                wchar_t* nextToken = wcstok_s(NULL, L"\\/", &context);
                if (nextToken) {
                    size_t currentLen = wcslen(currentPath);
                    size_t tokenLen = wcslen(token);

                    if (currentLen + tokenLen + 2 < MAX_PATH) {
                        if (currentLen > 0) wcsncat_s(currentPath, MAX_PATH, L"\\", 1);
                        wcsncat_s(currentPath, MAX_PATH, token, tokenLen);
                    }

                    BOOL found = FALSE;
                    for (int j = 0; j < parentDirCount; j++) {
                        if (_wcsicmp(parentDirs[j], currentPath) == 0) {
                            found = TRUE;
                            break;
                        }
                    }
                    if (!found && parentDirCount < MAX_PARENT_DIRS) {
                        wcsncpy(parentDirs[parentDirCount], currentPath, MAX_PATH - 1);
                        parentDirs[parentDirCount][MAX_PATH - 1] = L'\0';
                        parentDirCount++;
                    }
                }
                token = nextToken;
            }
        }
    }

    /* Build menu tree */
    for (int i = 0; i < count; i++) {
        const FontEntry* entry = sortedEntries[i].entry;
        BOOL isCurrentFont = IsFontEntryCurrent(entry, currentFontRelPath);

        wchar_t pathCopy[MAX_PATH];
        wcsncpy(pathCopy, entry->relativePath, MAX_PATH - 1);
        pathCopy[MAX_PATH - 1] = L'\0';

        wchar_t* context = NULL;
        wchar_t* token = wcstok_s(pathCopy, L"\\/", &context);

        HMENU hCurrent = hRootMenu;
        wchar_t currentPath[MAX_PATH] = L"";

        while (token) {
            wchar_t* nextToken = wcstok_s(NULL, L"\\/", &context);

            if (nextToken) {
                size_t currentLen = wcslen(currentPath);
                size_t tokenLen = wcslen(token);

                if (currentLen + tokenLen + 2 < MAX_PATH) {
                    if (currentLen > 0) wcsncat_s(currentPath, MAX_PATH, L"\\", 1);
                    wcsncat_s(currentPath, MAX_PATH, token, tokenLen);
                }

                BOOL shouldCheck = FALSE;
                if (parentDirs) {
                    for (int j = 0; j < parentDirCount; j++) {
                        if (_wcsicmp(parentDirs[j], currentPath) == 0) {
                            shouldCheck = TRUE;
                            break;
                        }
                    }
                }

                hCurrent = EnsureSubMenu(hCurrent, token, shouldCheck);
                if (!hCurrent) break;
            } else {
                UINT flags = MF_STRING;
                if (isCurrentFont) flags |= MF_CHECKED;
                UINT id = (UINT)(*fontId);
                if (hCurrent && RememberFontMenuId(id, entry->relativePath)) {
                    if (AppendMenuW(hCurrent, flags, id, entry->displayName)) {
                        (*fontId)++;
                    } else {
                        ForgetLastFontMenuId(id);
                    }
                }
            }

            token = nextToken;
        }
    }

    free(parentDirs);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Build font submenu with cached folder scan
 */
void BuildFontSubmenu(HMENU hMenu) {
    HMENU hFontSubMenu = CreatePopupMenu();
    if (!hFontSubMenu) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to create font submenu");
        return;
    }
    ResetFontMenuIdMap();

    int g_advancedFontId = CMD_FONT_SELECTION_BASE;

    if (NeedsFontLicenseVersionAcceptance()) {
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_LICENSE_AGREE,
                   GetLocalizedString(NULL, L"Click to agree to license agreement"));
    } else {
        FontMenu_RequestScanAsync();

        /* Get current font relative path */
        wchar_t currentFontRelPath[MAX_PATH] = L"";
        GetCurrentFontRelativePath(currentFontRelPath, MAX_PATH);

        BOOL cacheReady = FALSE;
        int fontCount = 0;
        BOOL isSystemFont = TRUE;
        FontEntry* fontSnapshot =
            (FontEntry*)malloc((size_t)MAX_FONT_ENTRIES * sizeof(*fontSnapshot));
        if (!fontSnapshot) {
            LOG_WARNING("Failed to allocate font menu cache snapshot");
        }

        AcquireSRWLockShared(&g_fontMenuCacheLock);
        cacheReady = g_fontMenuCacheReady || g_fontMenuCacheFailed;
        fontCount = g_fontMenuCacheCount;
        if (fontCount > MAX_FONT_ENTRIES) {
            fontCount = MAX_FONT_ENTRIES;
        }
        if (fontCount > 0 && fontSnapshot) {
            memcpy(fontSnapshot, g_fontMenuCache, (size_t)fontCount * sizeof(*fontSnapshot));
        } else if (fontCount > 0) {
            fontCount = 0;
            cacheReady = FALSE;
        }
        ReleaseSRWLockShared(&g_fontMenuCacheLock);

        if (fontCount == 0) {
            AppendMenuW(hFontSubMenu, MF_STRING | MF_GRAYED, 0,
                        cacheReady
                            ? GetLocalizedString(NULL, L"No font files found")
                            : GetLocalizedString(NULL, L"Loading..."));
            AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
        } else {
            BuildFontMenuFromEntries(hFontSubMenu, fontSnapshot, fontCount,
                                     currentFontRelPath, &g_advancedFontId);
            AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
        }

        /* Determine if current font is a system font */
        const char* prefix = FONTS_PATH_PREFIX;
        size_t prefixLen = strlen(prefix);

        if (_strnicmp(FONT_FILE_NAME, prefix, prefixLen) == 0) {
            isSystemFont = FALSE;
        } else if (strchr(FONT_FILE_NAME, ':') != NULL) {
            isSystemFont = (strstr(FONT_FILE_NAME, "Windows\\Fonts") != NULL ||
                           strstr(FONT_FILE_NAME, "WINDOWS\\Fonts") != NULL);
        } else if (strchr(FONT_FILE_NAME, '\\') != NULL || strchr(FONT_FILE_NAME, '/') != NULL) {
            isSystemFont = FALSE;
        } else {
            /* Just filename - check if in scanned fonts */
            wchar_t wFontName[MAX_PATH] = L"";
            if (MultiByteToWideChar(CP_UTF8, 0, FONT_FILE_NAME, -1, wFontName, MAX_PATH) > 0 &&
                wFontName[0] != L'\0') {
                for (int i = 0; i < fontCount; i++) {
                    if (_wcsicmp(GetPathBaseNameW(fontSnapshot[i].relativePath), wFontName) == 0) {
                        isSystemFont = FALSE;
                        break;
                    }
                }
            }
        }
        free(fontSnapshot);

        UINT systemFontFlags = MF_STRING;
        if (isSystemFont) systemFontFlags |= MF_CHECKED;

        AppendMenuW(hFontSubMenu, systemFontFlags, CLOCK_IDM_SYSTEM_FONT_PICKER,
                   GetLocalizedString(NULL, L"System Fonts..."));

        AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_ADVANCED,
                   GetLocalizedString(NULL, L"Open fonts folder"));
    }

    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu,
                     GetLocalizedString(NULL, L"Font"))) {
        DestroyMenu(hFontSubMenu);
        WriteLog(LOG_LEVEL_ERROR, "Failed to attach font submenu");
    }
}

/**
 * @brief Get font path from the current menu ID map
 */
BOOL GetFontPathFromMenuId(UINT id, char* outPath, size_t outPathSize) {
    if (!outPath || outPathSize == 0) return FALSE;
    outPath[0] = '\0';
    if (outPathSize > INT_MAX) return FALSE;
    if (id < CMD_FONT_SELECTION_BASE) return FALSE;

    for (int i = 0; i < g_fontMenuIdMapCount; i++) {
        if (g_fontMenuIdMap[i].id == id) {
            if (WideCharToMultiByte(CP_UTF8, 0, g_fontMenuIdMap[i].relativePath, -1,
                                    outPath, (int)outPathSize, NULL, NULL) <= 0) {
                outPath[0] = '\0';
                return FALSE;
            }
            return TRUE;
        }
    }

    return FALSE;
}
