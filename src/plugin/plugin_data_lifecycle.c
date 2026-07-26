/**
 * @file plugin_data_lifecycle.c
 * @brief Public plugin-data initialization and shutdown APIs.
 */

#include "plugin_data_internal.h"

void PluginData_SetOutputDirectoryFromPluginPath(const wchar_t* pluginPath) {
    wchar_t pluginDir[MAX_PATH];
    if (!GetDirectoryFromPathW(pluginPath, pluginDir, MAX_PATH)) {
        LOG_WARNING("PluginData: Could not derive output directory from plugin path");
        return;
    }

    if (!PluginData_BeginUse()) return;

    BOOL changed = FALSE;
    EnterCriticalSection(&g_dataCS);
    if (!EnsurePluginOutputDirectoryLocked() ||
        wcscmp(g_pluginOutputDirectory, pluginDir) != 0) {
        wcsncpy(g_pluginOutputDirectory, pluginDir, MAX_PATH - 1);
        g_pluginOutputDirectory[MAX_PATH - 1] = L'\0';
        ClearLastContentCacheLocked();
        InvalidateLastOutputFileStateLocked();
        SetDisplaySourcePathLocked(NULL);
        changed = TRUE;
    }
    LeaveCriticalSection(&g_dataCS);

    if (changed && !StopWatcherThreadIfIdle(PLUGIN_DATA_WATCHER_UI_STOP_WAIT_MS)) {
        LOG_WARNING("PluginData: Watcher stop deferred while changing output directory");
    }

    PluginData_EndUse();
}

BOOL PluginData_GetOutputPath(wchar_t* buffer, size_t bufferSize) {
    if (!PluginData_BeginUse()) return FALSE;
    BOOL ok = GetPluginOutputPathW(buffer, bufferSize);
    PluginData_EndUse();
    return ok;
}

BOOL PluginData_GetDisplaySourcePath(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return FALSE;
    buffer[0] = L'\0';

    if (!PluginData_BeginUse()) return FALSE;

    BOOL ok = FALSE;
    EnterCriticalSection(&g_dataCS);
    if (g_displaySourcePath[0] != L'\0' && wcslen(g_displaySourcePath) < bufferSize) {
        wcsncpy(buffer, g_displaySourcePath, bufferSize - 1);
        buffer[bufferSize - 1] = L'\0';
        ok = TRUE;
    }
    LeaveCriticalSection(&g_dataCS);

    if (!ok) {
        ok = GetPluginOutputPathW(buffer, bufferSize);
    }

    PluginData_EndUse();
    return ok;
}

void PluginData_Init(HWND hwnd) {
    AcquireSRWLockExclusive(&g_pluginDataLifecycleLock);
    if (g_pluginDataInitialized) {
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }

    BOOL locksInitializedNow = FALSE;
    EnsurePluginDataLocksInitialized(&locksInitializedNow);

    if (g_pluginDataResourcesRetained && HasRetainedWatcherThread()) {
        LOG_WARNING("PluginData: Init deferred because a previous watcher thread is still retiring");
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }

    if (!PluginExit_Init(hwnd, &g_dataCS)) {
        LOG_WARNING("PluginData: Init deferred because plugin exit resources are still retiring");
        if (locksInitializedNow && !g_pluginDataResourcesRetained) {
            DeletePluginDataLocks();
        }
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }

    EnterCriticalSection(&g_watchCS);
    g_watchStopInProgress = FALSE;
    LeaveCriticalSection(&g_watchCS);

    g_hNotifyWnd = hwnd;
    g_lastPluginDataRedrawTick = 0;
    InterlockedExchange(&g_pluginDataRedrawQueued, 0);
    InterlockedExchange(&g_pluginDataRedrawTimerArmed, 0);
    InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0);
    InterlockedExchange(&g_forceNextUpdate, FALSE);
    g_watchStartFailureCooldownUntil = 0;
    SetPollIntervalMs(DEFAULT_POLL_INTERVAL_MS);
    SetWatcherRunning(FALSE);

    EnterCriticalSection(&g_dataCS);
    ResetPluginDataStateLocked();
    LeaveCriticalSection(&g_dataCS);

    /* Ensure output directory exists (don't clear user's output.txt) */
    wchar_t outputPath[MAX_PATH];
    if (GetPluginOutputPathW(outputPath, MAX_PATH)) {
        EnsureOutputDirExistsW(outputPath);
    }

    g_pluginDataResourcesRetained = FALSE;
    g_pluginDataInitialized = TRUE;
    ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
}

void PluginData_Shutdown(void) {
    AcquireSRWLockExclusive(&g_pluginDataLifecycleLock);
    if (!g_pluginDataInitialized && !g_pluginDataResourcesRetained) {
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }
    g_pluginDataInitialized = FALSE;

    /* Stop watcher thread */
    BOOL watcherStopped = StopWatcherThreadIfIdle(PLUGIN_DATA_WATCHER_SHUTDOWN_WAIT_MS);
    StopPluginDataRedrawTimer(g_hNotifyWnd);
    g_hNotifyWnd = NULL;
    InterlockedExchange(&g_pluginDataRedrawQueued, 0);
    InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0);
    if (!watcherStopped) {
        g_pluginDataResourcesRetained = TRUE;
        LOG_WARNING("PluginData: Watcher resources retained because the watcher did not stop during shutdown");
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }
    EnterCriticalSection(&g_watchCS);
    if (g_hWatchStopEvent) {
        CloseHandle(g_hWatchStopEvent);
        g_hWatchStopEvent = NULL;
    }
    if (g_hWatchWakeEvent) {
        CloseHandle(g_hWatchWakeEvent);
        g_hWatchWakeEvent = NULL;
    }
    LeaveCriticalSection(&g_watchCS);

    /* Shutdown exit subsystem */
    if (!PluginExit_Shutdown()) {
        g_pluginDataResourcesRetained = TRUE;
        LOG_WARNING("PluginData: Exit countdown resources retained because countdown did not stop during shutdown");
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }

    /* Free memory */
    EnterCriticalSection(&g_dataCS);
    ResetPluginDataStateLocked();
    LeaveCriticalSection(&g_dataCS);

    DeletePluginDataLocks();
    g_pluginDataResourcesRetained = FALSE;
    ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
}
