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
#include "utils/directory_watcher.h"
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
static DirectoryWatcher g_fontFolderWatcher = {0};
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
#include "tray_menu_font_part01.inc"
#include "tray_menu_font_part02.inc"
#include "tray_menu_font_part03.inc"
