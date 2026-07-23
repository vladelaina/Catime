/**
 * @file plugin_exit.c
 * @brief Plugin exit countdown management
 *
 * Handles <exit>N</exit> tag parsing and countdown display.
 * When countdown completes, sends message to stop plugin.
 */

#include "plugin/plugin_exit.h"
#include "plugin_exit_internal.h"
#include "../resource/resource.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

/* ============================================================================
 * State
 * ============================================================================ */

volatile LONG g_exitInProgress = FALSE;
SRWLOCK g_exitLock = SRWLOCK_INIT;
CONDITION_VARIABLE g_exitStateChanged = CONDITION_VARIABLE_INIT;
BOOL g_exitThreadStopInProgress = FALSE;
HANDLE g_exitThread = NULL;
HANDLE g_exitStopEvent = NULL;

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define EXIT_COUNTDOWN_CANCEL_WAIT_MS 2000
#define EXIT_COUNTDOWN_SHUTDOWN_WAIT_MS 2000
#define EXIT_COUNTDOWN_START_FAILURE_COOLDOWN_MS 2000

/* Template for countdown display */
wchar_t* g_exitPrefix = NULL;
wchar_t* g_exitSuffix = NULL;
DWORD g_exitStartFailureCooldownUntil = 0;

/* ============================================================================
 * Exit Countdown Thread
 * ============================================================================ */

BOOL IsExitInProgress(void) {
    return InterlockedCompareExchange(&g_exitInProgress, FALSE, FALSE) != FALSE;
}

BOOL IsExitStartFailureCoolingDown(DWORD now) {
    return g_exitStartFailureCooldownUntil != 0 &&
           (LONG)(g_exitStartFailureCooldownUntil - now) > 0;
}

void MarkExitStartFailure(DWORD now) {
    DWORD cooldownUntil = now + EXIT_COUNTDOWN_START_FAILURE_COOLDOWN_MS;
    g_exitStartFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
}

void CleanupCompletedExitThreadHandleLocked(void) {
    HANDLE threadHandle = g_exitThread;
    if (!threadHandle) return;
    if (g_exitThreadStopInProgress) return;

    if (WaitForSingleObject(threadHandle, 0) == WAIT_OBJECT_0) {
        g_exitThread = NULL;
        CloseHandle(threadHandle);
        WakeAllConditionVariable(&g_exitStateChanged);
    }
}

void FreeExitTemplatesLocked(void) {
    if (g_exitPrefix) {
        free(g_exitPrefix);
        g_exitPrefix = NULL;
    }
    if (g_exitSuffix) {
        free(g_exitSuffix);
        g_exitSuffix = NULL;
    }
}

BOOL EnsureExitStopEventLocked(void) {
    if (!g_exitStopEvent) {
        g_exitStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!g_exitStopEvent) {
            LOG_ERROR("PluginExit: Failed to create stop event");
            return FALSE;
        }
    }

    ResetEvent(g_exitStopEvent);
    return TRUE;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

BOOL PluginExit_Init(HWND hwnd, CRITICAL_SECTION* dataCS) {
    AcquireSRWLockExclusive(&g_exitLock);
    CleanupCompletedExitThreadHandleLocked();
    if (g_exitThread || g_exitThreadStopInProgress) {
        InterlockedExchange(&g_exitInProgress, FALSE);
        if (g_exitStopEvent) {
            SetEvent(g_exitStopEvent);
        }
        LOG_WARNING("PluginExit: Init deferred because a previous countdown thread is still retiring");
        ReleaseSRWLockExclusive(&g_exitLock);
        return FALSE;
    }

    g_notifyWnd = hwnd;
    g_dataCS = dataCS;
    InterlockedExchange(&g_exitInProgress, FALSE);
    g_exitThreadStopInProgress = FALSE;
    g_exitStartFailureCooldownUntil = 0;
    FreeExitTemplatesLocked();
    if (!EnsureExitStopEventLocked()) {
        MarkExitStartFailure(GetTickCount());
        g_notifyWnd = NULL;
        g_dataCS = NULL;
        ReleaseSRWLockExclusive(&g_exitLock);
        return FALSE;
    }
    WakeAllConditionVariable(&g_exitStateChanged);
    ReleaseSRWLockExclusive(&g_exitLock);
    return TRUE;
}

static BOOL PluginExit_CancelWithTimeout(DWORD waitMs);

BOOL PluginExit_Shutdown(void) {
    BOOL stopped = PluginExit_CancelWithTimeout(EXIT_COUNTDOWN_SHUTDOWN_WAIT_MS);
    if (!stopped) {
        LOG_WARNING("PluginExit: Countdown thread did not stop before shutdown timeout");
        return FALSE;
    }
    AcquireSRWLockExclusive(&g_exitLock);
    if (g_exitStopEvent) {
        CloseHandle(g_exitStopEvent);
        g_exitStopEvent = NULL;
    }
    g_notifyWnd = NULL;
    g_dataCS = NULL;
    InterlockedExchange(&g_exitInProgress, FALSE);
    ReleaseSRWLockExclusive(&g_exitLock);
    return TRUE;
}

BOOL PluginExit_IsInProgress(void) {
    AcquireSRWLockExclusive(&g_exitLock);
    CleanupCompletedExitThreadHandleLocked();
    BOOL inProgress = IsExitInProgress();
    ReleaseSRWLockExclusive(&g_exitLock);
    return inProgress;
}


static BOOL PluginExit_CancelWithTimeout(DWORD waitMs) {
    HANDLE waitThread = NULL;
    BOOL ownsThreadStop = FALSE;
    BOOL stopped = TRUE;
    DWORD waitStart = GetTickCount();

    AcquireSRWLockExclusive(&g_exitLock);
    InterlockedExchange(&g_exitInProgress, FALSE);
    if (g_exitStopEvent) {
        SetEvent(g_exitStopEvent);
    }

    while (g_exitThreadStopInProgress) {
        DWORD elapsed = GetTickCount() - waitStart;
        DWORD remaining = elapsed >= waitMs ? 0 : waitMs - elapsed;
        if (remaining == 0 ||
            !SleepConditionVariableSRW(&g_exitStateChanged, &g_exitLock, remaining, 0)) {
            LOG_WARNING("PluginExit: Waiting for concurrent countdown stop timed out after %lu ms",
                        waitMs);
            ReleaseSRWLockExclusive(&g_exitLock);
            return FALSE;
        }
    }

    CleanupCompletedExitThreadHandleLocked();
    if (g_exitThread) {
        waitThread = g_exitThread;
        g_exitThreadStopInProgress = TRUE;
        ownsThreadStop = TRUE;
    }
    ReleaseSRWLockExclusive(&g_exitLock);

    if (waitThread) {
        DWORD waitResult = WaitForSingleObject(waitThread, waitMs);
        stopped = (waitResult == WAIT_OBJECT_0);
        if (!stopped) {
            if (waitResult == WAIT_TIMEOUT) {
                LOG_WARNING("PluginExit: Countdown cancel timed out after %lu ms", waitMs);
            } else {
                LOG_WARNING("PluginExit: Countdown cancel wait failed (wait=%lu, error=%lu)",
                            waitResult, GetLastError());
            }
        }
    }

    AcquireSRWLockExclusive(&g_exitLock);
    if (ownsThreadStop && stopped) {
        if (g_exitThread) {
            CloseHandle(g_exitThread);
            g_exitThread = NULL;
        }
        g_exitThreadStopInProgress = FALSE;
        WakeAllConditionVariable(&g_exitStateChanged);
    }

    if (ownsThreadStop && !stopped) {
        g_exitThreadStopInProgress = FALSE;
        WakeAllConditionVariable(&g_exitStateChanged);
        ReleaseSRWLockExclusive(&g_exitLock);
        return FALSE;
    }

    if (g_exitStopEvent) {
        ResetEvent(g_exitStopEvent);
    }

    FreeExitTemplatesLocked();
    ReleaseSRWLockExclusive(&g_exitLock);
    return TRUE;
}

void PluginExit_Cancel(void) {
    (void)PluginExit_CancelWithTimeout(EXIT_COUNTDOWN_CANCEL_WAIT_MS);
}
