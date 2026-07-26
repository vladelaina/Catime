/**
 * @file tray_animation_preview_worker.c
 * @brief Preview worker event and thread lifecycle management.
 */

#include "tray_animation_core_internal.h"

void SwapLoadedAnimation(LoadedAnimation* target, const LoadedAnimation* source) {
    if (!target || !source) return;
    *target = *source;
}

void CleanupCompletedPreviewWorkerLocked(void) {
    if (!g_previewWorkerThread) {
        return;
    }

    DWORD waitResult = WaitForSingleObject(g_previewWorkerThread, 0);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_FAILED) {
            WriteLog(LOG_LEVEL_WARNING, "Preview worker status check failed (error=%lu)", GetLastError());
        }
        return;
    }

    CloseHandle(g_previewWorkerThread);
    g_previewWorkerThread = NULL;
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
}

void CleanupRetiredPreviewWorkerOnExit(void) {
    AcquireSRWLockExclusive(&g_previewWorkerLock);
    if (g_previewWorkerRetiring) {
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
    }
    ReleaseSRWLockExclusive(&g_previewWorkerLock);
}

void SignalPreviewDecodeCancelLocked(void) {
    if (g_previewCancelEvent) {
        SetEvent(g_previewCancelEvent);
    }
}

void WakePreviewWorkerLocked(void) {
    if (g_previewRequestEvent) {
        SetEvent(g_previewRequestEvent);
    }
}

BOOL IsPreviewWorkerStartFailureCoolingDown(DWORD now) {
    return g_previewWorkerStartFailureCooldownUntil != 0 &&
           (LONG)(g_previewWorkerStartFailureCooldownUntil - now) > 0;
}

void MarkPreviewWorkerStartFailure(DWORD now) {
    DWORD cooldownUntil = now + PREVIEW_WORKER_START_RETRY_COOLDOWN_MS;
    g_previewWorkerStartFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
}

BOOL EnsurePreviewWorkerStartedLocked(void) {
    CleanupCompletedPreviewWorkerLocked();

    if (g_previewWorkerThread) {
        if (g_previewWorkerRetiring) {
            return FALSE;
        }
        return TRUE;
    }

    DWORD now = GetTickCount();
    if (IsPreviewWorkerStartFailureCoolingDown(now)) {
        return FALSE;
    }

    if (!g_previewRequestEvent) {
        g_previewRequestEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!g_previewRequestEvent) {
            WriteLog(LOG_LEVEL_ERROR, "Failed to create preview request event");
            MarkPreviewWorkerStartFailure(now);
            return FALSE;
        }
    }

    if (!g_previewStopEvent) {
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

BOOL IsPreviewWorkerRetiringAfterCleanup(void) {
    BOOL retiring = FALSE;

    AcquireSRWLockExclusive(&g_previewWorkerLock);
    CleanupCompletedPreviewWorkerLocked();
    retiring = g_previewWorkerRetiring;
    ReleaseSRWLockExclusive(&g_previewWorkerLock);

    return retiring;
}

BOOL WaitForPreviewRequestQuiet(HANDLE stopEvent, HANDLE requestEvent) {
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

BOOL ShutdownPreviewWorker(void) {
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

void PostPreviewLoadedMessage(void) {
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
