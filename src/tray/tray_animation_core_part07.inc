    ResetBuiltinIconUpdateCache();
    EnsureTrayAnimationTimerState();
    UpdateTrayIconToCurrentFrame();

    result = TRUE;

done:
    EndTrayAnimationRuntimeUse();
    if (result) {
        RefreshTrayBackgroundWorkState();
    }
    return result;
}

/**
 * @brief Set current animation
 */
BOOL SetCurrentAnimationName(const char* name) {
    return SetCurrentAnimationNameInternal(name, TRUE);
}

static void CancelStartupAnimationRetry(void) {
    if (g_startupRetryHwnd) {
        KillTimer(g_startupRetryHwnd, STARTUP_ANIMATION_RETRY_TIMER_ID);
    }
    g_startupRetryHwnd = NULL;
    g_startupRetryAnimationName[0] = '\0';
    g_startupRetryAttemptCount = 0;
}

static void ScheduleStartupAnimationRetry(HWND hwnd, const char* animationName) {
    if (!IsValidTrayAnimationWindow(hwnd) || !animationName || !*animationName ||
        _stricmp(animationName, "__logo__") == 0 ||
        !CopyStringExactA(animationName, g_startupRetryAnimationName,
                          sizeof(g_startupRetryAnimationName))) {
        return;
    }

    g_startupRetryAttemptCount = 0;
    if (SetTimer(hwnd, STARTUP_ANIMATION_RETRY_TIMER_ID,
                 STARTUP_ANIMATION_RETRY_INTERVAL_MS,
                 TrayAnimationStartupRetryTimerProc) == 0) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Failed to schedule startup tray animation retry (error=%lu)",
                 GetLastError());
        g_startupRetryAnimationName[0] = '\0';
        return;
    }

    g_startupRetryHwnd = hwnd;
    WriteLog(LOG_LEVEL_INFO,
             "Scheduled retry for startup tray animation '%s'", animationName);
}

static void CALLBACK TrayAnimationStartupRetryTimerProc(HWND hwnd, UINT msg,
                                                        UINT_PTR id, DWORD time) {
    (void)msg;
    (void)time;

    if (id != STARTUP_ANIMATION_RETRY_TIMER_ID) return;
    if (!IsValidTrayAnimationWindow(hwnd) || hwnd != g_startupRetryHwnd ||
        !IsTrayAnimationRuntimeActive()) {
        CancelStartupAnimationRetry();
        return;
    }

    char configPath[MAX_PATH] = {0};
    char configuredAnimation[MAX_PATH] = "__logo__";
    GetConfigPath(configPath, sizeof(configPath));
    ReadAnimationNameFromConfig(configuredAnimation,
                                sizeof(configuredAnimation), configPath);

    if (_stricmp(configuredAnimation, g_startupRetryAnimationName) != 0 ||
        _stricmp(g_animationName, configuredAnimation) == 0 ||
        _stricmp(g_animationName, "__logo__") != 0) {
        CancelStartupAnimationRetry();
        return;
    }

    g_startupRetryAttemptCount++;
    if (SetCurrentAnimationNameInternal(g_startupRetryAnimationName, FALSE)) {
        WriteLog(LOG_LEVEL_INFO,
                 "Recovered startup tray animation '%s' on retry %d",
                 configuredAnimation, g_startupRetryAttemptCount);
        CancelStartupAnimationRetry();
        return;
    }

    if (g_startupRetryAttemptCount >= STARTUP_ANIMATION_RETRY_MAX_ATTEMPTS) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Startup tray animation '%s' remained unavailable after %d retries; keeping its configuration for the next launch",
                 configuredAnimation, g_startupRetryAttemptCount);
        CancelStartupAnimationRetry();
    }
}

static BOOL QueueAnimationPreviewRequest(const char* name, BOOL fromPath) {
    if (!name || !*name) return FALSE;
    if (!BeginTrayAnimationRuntimeUse()) return FALSE;

    BOOL queued = FALSE;
    char requestedName[MAX_PATH] = {0};
    if (!CopyStringExactA(name, requestedName, sizeof(requestedName))) {
        LOG_WARNING("Ignoring animation preview request because the name is too long: %s", name);
        goto done;
    }

    AcquireSRWLockExclusive(&g_previewWorkerLock);

    CleanupCompletedPreviewWorkerLocked();

    BOOL duplicateRequest = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    duplicateRequest =
        (g_isPreviewActive &&
         g_previewAnimationFromPath == fromPath &&
         g_previewAnimationName[0] != '\0' &&
         _stricmp(g_previewAnimationName, requestedName) == 0) ||
        (g_previewWorkerThread &&
         g_pendingPreviewFromPath == fromPath &&
         g_pendingPreviewName[0] != '\0' &&
         _stricmp(g_pendingPreviewName, requestedName) == 0);
    if (IsAnimCriticalSectionReady()) {
        LeaveCriticalSection(&g_animCriticalSection);
    }
    if (duplicateRequest) {
        queued = TRUE;
        ReleaseSRWLockExclusive(&g_previewWorkerLock);
        goto done;
    }

    if (!EnsurePreviewWorkerStartedLocked()) {
        ReleaseSRWLockExclusive(&g_previewWorkerLock);
        goto done;
    }

    SignalPreviewDecodeCancelLocked();

    InterlockedIncrement(&g_previewRequestSerial);

    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    CopyStringExactA(requestedName, g_pendingPreviewName, sizeof(g_pendingPreviewName));
    g_pendingPreviewFromPath = fromPath;
    if (IsAnimCriticalSectionReady()) {
        LeaveCriticalSection(&g_animCriticalSection);
    }

    if (g_previewRequestEvent) {
        WakePreviewWorkerLocked();
        queued = TRUE;
    }
    ReleaseSRWLockExclusive(&g_previewWorkerLock);

done:
    EndTrayAnimationRuntimeUse();
    return queued;
}

/**
 * @brief Queue animation preview from file path
 */
BOOL PreviewAnimationFromFile(HWND hwnd, const char* filePath) {
    (void)hwnd;
    return QueueAnimationPreviewRequest(filePath, TRUE);
}

/**
 * @brief Preview worker thread
 */
static DWORD WINAPI PreviewWorkerThread(LPVOID param) {
    (void)param;

    HANDLE waitHandles[2] = { g_previewStopEvent, g_previewRequestEvent };
    for (;;) {
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }
        if (waitResult != WAIT_OBJECT_0 + 1) {
            WriteLog(LOG_LEVEL_WARNING, "PreviewWorkerThread: request wait failed (result=%lu, error=%lu)",
                     waitResult, GetLastError());
            break;
        }

        if (!WaitForPreviewRequestQuiet(g_previewStopEvent, g_previewRequestEvent)) {
            break;
        }

        char requestedName[MAX_PATH] = {0};
        BOOL requestedFromPath = FALSE;
        LONG requestSerial = 0;

        AcquireSRWLockExclusive(&g_previewWorkerLock);

        requestSerial = InterlockedCompareExchange(&g_previewRequestSerial, 0, 0);
        if (IsAnimCriticalSectionReady()) {
            EnterCriticalSection(&g_animCriticalSection);
            CopyStringExactA(g_pendingPreviewName, requestedName, sizeof(requestedName));
            requestedFromPath = g_pendingPreviewFromPath;
            LeaveCriticalSection(&g_animCriticalSection);
        } else {
            CopyStringExactA(g_pendingPreviewName, requestedName, sizeof(requestedName));
            requestedFromPath = g_pendingPreviewFromPath;
        }

        if (!requestedName[0]) {
            ReleaseSRWLockExclusive(&g_previewWorkerLock);
            continue;
        }

        if (g_previewCancelEvent) {
            ResetEvent(g_previewCancelEvent);
        }

        ReleaseSRWLockExclusive(&g_previewWorkerLock);

        LoadedAnimation tempAnim;
        LoadedAnimation oldPreview;
        LoadedAnimation_Init(&tempAnim);
        LoadedAnimation_Init(&oldPreview);

        int cx = GetSystemMetrics(SM_CXSMICON);
        int cy = GetSystemMetrics(SM_CYSMICON);
        MemoryPool* localPool = AnimationNeedsDecodePool(requestedName)
            ? MemoryPool_Create(MEMORY_POOL_SIZE)
            : NULL;
        if (requestedFromPath) {
            LoadAnimationFromPathWithCancel(requestedName, &tempAnim, localPool, cx, cy,
                                            g_previewCancelEvent);
        } else {
            LoadAnimationByNameWithCancel(requestedName, &tempAnim, localPool, cx, cy,
                                          g_previewCancelEvent);
        }
        if (localPool) {
            MemoryPool_Destroy(localPool);
        }

        if (g_previewStopEvent && WaitForSingleObject(g_previewStopEvent, 0) == WAIT_OBJECT_0) {
            LoadedAnimation_Free(&tempAnim);
            break;
        }

        if (g_previewCancelEvent && WaitForSingleObject(g_previewCancelEvent, 0) == WAIT_OBJECT_0) {
            LoadedAnimation_Free(&tempAnim);
            continue;
        }

        if (!BeginTrayAnimationRuntimeUse()) {
            LoadedAnimation_Free(&oldPreview);
            LoadedAnimation_Free(&tempAnim);
            continue;
        }

        BOOL shouldApply = FALSE;
        if (IsAnimCriticalSectionReady()) {
            EnterCriticalSection(&g_animCriticalSection);
