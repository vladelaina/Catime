
static BOOL StartWatcherThreadIfNeeded(void) {
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

static BOOL StopWatcherThreadIfIdle(DWORD waitMs) {
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

static void EnsurePluginDataLocksInitialized(BOOL* initializedNow) {
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

static void DeletePluginDataLocks(void) {
    if (!g_pluginDataLocksInitialized) {
        return;
    }

    DeleteCriticalSection(&g_watchCS);
    DeleteCriticalSection(&g_dataCS);
    g_pluginDataLocksInitialized = FALSE;
}

static BOOL HasRetainedWatcherThread(void) {
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
