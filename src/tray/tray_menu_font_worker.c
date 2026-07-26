/**
 * @file tray_menu_font_worker.c
 * @brief Asynchronous font scan worker and shared cache state.
 */

#include "tray_menu_font_internal.h"

#include "log.h"

#include <stdlib.h>
#include <string.h>

#define FONT_MENU_SCAN_REFRESH_COOLDOWN_MS 10000

FontEntry g_fontMenuCache[MAX_FONT_ENTRIES];
int g_fontMenuCacheCount = 0;
BOOL g_fontMenuCacheReady = FALSE;
BOOL g_fontMenuCacheFailed = FALSE;
SRWLOCK g_fontMenuCacheLock = SRWLOCK_INIT;
SRWLOCK g_fontScanThreadLock = SRWLOCK_INIT;
HANDLE g_hFontScanThread = NULL;
HANDLE g_hRetiredFontScanThread = NULL;
volatile LONG g_fontScanShuttingDown = 0;
volatile LONG g_fontScanGeneration = 0;
volatile LONG g_fontMenuLastScanTick = 0;

static BOOL IsFontMenuScanShuttingDown(void) {
    return InterlockedCompareExchange(&g_fontScanShuttingDown, 0, 0) != 0;
}

BOOL FontMenuInternal_IsScanCanceled(LONG generation) {
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

BOOL FontMenuInternal_CleanupRetiredScanThreadLocked(DWORD waitMs) {
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

static DWORD WINAPI FontScanThread(LPVOID lpParam) {
    LONG generation = (LONG)(INT_PTR)lpParam;

    FontEntry* entries = (FontEntry*)malloc((size_t)MAX_FONT_ENTRIES * sizeof(*entries));
    int count = FONT_MENU_SCAN_FAILED;
    if (entries) {
        ZeroMemory(entries, (size_t)MAX_FONT_ENTRIES * sizeof(*entries));
        count = FontMenuInternal_ScanFontsFolder(entries, MAX_FONT_ENTRIES, generation);
    } else {
        LOG_WARNING("Failed to allocate font menu scan buffer");
    }
    BOOL scanFailed = (count < 0);

    if (!FontMenuInternal_IsScanCanceled(generation)) {
        AcquireSRWLockExclusive(&g_fontMenuCacheLock);
        if (!FontMenuInternal_IsScanCanceled(generation)) {
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

    if (!FontMenuInternal_CleanupRetiredScanThreadLocked(0)) {
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
