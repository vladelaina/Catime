/**
 * @file tray_menu_font_lifecycle.c
 * @brief Font-folder watcher and worker startup/shutdown lifecycle.
 */

#include "tray_menu_font_internal.h"

#include "font/font_path_manager.h"
#include "log.h"
#include "utils/directory_watcher.h"

#define ASYNC_FONT_SCAN_STOP_TIMEOUT_MS 2000

static DirectoryWatcher g_fontFolderWatcher = {0};

static void InvalidateFontMenuScanCooldown(void) {
    InterlockedExchange(&g_fontMenuLastScanTick, 0);
}

static void OnFontFolderChanged(void* context) {
    (void)context;
    InvalidateFontMenuScanCooldown();
    FontMenu_RequestScanAsync();
}

static void StartFontFolderWatcher(void) {
    wchar_t fontsPath[MAX_PATH];
    if (!GetFontsFolderW(fontsPath, MAX_PATH, TRUE)) {
        LOG_WARNING("Font folder watcher could not resolve fonts path");
        return;
    }

    DirectoryWatcher_Start(&g_fontFolderWatcher,
                           fontsPath,
                           TRUE,
                           DIRECTORY_WATCHER_DEFAULT_FILTER,
                           DIRECTORY_WATCHER_DEFAULT_DEBOUNCE_MS,
                           OnFontFolderChanged,
                           NULL,
                           "FontFolderWatcher");
}

static void StopFontFolderWatcher(void) {
    DirectoryWatcher_Stop(&g_fontFolderWatcher, ASYNC_FONT_SCAN_STOP_TIMEOUT_MS);
}

void FontMenu_Initialize(void) {
    AcquireSRWLockExclusive(&g_fontScanThreadLock);
    if (!FontMenuInternal_CleanupRetiredScanThreadLocked(ASYNC_FONT_SCAN_STOP_TIMEOUT_MS)) {
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
    StartFontFolderWatcher();
}

void FontMenu_Shutdown(void) {
    HANDLE hThread = NULL;

    StopFontFolderWatcher();

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
                    if (FontMenuInternal_CleanupRetiredScanThreadLocked(0)) {
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

    FontMenuInternal_ResetIdMap();
}
