/**
 * @file plugin_data_watcher.c
 * @brief Watcher synchronization and thread lifecycle.
 */

#include "plugin_data_internal.h"

BOOL IsWatcherRunning(void) {
    return InterlockedCompareExchange(&g_isRunning, FALSE, FALSE) != FALSE;
}

void SetWatcherRunning(BOOL running) {
    InterlockedExchange(&g_isRunning, running ? TRUE : FALSE);
}

BOOL IsWatcherStartFailureCoolingDown(DWORD now) {
    return g_watchStartFailureCooldownUntil != 0 &&
           (LONG)(g_watchStartFailureCooldownUntil - now) > 0;
}

void MarkWatcherStartFailure(DWORD now) {
    DWORD cooldownUntil = now + PLUGIN_DATA_WATCHER_START_FAILURE_COOLDOWN_MS;
    g_watchStartFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
}

void CloseWatcherEventsIfIdleLocked(void) {
    if (g_hWatchThread || g_watchStopInProgress) return;

    if (g_hWatchStopEvent) {
        CloseHandle(g_hWatchStopEvent);
        g_hWatchStopEvent = NULL;
    }
    if (g_hWatchWakeEvent) {
        CloseHandle(g_hWatchWakeEvent);
        g_hWatchWakeEvent = NULL;
    }
}

void WakeWatcherThreadLocked(void) {
    if (g_hWatchWakeEvent) {
        SetEvent(g_hWatchWakeEvent);
    }
}

void WakeWatcherThread(void) {
    EnterCriticalSection(&g_watchCS);
    WakeWatcherThreadLocked();
    LeaveCriticalSection(&g_watchCS);
}

void CleanupCompletedWatcherThreadLocked(void) {
    HANDLE hThread = g_hWatchThread;
    if (!hThread || g_watchStopInProgress) return;

    if (WaitForSingleObject(hThread, 0) == WAIT_OBJECT_0) {
        CloseHandle(hThread);
        g_hWatchThread = NULL;
        SetWatcherRunning(FALSE);
        WakeAllConditionVariable(&g_watchStopCompleted);
    }
}

BOOL WaitForWatcherStopGateLocked(DWORD waitMs) {
    DWORD waitStart = GetTickCount();
    while (g_watchStopInProgress) {
        DWORD elapsed = GetTickCount() - waitStart;
        DWORD remaining = elapsed >= waitMs ? 0 : waitMs - elapsed;
        if (remaining == 0 ||
            !SleepConditionVariableCS(&g_watchStopCompleted, &g_watchCS, remaining)) {
            LOG_WARNING("PluginData: Timed out waiting for watcher stop gate after %lu ms",
                        waitMs);
            return FALSE;
        }
    }
    return TRUE;
}

BOOL EnsureWatcherEvents(void) {
    if (!g_hWatchStopEvent) {
        g_hWatchStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!g_hWatchStopEvent) return FALSE;
    }
    if (!g_hWatchWakeEvent) {
        g_hWatchWakeEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!g_hWatchWakeEvent) {
            if (!g_hWatchThread && g_hWatchStopEvent) {
                CloseHandle(g_hWatchStopEvent);
                g_hWatchStopEvent = NULL;
            }
            return FALSE;
        }
    }
    return TRUE;
}

BOOL StartWatcherThreadIfNeeded(void) {
    EnterCriticalSection(&g_watchCS);

    if (!WaitForWatcherStopGateLocked(PLUGIN_DATA_WATCHER_STOP_GATE_WAIT_MS)) {
        LeaveCriticalSection(&g_watchCS);
        return FALSE;
    }

    CleanupCompletedWatcherThreadLocked();
    if (g_hWatchThread) {
        if (!IsWatcherRunning()) {
            LOG_WARNING("PluginData: Watcher is still retiring; start deferred");
            LeaveCriticalSection(&g_watchCS);
            return FALSE;
        }
        LeaveCriticalSection(&g_watchCS);
        return TRUE;
    }

    DWORD now = GetTickCount();
    if (IsWatcherStartFailureCoolingDown(now)) {
        LeaveCriticalSection(&g_watchCS);
        return FALSE;
    }

    if (!EnsureWatcherEvents()) {
        LOG_ERROR("PluginData: Failed to create watcher events");
        MarkWatcherStartFailure(now);
        LeaveCriticalSection(&g_watchCS);
        return FALSE;
    }

    ResetEvent(g_hWatchStopEvent);
    ResetEvent(g_hWatchWakeEvent);
    SetWatcherRunning(TRUE);
    g_hWatchThread = CreateThread(NULL, 0, FileWatcherThread, NULL, 0, NULL);
    if (!g_hWatchThread) {
        SetWatcherRunning(FALSE);
        LOG_ERROR("PluginData: Failed to start watcher thread");
        MarkWatcherStartFailure(now);
        CloseWatcherEventsIfIdleLocked();
        LeaveCriticalSection(&g_watchCS);
        return FALSE;
    }

    g_watchStartFailureCooldownUntil = 0;
    LeaveCriticalSection(&g_watchCS);
    return TRUE;
}

BOOL StopWatcherThreadIfIdle(DWORD waitMs) {
    HANDLE hThread = NULL;
    BOOL stopped = TRUE;

    EnterCriticalSection(&g_watchCS);

    if (!WaitForWatcherStopGateLocked(PLUGIN_DATA_WATCHER_STOP_GATE_WAIT_MS)) {
        LeaveCriticalSection(&g_watchCS);
        return FALSE;
    }

    CleanupCompletedWatcherThreadLocked();
    if (!g_hWatchThread) {
        LeaveCriticalSection(&g_watchCS);
        return TRUE;
    }

    hThread = g_hWatchThread;
    g_hWatchThread = NULL;
    g_watchStopInProgress = TRUE;
    SetWatcherRunning(FALSE);
    if (g_hWatchStopEvent) {
        SetEvent(g_hWatchStopEvent);
    }
    WakeWatcherThreadLocked();
    LeaveCriticalSection(&g_watchCS);

    DWORD waitResult = WaitForSingleObject(hThread, waitMs);
    DWORD waitError = (waitResult == WAIT_FAILED) ? GetLastError() : ERROR_SUCCESS;
    EnterCriticalSection(&g_watchCS);
    if (waitResult == WAIT_OBJECT_0 || WaitForSingleObject(hThread, 0) == WAIT_OBJECT_0) {
        CloseHandle(hThread);
        stopped = TRUE;
    } else {
        g_hWatchThread = hThread;
        LOG_WARNING("PluginData: Watcher stop wait returned %lu (error=%lu)",
                    waitResult, waitError);
        stopped = FALSE;
    }
    g_watchStopInProgress = FALSE;
    WakeAllConditionVariable(&g_watchStopCompleted);
    LeaveCriticalSection(&g_watchCS);
    return stopped;
}

void EnsurePluginDataLocksInitialized(BOOL* initializedNow) {
    if (initializedNow) {
        *initializedNow = FALSE;
    }
    if (g_pluginDataLocksInitialized) {
        return;
    }

    InitializeCriticalSection(&g_dataCS);
    InitializeCriticalSection(&g_watchCS);
    g_pluginDataLocksInitialized = TRUE;
    if (initializedNow) {
        *initializedNow = TRUE;
    }
}

void DeletePluginDataLocks(void) {
    if (!g_pluginDataLocksInitialized) {
        return;
    }

    DeleteCriticalSection(&g_watchCS);
    DeleteCriticalSection(&g_dataCS);
    g_pluginDataLocksInitialized = FALSE;
}

BOOL HasRetainedWatcherThread(void) {
    BOOL retained = FALSE;
    if (!g_pluginDataLocksInitialized) {
        return FALSE;
    }

    EnterCriticalSection(&g_watchCS);
    CleanupCompletedWatcherThreadLocked();
    retained = (g_hWatchThread != NULL || g_watchStopInProgress);
    LeaveCriticalSection(&g_watchCS);
    return retained;
}
