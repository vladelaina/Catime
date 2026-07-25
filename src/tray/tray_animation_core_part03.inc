        g_previewStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!g_previewStopEvent) {
            CloseHandle(g_previewRequestEvent);
            g_previewRequestEvent = NULL;
            WriteLog(LOG_LEVEL_ERROR, "Failed to create preview stop event");
            MarkPreviewWorkerStartFailure(now);
            return FALSE;
        }
    }

    if (!g_previewCancelEvent) {
        g_previewCancelEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!g_previewCancelEvent) {
            CloseHandle(g_previewRequestEvent);
            CloseHandle(g_previewStopEvent);
            g_previewRequestEvent = NULL;
            g_previewStopEvent = NULL;
            WriteLog(LOG_LEVEL_ERROR, "Failed to create preview cancel event");
            MarkPreviewWorkerStartFailure(now);
            return FALSE;
        }
    }

    ResetEvent(g_previewStopEvent);
    ResetEvent(g_previewCancelEvent);
    g_previewWorkerThread = CreateThread(NULL, 0, PreviewWorkerThread, NULL, 0, NULL);
    if (!g_previewWorkerThread) {
        CloseHandle(g_previewRequestEvent);
        CloseHandle(g_previewStopEvent);
        CloseHandle(g_previewCancelEvent);
        g_previewRequestEvent = NULL;
        g_previewStopEvent = NULL;
        g_previewCancelEvent = NULL;
        WriteLog(LOG_LEVEL_ERROR, "Failed to create preview worker thread");
        MarkPreviewWorkerStartFailure(now);
        return FALSE;
    }

    g_previewWorkerRetiring = FALSE;
    g_previewWorkerStartFailureCooldownUntil = 0;
    return TRUE;
}

static BOOL IsPreviewWorkerRetiringAfterCleanup(void) {
    BOOL retiring = FALSE;

    AcquireSRWLockExclusive(&g_previewWorkerLock);
    CleanupCompletedPreviewWorkerLocked();
    retiring = g_previewWorkerRetiring;
    ReleaseSRWLockExclusive(&g_previewWorkerLock);

    return retiring;
}

static BOOL WaitForPreviewRequestQuiet(HANDLE stopEvent, HANDLE requestEvent) {
    HANDLE waitHandles[2] = { stopEvent, requestEvent };

    for (;;) {
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, PREVIEW_REQUEST_DEBOUNCE_MS);
        if (waitResult == WAIT_OBJECT_0) {
            return FALSE;
        }
        if (waitResult == WAIT_OBJECT_0 + 1) {
            continue;
        }
        if (waitResult == WAIT_TIMEOUT) {
            return TRUE;
        }

        WriteLog(LOG_LEVEL_WARNING, "PreviewWorkerThread: debounce wait failed (error=%lu)", GetLastError());
        return TRUE;
    }
}

static BOOL ShutdownPreviewWorker(void) {
    HANDLE workerThread = NULL;
    BOOL stopped = TRUE;

    AcquireSRWLockExclusive(&g_previewWorkerLock);

    CleanupCompletedPreviewWorkerLocked();
    if (g_previewWorkerRetiring) {
        ReleaseSRWLockExclusive(&g_previewWorkerLock);
        return FALSE;
    }

    if (g_previewStopEvent) {
        SetEvent(g_previewStopEvent);
    }

    SignalPreviewDecodeCancelLocked();
    WakePreviewWorkerLocked();

    workerThread = g_previewWorkerThread;
    ReleaseSRWLockExclusive(&g_previewWorkerLock);

    if (workerThread) {
        DWORD waitResult = WaitForSingleObject(workerThread, PREVIEW_WORKER_SHUTDOWN_WAIT_MS);
        if (waitResult != WAIT_OBJECT_0) {
            WriteLog(LOG_LEVEL_WARNING,
                     "Preview worker did not stop within %lu ms (wait=%lu, error=%lu)",
                     (DWORD)PREVIEW_WORKER_SHUTDOWN_WAIT_MS, waitResult, GetLastError());
            stopped = FALSE;
        }
    }

    if (!stopped) {
        AcquireSRWLockExclusive(&g_previewWorkerLock);
        if (g_previewWorkerThread == workerThread) {
            g_previewWorkerRetiring = TRUE;
            CleanupCompletedPreviewWorkerLocked();
        }
        stopped = (g_previewWorkerThread != workerThread);
        ReleaseSRWLockExclusive(&g_previewWorkerLock);
        return stopped;
    }

    AcquireSRWLockExclusive(&g_previewWorkerLock);

    if (g_previewWorkerThread) {
        CloseHandle(g_previewWorkerThread);
        g_previewWorkerThread = NULL;
    }
    g_previewWorkerRetiring = FALSE;

    if (g_previewRequestEvent) {
        CloseHandle(g_previewRequestEvent);
        g_previewRequestEvent = NULL;
    }

    if (g_previewStopEvent) {
        CloseHandle(g_previewStopEvent);
        g_previewStopEvent = NULL;
    }

    if (g_previewCancelEvent) {
        CloseHandle(g_previewCancelEvent);
        g_previewCancelEvent = NULL;
    }

    ReleaseSRWLockExclusive(&g_previewWorkerLock);
    return TRUE;
}

static void PostPreviewLoadedMessage(void) {
    if (!IsTrayAnimationRuntimeActive()) return;
    HWND trayHwnd = GetValidTrayAnimationWindow();
    if (!trayHwnd) return;

    if (ClaimPendingTrayUpdate()) {
        return;
    }

    if (!PostMessage(trayHwnd, CLOCK_WM_ANIMATION_PREVIEW_LOADED, 0, 0)) {
        ClearPendingTrayUpdate();
        MarkTrayFrameDirty();
    }
}

/**
 * @brief Normalize config value (trim whitespace and quotes)
 */
static void NormalizeAnimConfigValue(char* s) {
    if (!s) return;
    char* p = s;
    while (*p && (isspace((unsigned char)*p) || *p == '"' || *p == '\'')) p++;
    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }
    size_t len = strlen(s);
    while (len > 0 && (isspace((unsigned char)s[len - 1]) || s[len - 1] == '"' || s[len - 1] == '\'')) {
        s[--len] = '\0';
    }
}

/**
 * @brief Record successful update
 */
static void RecordSuccessfulUpdate(void) {
    g_consecutiveUpdateFailures = 0;
    g_lastSuccessfulUpdateTime = GetTickCount();
    g_updateFailureReported = FALSE;
    g_lastPreviewShellFailureLogTick = 0;
    InterlockedExchange(&g_shellUpdateBackoffActive, 0);
    InterlockedExchange(&g_lastFailedUpdateAttemptTick, 0);
}

/**
 * @brief Record failed update
 * @return TRUE if repeated failures should trigger recovery handling
 */
static BOOL RecordFailedUpdate(void) {
    DWORD currentTime = GetTickCount();
    InterlockedExchange(&g_lastFailedUpdateAttemptTick, (LONG)currentTime);
    if (g_consecutiveUpdateFailures < UINT_MAX) {
        g_consecutiveUpdateFailures++;
    }
    if (g_updateFailureReported) {
        return FALSE;
    }

    if (g_consecutiveUpdateFailures >= MAX_CONSECUTIVE_FAILURES) {
        g_updateFailureReported = TRUE;
        InterlockedExchange(&g_shellUpdateBackoffActive, 1);
        return TRUE;
    }

    if (g_lastSuccessfulUpdateTime > 0) {
        DWORD elapsed = currentTime - g_lastSuccessfulUpdateTime;
        if (elapsed > UPDATE_TIMEOUT_MS) {
            g_updateFailureReported = TRUE;
            InterlockedExchange(&g_shellUpdateBackoffActive, 1);
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief Keep the selected animation alive across transient Shell failures
 */
static void HandleRepeatedTrayUpdateFailure(void) {
    WriteLog(LOG_LEVEL_WARNING,
             "Tray icon update is temporarily unavailable; retaining the selected animation and retrying");
}

static void LogPreviewShellFailureThrottled(void) {
    DWORD now = GetTickCount();
    if (g_lastPreviewShellFailureLogTick == 0 ||
        (DWORD)(now - g_lastPreviewShellFailureLogTick) >= UPDATE_TIMEOUT_MS) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Tray preview icon update is temporarily unavailable");
        g_lastPreviewShellFailureLogTick = now ? now : 1u;
    }
}

/**
 * @brief Compute scaled delay based on CPU/memory/timer
 */
static double ComputeAnimationSpeedScalePercent(AnimationSpeedMetric metric) {
    if (metric == ANIMATION_SPEED_ORIGINAL) {
        return 100.0;
    }
    if (metric == ANIMATION_SPEED_FIXED) {
        return GetAnimationFixedSpeedMultiplier() * 100.0;
    }

    double percent = 0.0;

    if (metric == ANIMATION_SPEED_CPU) {
        float cpu = 0.0f;
        SystemMonitor_GetCpuUsage(&cpu);
        percent = cpu;
    } else if (metric == ANIMATION_SPEED_TIMER) {
        if (!CLOCK_SHOW_CURRENT_TIME && !CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0) {
            double p = (double)countdown_elapsed_time / (double)CLOCK_TOTAL_TIME;
            if (p < 0.0) p = 0.0;
            if (p > 1.0) p = 1.0;
            percent = p * 100.0;
        }
    } else {
        float mem = 0.0f;
