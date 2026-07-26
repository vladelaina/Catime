/**
 * @file plugin_data_watcher_thread.c
 * @brief Output-file watcher thread and change debounce.
 */

#include "plugin_data_internal.h"

static DWORD GetChangeDebounceMs(void) {
    DWORD debounceMs = GetPollIntervalMs();
    if (debounceMs < MIN_POLL_INTERVAL_MS) debounceMs = MIN_POLL_INTERVAL_MS;
    if (debounceMs > MAX_CHANGE_DEBOUNCE_MS) debounceMs = MAX_CHANGE_DEBOUNCE_MS;
    return debounceMs;
}

static BOOL RearmChangeNotification(HANDLE* changeHandle) {
    if (!changeHandle || *changeHandle == INVALID_HANDLE_VALUE) {
        return TRUE;
    }

    if (FindNextChangeNotification(*changeHandle)) {
        return TRUE;
    }

    FindCloseChangeNotification(*changeHandle);
    *changeHandle = INVALID_HANDLE_VALUE;
    LOG_WARNING("PluginData: Change notification lost, switching to polling");
    return FALSE;
}

static BOOL DebouncePluginOutputChange(HANDLE* changeHandle) {
    DWORD debounceMs = GetChangeDebounceMs();
    DWORD start = GetTickCount();

    while (GetTickCount() - start < debounceMs) {
        HANDLE waitHandles[3];
        DWORD waitCount = 0;
        DWORD elapsed = GetTickCount() - start;
        DWORD remaining = debounceMs - elapsed;

        waitHandles[waitCount++] = g_hWatchStopEvent;
        waitHandles[waitCount++] = g_hWatchWakeEvent;
        if (changeHandle && *changeHandle != INVALID_HANDLE_VALUE) {
            waitHandles[waitCount++] = *changeHandle;
        }

        DWORD waitResult = WaitForMultipleObjects(waitCount, waitHandles, FALSE, remaining);
        if (waitResult == WAIT_TIMEOUT) {
            return TRUE;
        }
        if (waitResult == WAIT_OBJECT_0) {
            return FALSE;
        }
        if (waitResult == WAIT_OBJECT_0 + 1) {
            if (g_hWatchWakeEvent) {
                ResetEvent(g_hWatchWakeEvent);
            }
            return TRUE;
        }
        if (changeHandle && *changeHandle != INVALID_HANDLE_VALUE &&
            waitResult == WAIT_OBJECT_0 + 2) {
            RearmChangeNotification(changeHandle);
            continue;
        }
        if (waitResult == WAIT_FAILED) {
            LOG_WARNING("PluginData: Change debounce wait failed (error=%lu)", GetLastError());
            return TRUE;
        }

        return TRUE;
    }

    return TRUE;
}

/**
 * @brief Background thread to monitor plugin data file
 */
DWORD WINAPI FileWatcherThread(LPVOID lpParam) {
    (void)lpParam;

    wchar_t filePath[MAX_PATH];
    if (!GetPluginOutputPathW(filePath, MAX_PATH)) {
        LOG_WARNING("PluginData: Failed to get output file path");
        SetWatcherRunning(FALSE);
        return 0;
    }

    wchar_t outputDir[MAX_PATH] = {0};
    HANDLE changeHandle = INVALID_HANDLE_VALUE;
    if (GetPluginOutputDirectory(outputDir, MAX_PATH)) {
        changeHandle = FindFirstChangeNotificationW(
            outputDir,
            FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE
        );
        if (changeHandle == INVALID_HANDLE_VALUE) {
            LOG_WARNING("PluginData: Change notification unavailable, falling back to polling");
        }
    }

    FILETIME lastWriteTime = {0};
    ULONGLONG lastFileSize = 0;
    EnterCriticalSection(&g_dataCS);
    CopyLastOutputFileStateLocked(&lastWriteTime, &lastFileSize);
    LeaveCriticalSection(&g_dataCS);

    while (IsWatcherRunning()) {
        BOOL forceRefresh = InterlockedExchange(&g_forceNextUpdate, FALSE) != FALSE;
        if (forceRefresh) {
            ZeroMemory(&lastWriteTime, sizeof(lastWriteTime));
            lastFileSize = 0;
        }

        ProcessPluginOutputFile(filePath, forceRefresh, &lastWriteTime, &lastFileSize);

        HANDLE waitHandles[3];
        DWORD waitCount = 0;
        waitHandles[waitCount++] = g_hWatchStopEvent;
        waitHandles[waitCount++] = g_hWatchWakeEvent;
        if (changeHandle != INVALID_HANDLE_VALUE) {
            waitHandles[waitCount++] = changeHandle;
        }

        DWORD waitTimeout = (changeHandle != INVALID_HANDLE_VALUE) ? INFINITE : GetPollIntervalMs();
        DWORD waitResult = WaitForMultipleObjects(waitCount, waitHandles, FALSE, waitTimeout);
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }
        if (waitResult == WAIT_OBJECT_0 + 1) {
            if (g_hWatchWakeEvent) {
                ResetEvent(g_hWatchWakeEvent);
            }
            continue;
        }
        if (changeHandle != INVALID_HANDLE_VALUE && waitResult == WAIT_OBJECT_0 + 2) {
            RearmChangeNotification(&changeHandle);
            if (!DebouncePluginOutputChange(&changeHandle)) {
                break;
            }
            continue;
        }
        if (waitResult == WAIT_FAILED) {
            LOG_WARNING("PluginData: Watch wait failed (error=%lu), switching to polling", GetLastError());
            if (changeHandle != INVALID_HANDLE_VALUE) {
                FindCloseChangeNotification(changeHandle);
                changeHandle = INVALID_HANDLE_VALUE;
            }
            continue;
        }
    }

    if (changeHandle != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(changeHandle);
    }

    SetWatcherRunning(FALSE);

    return 0;
}
