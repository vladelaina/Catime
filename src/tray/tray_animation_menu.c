/**
 * @file tray_animation_menu.c
 * @brief Animation menu implementation (async scan cache)
 */

#include "tray/tray_animation_menu.h"
#include "tray/tray_animation_loader.h"
#include "utils/natural_sort.h"
#include "config.h"
#include "log.h"
#include "language.h"
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_ANIM_ENTRIES 200
#define MAX_RECURSION_DEPTH 10
#define MAX_ANIM_NAME_LENGTH 260
#define MAX_ANIM_SCAN_ENTRIES 4096
#define ASYNC_ANIM_SCAN_STOP_TIMEOUT_MS 2000
#define ANIMATION_MENU_SCAN_REFRESH_COOLDOWN_MS 2000
#define ANIMATION_MENU_SCAN_FAILED (-1)

/* ============================================================================
 * Forward declarations
 * ============================================================================ */

BOOL SetCurrentAnimationName(const char* name);

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/**
 * @brief Single animation entry for menu building
 */
typedef struct {
    char fileName[MAX_ANIM_NAME_LENGTH];
    char relativePath[MAX_PATH];
    BOOL isSpecial;
} AnimEntry;

/**
 * @brief Animation scan context
 */
typedef struct {
    AnimEntry* entries;
    int count;
    int capacity;
    int scannedEntries;
    BOOL truncated;
    BOOL full;
    BOOL failed;
} AnimScanContext;

typedef struct {
    UINT id;
    char relativePath[MAX_PATH];
} AnimMenuIdMapEntry;

static AnimMenuIdMapEntry g_animMenuIdMap[MAX_ANIM_ENTRIES];
static int g_animMenuIdMapCount = 0;
static AnimEntry g_animMenuCache[MAX_ANIM_ENTRIES];
static int g_animMenuCacheCount = 0;
static BOOL g_animMenuCacheReady = FALSE;
static BOOL g_animMenuCacheFailed = FALSE;
static SRWLOCK g_animMenuCacheLock = SRWLOCK_INIT;
static SRWLOCK g_animScanThreadLock = SRWLOCK_INIT;
static HANDLE g_hAnimScanThread = NULL;
static volatile LONG g_animScanShuttingDown = 0;
static volatile LONG g_animScanGeneration = 0;
static volatile LONG g_animMenuLastScanTick = 0;

static BOOL IsAnimationMenuScanShuttingDown(void) {
    return InterlockedCompareExchange(&g_animScanShuttingDown, 0, 0) != 0;
}

static BOOL IsAnimationMenuScanCanceled(LONG generation) {
    return IsAnimationMenuScanShuttingDown() ||
           InterlockedCompareExchange(&g_animScanGeneration, 0, 0) != generation;
}

static BOOL IsAnimationMenuCacheRecentlyScanned(DWORD now) {
    DWORD lastScanTick = (DWORD)InterlockedCompareExchange(&g_animMenuLastScanTick, 0, 0);
    if (lastScanTick == 0 ||
        (DWORD)(now - lastScanTick) >= ANIMATION_MENU_SCAN_REFRESH_COOLDOWN_MS) {
        return FALSE;
    }

    AcquireSRWLockShared(&g_animMenuCacheLock);
    BOOL recentlyScanned = g_animMenuCacheReady || g_animMenuCacheFailed;
    ReleaseSRWLockShared(&g_animMenuCacheLock);
    return recentlyScanned;
}

static void MarkAnimationMenuScanStartFailure(DWORD now) {
    AcquireSRWLockExclusive(&g_animMenuCacheLock);
    ZeroMemory(g_animMenuCache, sizeof(g_animMenuCache));
    g_animMenuCacheCount = 0;
    g_animMenuCacheReady = FALSE;
    g_animMenuCacheFailed = TRUE;
    InterlockedExchange(&g_animMenuLastScanTick, (LONG)now);
    ReleaseSRWLockExclusive(&g_animMenuCacheLock);
}

static BOOL CopyStringExactA(const char* src, char* out, size_t outSize) {
    if (!out || outSize == 0) return FALSE;
    out[0] = '\0';
    if (!src) return FALSE;

    size_t srcLen = strlen(src);
    if (srcLen >= outSize) return FALSE;

    memcpy(out, src, srcLen + 1);
    return TRUE;
}

static void ResetAnimationMenuIdMap(void) {
    ZeroMemory(g_animMenuIdMap, sizeof(g_animMenuIdMap));
    g_animMenuIdMapCount = 0;
}

static BOOL RememberAnimationMenuId(UINT id, const char* relativePath) {
    if (!relativePath || g_animMenuIdMapCount >= MAX_ANIM_ENTRIES) return FALSE;

    AnimMenuIdMapEntry entry = {0};
    entry.id = id;
    if (!CopyStringExactA(relativePath, entry.relativePath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_WARNING, "Animation menu path is too long: %s", relativePath);
        return FALSE;
    }

    g_animMenuIdMap[g_animMenuIdMapCount] = entry;
    g_animMenuIdMapCount++;
    return TRUE;
}

static void ForgetLastAnimationMenuId(UINT id) {
    if (g_animMenuIdMapCount <= 0) return;
    if (g_animMenuIdMap[g_animMenuIdMapCount - 1].id != id) return;

    g_animMenuIdMapCount--;
    ZeroMemory(&g_animMenuIdMap[g_animMenuIdMapCount], sizeof(g_animMenuIdMap[0]));
}

static const char* FindAnimationMenuPath(UINT id) {
    for (int i = 0; i < g_animMenuIdMapCount; i++) {
        if (g_animMenuIdMap[i].id == id) {
            return g_animMenuIdMap[i].relativePath;
        }
    }

    return NULL;
}

/* ============================================================================
 * Path Helpers
 * ============================================================================ */

/**
 * @brief Get animations folder path (wide char)
 */
static BOOL GetAnimationsFolderPathW(wchar_t* outPath, size_t size) {
    if (!outPath || size == 0 || size > INT_MAX) return FALSE;

    char utf8Path[MAX_PATH];
    GetAnimationsFolderPath(utf8Path, MAX_PATH);

    if (utf8Path[0] == '\0') return FALSE;

    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, outPath, (int)size) <= 0) {
        outPath[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

/* ============================================================================
 * Animation Entry Management
 * ============================================================================ */

/**
 * @brief Check if file is an animation file
 */
static BOOL IsAnimationFile(const wchar_t* fileName) {
    const wchar_t* ext = wcsrchr(fileName, L'.');
    if (!ext) return FALSE;

    return _wcsicmp(ext, L".gif") == 0 ||
           _wcsicmp(ext, L".webp") == 0 ||
           _wcsicmp(ext, L".ico") == 0 ||
           _wcsicmp(ext, L".png") == 0 ||
           _wcsicmp(ext, L".jpg") == 0 ||
           _wcsicmp(ext, L".jpeg") == 0 ||
           _wcsicmp(ext, L".bmp") == 0 ||
           _wcsicmp(ext, L".tif") == 0 ||
           _wcsicmp(ext, L".tiff") == 0;
}

/**
 * @brief Add animation entry to context
 */
static BOOL AddAnimEntry(AnimScanContext* ctx, const char* fileName,
                         const char* relativePath, BOOL isSpecial) {
    if (ctx->count >= ctx->capacity) {
        if (!ctx->full) {
            WriteLog(LOG_LEVEL_WARNING, "Animation list capacity reached (%d), stopping scan",
                     ctx->capacity);
        }
        ctx->full = TRUE;
        return FALSE;
    }

    AnimEntry* entry = &ctx->entries[ctx->count];
    strncpy(entry->fileName, fileName, MAX_ANIM_NAME_LENGTH - 1);
    entry->fileName[MAX_ANIM_NAME_LENGTH - 1] = '\0';

    strncpy(entry->relativePath, relativePath, MAX_PATH - 1);
    entry->relativePath[MAX_PATH - 1] = '\0';

    entry->isSpecial = isSpecial;

    ctx->count++;
    return TRUE;
}

/* ============================================================================
 * Folder Scanning
 * ============================================================================ */

/**
 * @brief Recursively scan animations folder
 */
static void ScanAnimationFolderRecursive(const wchar_t* folderPath,
                                          const char* relativePathUtf8,
                                          AnimScanContext* ctx,
                                          int depth,
                                          LONG generation) {
    if (ctx->full || IsAnimationMenuScanCanceled(generation)) return;

    if (depth >= MAX_RECURSION_DEPTH) {
        WriteLog(LOG_LEVEL_WARNING, "Max recursion depth reached at: %ls", folderPath);
        return;
    }

    wchar_t searchPath[MAX_PATH];
    int written = _snwprintf_s(searchPath, MAX_PATH, _TRUNCATE, L"%s\\*", folderPath);
    if (written < 0 || written >= MAX_PATH) {
        WriteLog(LOG_LEVEL_WARNING, "Animation scan path is too long: %ls", folderPath);
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
            WriteLog(LOG_LEVEL_WARNING, "Animation folder scan failed: %ls (error=%lu)",
                     folderPath, error);
            ctx->failed = TRUE;
        }
        return;
    }

    BOOL stoppedEarly = FALSE;
    do {
        if (ctx->full || IsAnimationMenuScanCanceled(generation)) {
            stoppedEarly = TRUE;
            break;
        }

        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        if (ctx->scannedEntries >= MAX_ANIM_SCAN_ENTRIES) {
            if (!ctx->truncated) {
                WriteLog(LOG_LEVEL_WARNING, "Animation scan entry limit reached (%d)",
                         MAX_ANIM_SCAN_ENTRIES);
            }
            ctx->truncated = TRUE;
            ctx->full = TRUE;
            stoppedEarly = TRUE;
            break;
        }
        ctx->scannedEntries++;

        BOOL isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (!isDirectory && !IsAnimationFile(findData.cFileName)) {
            continue;
        }

        char fileNameUtf8[MAX_ANIM_NAME_LENGTH];
        if (WideCharToMultiByte(CP_UTF8, 0, findData.cFileName, -1,
                                fileNameUtf8, MAX_ANIM_NAME_LENGTH, NULL, NULL) <= 0) {
            continue;
        }

        char newRelativePath[MAX_PATH];
        if (relativePathUtf8[0] == '\0') {
            strncpy(newRelativePath, fileNameUtf8, MAX_PATH - 1);
        } else {
            int relativePathLen = snprintf(newRelativePath, MAX_PATH, "%s\\%s", relativePathUtf8, fileNameUtf8);
            if (relativePathLen < 0 || relativePathLen >= MAX_PATH) {
                continue;
            }
        }
        newRelativePath[MAX_PATH - 1] = '\0';

        if (isDirectory) {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                continue;
            }

            wchar_t fullPath[MAX_PATH];
            int len1 = _snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPath, findData.cFileName);
            if (len1 < 0 || len1 >= MAX_PATH) continue;

            ScanAnimationFolderRecursive(fullPath, newRelativePath, ctx, depth + 1, generation);
            if (ctx->failed) {
                stoppedEarly = TRUE;
                break;
            }
        } else {
            if (!AddAnimEntry(ctx, fileNameUtf8, newRelativePath, FALSE)) {
                stoppedEarly = TRUE;
                break;
            }
        }
    } while (FindNextFileW(hFind, &findData));

    DWORD findError = stoppedEarly ? ERROR_SUCCESS : GetLastError();
    FindClose(hFind);
    if (!stoppedEarly && findError != ERROR_NO_MORE_FILES) {
        WriteLog(LOG_LEVEL_WARNING, "Animation folder enumeration failed: %ls (error=%lu)",
                 folderPath, findError);
        ctx->failed = TRUE;
    }
}

/**
 * @brief Scan animations folder and return entries
 */
static int ScanAnimationsFolder(AnimEntry* entries, int capacity, LONG generation) {
    AnimScanContext ctx = {0};
    ctx.entries = entries;
    ctx.count = 0;
    ctx.capacity = capacity;

    wchar_t animPath[MAX_PATH];
    if (!GetAnimationsFolderPathW(animPath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_WARNING, "Failed to get animations folder path");
        return ANIMATION_MENU_SCAN_FAILED;
    }

    DWORD attribs = GetFileAttributesW(animPath);
    if (attribs == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return 0;
        }
        WriteLog(LOG_LEVEL_WARNING, "Failed to stat animations folder: %ls (error=%lu)",
                 animPath, error);
        return ANIMATION_MENU_SCAN_FAILED;
    }
    if (!(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        WriteLog(LOG_LEVEL_WARNING, "Animations path is not a folder: %ls", animPath);
        return ANIMATION_MENU_SCAN_FAILED;
    }

    ScanAnimationFolderRecursive(animPath, "", &ctx, 0, generation);
    if (ctx.failed || IsAnimationMenuScanCanceled(generation)) {
        return ANIMATION_MENU_SCAN_FAILED;
    }

    WriteLog(LOG_LEVEL_INFO, "Animation scan complete: %d animations found%s",
             ctx.count, ctx.truncated ? " (truncated)" : "");
    return ctx.count;
}

static DWORD WINAPI AnimationScanThread(LPVOID lpParam) {
    LONG generation = (LONG)(INT_PTR)lpParam;

    AnimEntry* entries = (AnimEntry*)malloc((size_t)MAX_ANIM_ENTRIES * sizeof(*entries));
    int count = ANIMATION_MENU_SCAN_FAILED;
    if (entries) {
        ZeroMemory(entries, (size_t)MAX_ANIM_ENTRIES * sizeof(*entries));
        count = ScanAnimationsFolder(entries, MAX_ANIM_ENTRIES, generation);
    } else {
        LOG_WARNING("Failed to allocate animation menu scan buffer");
    }
    BOOL scanFailed = (count < 0);

    if (!IsAnimationMenuScanCanceled(generation)) {
        AcquireSRWLockExclusive(&g_animMenuCacheLock);
        if (!IsAnimationMenuScanCanceled(generation)) {
            if (!scanFailed && count > 0 && entries) {
                memcpy(g_animMenuCache, entries, (size_t)count * sizeof(AnimEntry));
            }
            if (scanFailed) {
                count = 0;
            }
            if (count < MAX_ANIM_ENTRIES) {
                ZeroMemory(&g_animMenuCache[count],
                           (size_t)(MAX_ANIM_ENTRIES - count) * sizeof(AnimEntry));
            }
            g_animMenuCacheCount = count;
            g_animMenuCacheReady = !scanFailed;
            g_animMenuCacheFailed = scanFailed;
            InterlockedExchange(&g_animMenuLastScanTick, (LONG)GetTickCount());
        }
        ReleaseSRWLockExclusive(&g_animMenuCacheLock);
    }

    free(entries);
    return 0;
}

void AnimationMenu_RequestScanAsync(void) {
    AcquireSRWLockExclusive(&g_animScanThreadLock);

    if (IsAnimationMenuScanShuttingDown()) {
        ReleaseSRWLockExclusive(&g_animScanThreadLock);
        return;
    }

    if (g_hAnimScanThread) {
        DWORD wait = WaitForSingleObject(g_hAnimScanThread, 0);
        if (wait == WAIT_OBJECT_0) {
            CloseHandle(g_hAnimScanThread);
            g_hAnimScanThread = NULL;
        } else {
            ReleaseSRWLockExclusive(&g_animScanThreadLock);
            return;
        }
    }

    DWORD now = GetTickCount();
    if (IsAnimationMenuCacheRecentlyScanned(now)) {
        ReleaseSRWLockExclusive(&g_animScanThreadLock);
        return;
    }

    LONG generation = InterlockedCompareExchange(&g_animScanGeneration, 0, 0);
    HANDLE hThread = CreateThread(NULL, 0, AnimationScanThread,
                                  (LPVOID)(INT_PTR)generation, 0, NULL);
    if (hThread) {
        g_hAnimScanThread = hThread;
    } else {
        LOG_WARNING("Failed to start animation menu scan thread");
        MarkAnimationMenuScanStartFailure(now);
    }

    ReleaseSRWLockExclusive(&g_animScanThreadLock);
}

void AnimationMenu_Initialize(void) {
    AcquireSRWLockExclusive(&g_animScanThreadLock);
    if (g_hAnimScanThread) {
        DWORD wait = WaitForSingleObject(g_hAnimScanThread, 0);
        if (wait == WAIT_OBJECT_0) {
            CloseHandle(g_hAnimScanThread);
            g_hAnimScanThread = NULL;
        }
    }
    ReleaseSRWLockExclusive(&g_animScanThreadLock);

    InterlockedIncrement(&g_animScanGeneration);
    InterlockedExchange(&g_animScanShuttingDown, 0);
}

void AnimationMenu_Shutdown(void) {
    HANDLE hThread = NULL;

    AcquireSRWLockExclusive(&g_animScanThreadLock);
    InterlockedIncrement(&g_animScanGeneration);
    InterlockedExchange(&g_animScanShuttingDown, 1);
    hThread = g_hAnimScanThread;
    ReleaseSRWLockExclusive(&g_animScanThreadLock);

    if (hThread) {
        DWORD wait = WaitForSingleObject(hThread, ASYNC_ANIM_SCAN_STOP_TIMEOUT_MS);
        if (wait != WAIT_OBJECT_0) {
            LOG_WARNING("Animation menu scan stop timed out after %lu ms (wait=%lu, error=%lu)",
                        (DWORD)ASYNC_ANIM_SCAN_STOP_TIMEOUT_MS,
                        wait,
                        GetLastError());
            if (wait == WAIT_TIMEOUT) {
                AcquireSRWLockExclusive(&g_animScanThreadLock);
                if (g_hAnimScanThread == hThread) {
                    CloseHandle(g_hAnimScanThread);
                    g_hAnimScanThread = NULL;
                }
                ReleaseSRWLockExclusive(&g_animScanThreadLock);
            }
        } else {
            AcquireSRWLockExclusive(&g_animScanThreadLock);
            if (g_hAnimScanThread == hThread) {
                CloseHandle(g_hAnimScanThread);
                g_hAnimScanThread = NULL;
            } else {
                CloseHandle(hThread);
            }
            ReleaseSRWLockExclusive(&g_animScanThreadLock);
        }
    }

    AcquireSRWLockExclusive(&g_animMenuCacheLock);
    ZeroMemory(g_animMenuCache, sizeof(g_animMenuCache));
    g_animMenuCacheCount = 0;
    g_animMenuCacheReady = FALSE;
    g_animMenuCacheFailed = FALSE;
    ReleaseSRWLockExclusive(&g_animMenuCacheLock);
    InterlockedExchange(&g_animMenuLastScanTick, 0);

    ResetAnimationMenuIdMap();
}

/* ============================================================================
 * Menu Building
 * ============================================================================ */

/**
 * @brief Comparator for animation entries (by relative path)
 */
typedef struct {
    const AnimEntry* entry;
} AnimEntrySortItem;

static int CompareAnimEntries(const void* a, const void* b) {
    const AnimEntrySortItem* ea = a;
    const AnimEntrySortItem* eb = b;
    return NaturalPathCompareA(ea->entry->relativePath, eb->entry->relativePath);
}

/**
 * @brief Find or create a submenu with the given name
 */
static HMENU EnsureSubMenu(HMENU hParent, const wchar_t* name) {
    if (!hParent || !name) return NULL;

    int count = GetMenuItemCount(hParent);
    MENUITEMINFOW mii = {0};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING | MIIM_SUBMENU;
    wchar_t buffer[MAX_PATH] = {0};

    for (int i = 0; i < count; i++) {
        mii.dwTypeData = buffer;
        mii.cch = MAX_PATH;
        if (GetMenuItemInfoW(hParent, i, TRUE, &mii)) {
            if (mii.hSubMenu && wcscmp(buffer, name) == 0) {
                return mii.hSubMenu;
            }
        }
    }

    HMENU hSub = CreatePopupMenu();
    if (!hSub) return NULL;

    if (!AppendMenuW(hParent, MF_POPUP, (UINT_PTR)hSub, name)) {
        DestroyMenu(hSub);
        return NULL;
    }
    return hSub;
}

/**
 * @brief Build menu from scanned entries
 */
static void BuildAnimationMenuFromEntries(HMENU hRootMenu, const AnimEntry* entries, int count,
                                           const char* currentAnim, UINT* nextId) {
    if (!entries || count <= 0 || count > MAX_ANIM_ENTRIES) return;

    /* Sort entries for consistent display */
    AnimEntrySortItem sortedEntries[MAX_ANIM_ENTRIES];

    for (int i = 0; i < count; i++) {
        sortedEntries[i].entry = &entries[i];
    }

    qsort(sortedEntries, count, sizeof(sortedEntries[0]), CompareAnimEntries);

    for (int i = 0; i < count; i++) {
        const AnimEntry* entry = sortedEntries[i].entry;

        if (entry->isSpecial) continue;

        /* Parse relative path to find parent menu */
        char pathCopy[MAX_PATH];
        strncpy(pathCopy, entry->relativePath, MAX_PATH - 1);
        pathCopy[MAX_PATH - 1] = '\0';

        char* context = NULL;
        char* token = strtok_s(pathCopy, "\\/", &context);

        HMENU hCurrent = hRootMenu;

        while (token) {
            char* nextToken = strtok_s(NULL, "\\/", &context);

            wchar_t wName[MAX_PATH];
            if (MultiByteToWideChar(CP_UTF8, 0, token, -1, wName, MAX_PATH) <= 0) {
                break;
            }

            if (nextToken) {
                hCurrent = EnsureSubMenu(hCurrent, wName);
                if (!hCurrent) break;
            } else {
                UINT flags = MF_STRING;
                if (currentAnim && strcmp(currentAnim, entry->relativePath) == 0) {
                    flags |= MF_CHECKED;
                }

                if (*nextId < CLOCK_IDM_ANIMATIONS_END) {
                    UINT id = *nextId;
                    if (hCurrent && RememberAnimationMenuId(id, entry->relativePath)) {
                        if (AppendMenuW(hCurrent, flags, id, wName)) {
                            (*nextId)++;
                        } else {
                            ForgetLastAnimationMenuId(id);
                        }
                    }
                }
            }

            token = nextToken;
        }
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Build animation menu with cached folder scan
 */
void BuildAnimationMenu(HMENU hMenu, const char* currentAnimationName) {
    if (!hMenu) return;
    ResetAnimationMenuIdMap();
    AnimationMenu_RequestScanAsync();

    /* Builtin options */
    int builtinCount = 0;
    const BuiltinAnimDef* builtins = GetBuiltinAnims(&builtinCount);

    for (int i = 0; i < builtinCount; i++) {
        UINT flags = MF_STRING;
        if (currentAnimationName && _stricmp(currentAnimationName, builtins[i].name) == 0) {
            flags |= MF_CHECKED;
        }

        const wchar_t* translatedLabel = GetLocalizedString(NULL, builtins[i].menuLabel);
        AppendMenuW(hMenu, flags, builtins[i].menuId, translatedLabel);
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    BOOL cacheReady = FALSE;
    int animCount = 0;
    AnimEntry* animSnapshot =
        (AnimEntry*)malloc((size_t)MAX_ANIM_ENTRIES * sizeof(*animSnapshot));
    if (!animSnapshot) {
        LOG_WARNING("Failed to allocate animation menu cache snapshot");
    }

    AcquireSRWLockShared(&g_animMenuCacheLock);
    cacheReady = g_animMenuCacheReady || g_animMenuCacheFailed;
    animCount = g_animMenuCacheCount;
    if (animCount > MAX_ANIM_ENTRIES) {
        animCount = MAX_ANIM_ENTRIES;
    }
    if (animCount > 0 && animSnapshot) {
        memcpy(animSnapshot, g_animMenuCache, (size_t)animCount * sizeof(*animSnapshot));
    } else if (animCount > 0) {
        animCount = 0;
        cacheReady = FALSE;
    }
    ReleaseSRWLockShared(&g_animMenuCacheLock);

    if (animCount > 0) {
        UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;
        BuildAnimationMenuFromEntries(hMenu, animSnapshot, animCount,
                                      currentAnimationName, &nextId);
    }
    free(animSnapshot);

    if (animCount <= 0 && !cacheReady) {
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, GetLocalizedString(NULL, L"Loading..."));
    }
}

/**
 * @brief Handle animation menu command
 */
BOOL HandleAnimationMenuCommand(HWND hwnd, UINT id) {
    (void)hwnd;

    if (id == CLOCK_IDM_ANIM_SPEED_MEMORY || id == CLOCK_IDM_ANIM_SPEED_CPU || id == CLOCK_IDM_ANIM_SPEED_TIMER) {
        return FALSE;
    }

    if (id == CLOCK_IDM_ANIMATIONS_OPEN_DIR) {
        OpenAnimationsFolder();
        return TRUE;
    }

    /* Check builtin animations first */
    const BuiltinAnimDef* def = GetBuiltinAnimDefById(id);
    if (def) {
        return SetCurrentAnimationName(def->name);
    }

    if (id >= CLOCK_IDM_ANIMATIONS_BASE && id < CLOCK_IDM_ANIMATIONS_END) {
        const char* relativePath = FindAnimationMenuPath(id);
        return relativePath ? SetCurrentAnimationName(relativePath) : FALSE;
    }

    return FALSE;
}

/**
 * @brief Open animations folder in Explorer
 */
void OpenAnimationsFolder(void) {
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    if (base[0] == '\0') return;

    wchar_t wPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, base, -1, wPath, MAX_PATH) <= 0) return;

    ShellExecuteW(NULL, L"open", wPath, NULL, NULL, SW_SHOWNORMAL);
}

/**
 * @brief Get animation name from the current menu ID map
 */
BOOL GetAnimationNameFromMenuId(UINT id, char* outPath, size_t outPathSize) {
    if (!outPath || outPathSize == 0) return FALSE;
    outPath[0] = '\0';

    /* Check builtin animations */
    const BuiltinAnimDef* def = GetBuiltinAnimDefById(id);
    if (def) {
        return CopyStringExactA(def->name, outPath, outPathSize);
    }

    if (id >= CLOCK_IDM_ANIMATIONS_BASE && id < CLOCK_IDM_ANIMATIONS_END) {
        const char* relativePath = FindAnimationMenuPath(id);
        if (!relativePath) return FALSE;

        return CopyStringExactA(relativePath, outPath, outPathSize);
    }

    return FALSE;
}
