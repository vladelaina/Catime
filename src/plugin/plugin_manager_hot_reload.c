/**
 * @file plugin_manager_hot_reload.c
 * @brief Hot-reload monitoring and thread lifecycle.
 */

#include "plugin_manager_internal.h"

int ComparePluginInfo(const void* a, const void* b) {
    const PluginInfo* pa = (const PluginInfo*)a;
    const PluginInfo* pb = (const PluginInfo*)b;
    return NaturalCompareW(pa->displayName, pb->displayName);
}

/**
 * @brief Get file modification time
 */
BOOL GetFileModTime(const wchar_t* path, FILETIME* modTime) {
    HANDLE hFile = CreateFileW(path, GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    BOOL result = GetFileTime(hFile, NULL, NULL, modTime);
    CloseHandle(hFile);
    return result;
}

/**
 * @brief Hot-reload monitoring thread
 */
DWORD WINAPI HotReloadThread(LPVOID lpParam) {
    (void)lpParam;

    while (IsHotReloadRunning()) {
        if (g_hHotReloadStopEvent &&
            WaitForSingleObject(g_hHotReloadStopEvent, 1000) == WAIT_OBJECT_0) {
            break;
        }
        if (!IsHotReloadRunning()) break;

        int indexToMonitor = -1;
        wchar_t pathToCheck[MAX_PATH] = {0};
        wchar_t nameToCheck[64] = {0};
        FILETIME lastModTime = {0};

        EnterCriticalSection(&g_pluginCS);

        /* Find running plugin or last running */
        for (int i = 0; i < g_pluginCount; i++) {
            if (g_plugins[i].isRunning) {
                indexToMonitor = i;
                g_lastRunningPluginIndex = i;
                break;
            }
        }
        if (indexToMonitor < 0 && g_lastRunningPluginIndex >= 0 &&
            g_lastRunningPluginIndex < g_pluginCount) {
            indexToMonitor = g_lastRunningPluginIndex;
        }

        if (indexToMonitor >= 0) {
            wcsncpy(pathToCheck, g_plugins[indexToMonitor].path, MAX_PATH - 1);
            pathToCheck[MAX_PATH - 1] = L'\0';
            wcsncpy(nameToCheck, g_plugins[indexToMonitor].name, 63);
            nameToCheck[63] = L'\0';
            lastModTime = g_plugins[indexToMonitor].lastModTime;
        }

        LeaveCriticalSection(&g_pluginCS);

        if (indexToMonitor >= 0) {
            FILETIME currentModTime;
            if (GetFileModTime(pathToCheck, &currentModTime) &&
                CompareFileTime(&currentModTime, &lastModTime) != 0) {
                BOOL shouldPostReload = FALSE;

                EnterCriticalSection(&g_pluginCS);
                if (indexToMonitor < g_pluginCount &&
                    wcscmp(g_plugins[indexToMonitor].name, nameToCheck) == 0 &&
                    wcscmp(g_plugins[indexToMonitor].path, pathToCheck) == 0 &&
                    CompareFileTime(&currentModTime, &g_plugins[indexToMonitor].lastModTime) != 0) {
                    g_plugins[indexToMonitor].lastModTime = currentModTime;
                    shouldPostReload = TRUE;
                }
                LeaveCriticalSection(&g_pluginCS);

                if (shouldPostReload) {
                    /* Post message to main thread instead of calling directly */
                    /* This avoids deadlock when security dialog needs to be shown */
                    HWND hwnd = PluginProcess_GetNotifyWindow();
                    if (hwnd) {
                        LONG requestGeneration = 0;
                        EnterCriticalSection(&g_pluginCS);
                        requestGeneration = QueueHotReloadRequestLocked(indexToMonitor,
                                                                        nameToCheck,
                                                                        pathToCheck);
                        LeaveCriticalSection(&g_pluginCS);

                        if (requestGeneration == 0 ||
                            !PostMessage(hwnd, WM_PLUGIN_HOT_RELOAD,
                                         (WPARAM)requestGeneration, 0)) {
                            EnterCriticalSection(&g_pluginCS);
                            if (InterlockedCompareExchange(&g_hotReloadRequestGeneration,
                                                           0, 0) == requestGeneration) {
                                g_hotReloadRequestPending = FALSE;
                            }
                            LeaveCriticalSection(&g_pluginCS);
                        }
                    } else {
                        /* No window available - skip hot-reload this cycle */
                        /* Window should be set during initialization */
                        LOG_WARNING("[HotReload] No notify window, skipping reload");
                    }
                }
            }
        }
    }

    return 0;
}

BOOL AnyPluginRunningLocked(void) {
    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            return TRUE;
        }
    }
    return FALSE;
}

void StartHotReloadIfNeeded(void) {
    AcquireSRWLockExclusive(&g_hotReloadLock);

    CleanupCompletedHotReloadThreadLocked();
    if (g_hHotReloadThread) {
        ReleaseSRWLockExclusive(&g_hotReloadLock);
        return;
    }

    DWORD now = GetTickCount();
    if (IsHotReloadStartFailureCoolingDown(now)) {
        ReleaseSRWLockExclusive(&g_hotReloadLock);
        return;
    }

    SetHotReloadRunning(TRUE);
    g_hHotReloadStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_hHotReloadStopEvent) {
        SetHotReloadRunning(FALSE);
        LOG_WARNING("Failed to create hot-reload stop event");
        MarkHotReloadStartFailure(now);
        ReleaseSRWLockExclusive(&g_hotReloadLock);
        return;
    }

    g_hHotReloadThread = CreateThread(NULL, 0, HotReloadThread, NULL, 0, NULL);
    if (!g_hHotReloadThread) {
        LOG_WARNING("Failed to start hot-reload thread");
        CloseHandle(g_hHotReloadStopEvent);
        g_hHotReloadStopEvent = NULL;
        SetHotReloadRunning(FALSE);
        MarkHotReloadStartFailure(now);
    } else {
        g_hotReloadStartFailureCooldownUntil = 0;
    }

    ReleaseSRWLockExclusive(&g_hotReloadLock);
}

void CleanupCompletedHotReloadThreadLocked(void) {
    if (!g_hHotReloadThread) return;

    DWORD waitResult = WaitForSingleObject(g_hHotReloadThread, 0);
    if (waitResult == WAIT_TIMEOUT) {
        return;
    }
    if (waitResult == WAIT_FAILED) {
        LOG_WARNING("Hot-reload thread status check failed: %lu", GetLastError());
        return;
    }

    CloseHandle(g_hHotReloadThread);
    g_hHotReloadThread = NULL;

    if (g_hHotReloadStopEvent) {
        CloseHandle(g_hHotReloadStopEvent);
        g_hHotReloadStopEvent = NULL;
    }
    SetHotReloadRunning(FALSE);
}

BOOL StopHotReloadThreadLocked(void) {
    BOOL stopped = TRUE;

    CleanupCompletedHotReloadThreadLocked();

    if (g_hHotReloadThread) {
        SetHotReloadRunning(FALSE);
        if (g_hHotReloadStopEvent) {
            SetEvent(g_hHotReloadStopEvent);
        }
        DWORD waitResult = WaitForSingleObject(g_hHotReloadThread, HOT_RELOAD_STOP_TIMEOUT_MS);
        if (waitResult == WAIT_TIMEOUT) {
            LOG_WARNING("Hot-reload thread stop timed out after %lu ms",
                        (DWORD)HOT_RELOAD_STOP_TIMEOUT_MS);
            return FALSE;
        }
        if (waitResult == WAIT_FAILED) {
            LOG_WARNING("Hot-reload thread stop wait failed: %lu", GetLastError());
            return FALSE;
        }
        CloseHandle(g_hHotReloadThread);
        g_hHotReloadThread = NULL;
    } else {
        SetHotReloadRunning(FALSE);
    }
    if (g_hHotReloadStopEvent) {
        CloseHandle(g_hHotReloadStopEvent);
        g_hHotReloadStopEvent = NULL;
    }

    return stopped;
}

BOOL StopHotReloadThread(void) {
    BOOL stopped;
    AcquireSRWLockExclusive(&g_hotReloadLock);
    stopped = StopHotReloadThreadLocked();
    ReleaseSRWLockExclusive(&g_hotReloadLock);
    return stopped;
}

void StopHotReloadIfIdle(void) {
    BOOL hasRunningPlugin = FALSE;

    if (!g_pluginManagerInitialized) return;

    AcquireSRWLockExclusive(&g_hotReloadLock);
    if (!g_hHotReloadThread) {
        ReleaseSRWLockExclusive(&g_hotReloadLock);
        return;
    }

    EnterCriticalSection(&g_pluginCS);
    hasRunningPlugin = AnyPluginRunningLocked();
    LeaveCriticalSection(&g_pluginCS);

    if (!hasRunningPlugin) {
        StopHotReloadThreadLocked();
    }
    ReleaseSRWLockExclusive(&g_hotReloadLock);
}

/**
 * @brief Extract display name from plugin filename (keep extension)
 */
void ExtractDisplayName(const wchar_t* filename, wchar_t* displayName, size_t bufferSize) {
    wcsncpy(displayName, filename, bufferSize - 1);
    displayName[bufferSize - 1] = L'\0';
}
