#include "timer/main_timer.h"
#include "timer/main_timer_internal.h"
#include "../../resource/resource.h"
#include "log.h"
#include <mmsystem.h>
#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#endif
static MMRESULT g_mainTimerId = 0;
static HWND g_mainHwnd = NULL;
static UINT g_timerInterval = 20;
static BOOL g_highPrecisionActive = FALSE;
static BOOL g_setTimerActive = FALSE;
static UINT g_timerResolutionMs = 0;
static volatile LONG g_tickMessagePending = 0;
static volatile LONG g_acceptTimerCallbacks = 0;
static volatile LONG g_activeTimerCallbacks = 0;
static volatile LONG g_timerGeneration = 1;
static SRWLOCK g_mainTimerLock = SRWLOCK_INIT;
#define TIMER_RESOLUTION_MIN_MS 1
#define TIMER_RESOLUTION_MAX_MS 10
#define NormalizeInterval(value) ((value) > 0 ? (value) : 20)
static void CALLBACK MainTimerCallback(UINT uTimerID, UINT uMsg,
                                       DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    (void)uTimerID; (void)uMsg; (void)dwUser; (void)dw1; (void)dw2;
    if (InterlockedCompareExchange(&g_acceptTimerCallbacks, 0, 0) == 0) return;
    InterlockedIncrement(&g_activeTimerCallbacks);
    if (InterlockedCompareExchange(&g_acceptTimerCallbacks, 0, 0) == 0) {
        InterlockedDecrement(&g_activeTimerCallbacks);
        return;
    }
    HWND hwnd = g_mainHwnd;
    LONG generation = (LONG)dwUser;
    LONG currentGeneration = InterlockedCompareExchange(&g_timerGeneration, 0, 0);
    if (generation != currentGeneration) {
        InterlockedDecrement(&g_activeTimerCallbacks);
        return;
    }
    if (!MainTimer_IsValidWindow(hwnd)) {
        InterlockedDecrement(&g_activeTimerCallbacks);
        return;
    }
    if (InterlockedCompareExchange(&g_tickMessagePending, 1, 0) != 0) {
        InterlockedDecrement(&g_activeTimerCallbacks);
        return;
    }
    if (!PostMessage(hwnd, CLOCK_WM_MAIN_TIMER_TICK, (WPARAM)generation, 0)) {
        InterlockedExchange(&g_tickMessagePending, 0);
    }
    InterlockedDecrement(&g_activeTimerCallbacks);
}
static UINT ChooseTimerResolutionMs(UINT intervalMs) {
    UINT resolution = intervalMs / 2;
    if (resolution < TIMER_RESOLUTION_MIN_MS) {
        resolution = TIMER_RESOLUTION_MIN_MS;
    }
    if (resolution > TIMER_RESOLUTION_MAX_MS) {
        resolution = TIMER_RESOLUTION_MAX_MS;
    }
    return resolution;
}
static UINT ClampTimerResolutionToDeviceCaps(UINT requestedMs) {
    TIMECAPS caps;
    if (timeGetDevCaps(&caps, sizeof(caps)) != TIMERR_NOERROR) {
        return requestedMs;
    }
    if (requestedMs < caps.wPeriodMin) {
        return caps.wPeriodMin;
    }
    if (requestedMs > caps.wPeriodMax) {
        return caps.wPeriodMax;
    }
    return requestedMs;
}
static BOOL StartSetTimerFallback(void) {
    if (!MainTimer_IsValidWindow(g_mainHwnd)) {
        LOG_WARNING("MainTimer: cannot start SetTimer fallback without a valid window");
        return FALSE;
    }
    InterlockedExchange(&g_acceptTimerCallbacks, 0);
    KillTimer(g_mainHwnd, TIMER_ID_MAIN);
    g_setTimerActive = SetTimer(g_mainHwnd, TIMER_ID_MAIN, g_timerInterval, NULL) != 0;
    if (!g_setTimerActive) {
        LOG_WARNING("MainTimer: SetTimer fallback failed (interval=%u, error=%lu)",
                    g_timerInterval, GetLastError());
    }
    return g_setTimerActive;
}
static BOOL StartMultimediaTimer(void) {
    LONG generation = InterlockedIncrement(&g_timerGeneration);
    InterlockedExchange(&g_acceptTimerCallbacks, 1);
    MMRESULT timerId = timeSetEvent(
        g_timerInterval,
        g_timerResolutionMs > 0 ? g_timerResolutionMs : TIMER_RESOLUTION_MAX_MS,
        MainTimerCallback,
        (DWORD_PTR)generation,
        TIME_PERIODIC | TIME_KILL_SYNCHRONOUS
    );
    if (timerId == 0) {
        LOG_WARNING("MainTimer: timeSetEvent failed (interval=%u)", g_timerInterval);
        InterlockedExchange(&g_acceptTimerCallbacks, 0);
        MainTimer_WaitForCallbacks(&g_activeTimerCallbacks);
        return FALSE;
    }
    g_mainTimerId = timerId;
    g_setTimerActive = FALSE;
    return TRUE;
}
static BOOL ShouldUseHighPrecision(UINT intervalMs) {
    return intervalMs <= 33;
}
static BOOL AcquireTimerResolution(void) {
    UINT requestedResolutionMs = ClampTimerResolutionToDeviceCaps(
        ChooseTimerResolutionMs(g_timerInterval));
    if (g_timerResolutionMs == requestedResolutionMs) {
        return TRUE;
    }
    if (g_timerResolutionMs > 0) {
        timeEndPeriod(g_timerResolutionMs);
        g_timerResolutionMs = 0;
    }
    MMRESULT res = timeBeginPeriod(requestedResolutionMs);
    if (res == TIMERR_NOERROR) {
        g_timerResolutionMs = requestedResolutionMs;
        return TRUE;
    }
    LOG_WARNING("MainTimer: timeBeginPeriod failed for high-precision timer (resolution=%u)",
                requestedResolutionMs);
    return FALSE;
}
static void ReleaseTimerResolution(void) {
    if (g_timerResolutionMs > 0) {
        timeEndPeriod(g_timerResolutionMs);
        g_timerResolutionMs = 0;
    }
}
static void StopTimerBackendLocked(void) {
    InterlockedExchange(&g_acceptTimerCallbacks, 0);
    InterlockedIncrement(&g_timerGeneration);
    MMRESULT timerId = g_mainTimerId;
    HWND hwnd = g_mainHwnd;
    g_mainTimerId = 0;
    g_highPrecisionActive = FALSE;
    if (timerId != 0) {
        timeKillEvent(timerId);
    }
    MainTimer_WaitForCallbacks(&g_activeTimerCallbacks);
    ReleaseTimerResolution();
    if (hwnd) {
        KillTimer(hwnd, TIMER_ID_MAIN);
    }
    g_setTimerActive = FALSE;
    InterlockedExchange(&g_tickMessagePending, 0);
}
static BOOL StartConfiguredTimerLocked(void) {
    if (!g_mainHwnd) return FALSE;
    if (!ShouldUseHighPrecision(g_timerInterval)) {
        g_highPrecisionActive = FALSE;
        return StartSetTimerFallback();
    }
    if (StartMultimediaTimer()) {
        g_highPrecisionActive = TRUE;
        KillTimer(g_mainHwnd, TIMER_ID_MAIN);
        return TRUE;
    }
    ReleaseTimerResolution();
    g_highPrecisionActive = FALSE;
    return StartSetTimerFallback();
}
static BOOL RestartTimerBackendLocked(void) {
    StopTimerBackendLocked();
    if (ShouldUseHighPrecision(g_timerInterval) && !AcquireTimerResolution()) {
        g_highPrecisionActive = FALSE;
        return StartSetTimerFallback();
    }
    return StartConfiguredTimerLocked();
}
static BOOL RestartTimerBackendWithRollbackLocked(UINT oldInterval,
                                                  BOOL restoreOldIntervalOnFailure) {
    UINT requestedInterval = g_timerInterval;
    BOOL result = RestartTimerBackendLocked();
    if (result || !restoreOldIntervalOnFailure || oldInterval == requestedInterval) {
        return result;
    }
    g_timerInterval = oldInterval;
    if (RestartTimerBackendLocked()) {
        LOG_WARNING("MainTimer: kept previous interval %u after restart to %u failed",
                    oldInterval, requestedInterval);
    } else {
        LOG_WARNING("MainTimer: failed to restore previous interval %u after restart to %u failed",
                    oldInterval, requestedInterval);
    }
    return FALSE;
}
static BOOL InitTimerLocked(HWND hwnd, UINT intervalMs) {
    StopTimerBackendLocked();
    g_highPrecisionActive = FALSE;
    g_mainHwnd = hwnd;
    g_timerInterval = NormalizeInterval(intervalMs);
    return RestartTimerBackendLocked();
}
static BOOL IsConfiguredTimerRunningLocked(void) {
    if (g_highPrecisionActive) {
        return g_mainTimerId != 0;
    }
    return g_setTimerActive;
}
BOOL MainTimer_Init(HWND hwnd, UINT intervalMs) {
    if (!MainTimer_IsValidWindow(hwnd)) return FALSE;
    AcquireSRWLockExclusive(&g_mainTimerLock);
    BOOL result = InitTimerLocked(hwnd, intervalMs);
    ReleaseSRWLockExclusive(&g_mainTimerLock);
    return result;
}
BOOL MainTimer_Start(HWND hwnd, UINT intervalMs) {
    if (!MainTimer_IsValidWindow(hwnd)) return FALSE;
    AcquireSRWLockExclusive(&g_mainTimerLock);
    BOOL result = FALSE;
    UINT normalized = NormalizeInterval(intervalMs);
    if (!g_mainHwnd || g_mainHwnd != hwnd) {
        result = InitTimerLocked(hwnd, normalized);
    } else if (normalized == g_timerInterval && IsConfiguredTimerRunningLocked()) {
        result = TRUE;
    } else {
        UINT oldInterval = g_timerInterval;
        BOOL wasRunning = IsConfiguredTimerRunningLocked();
        g_timerInterval = normalized;
        result = RestartTimerBackendWithRollbackLocked(oldInterval, wasRunning);
    }
    ReleaseSRWLockExclusive(&g_mainTimerLock);
    return result;
}
void MainTimer_Stop(void) {
    AcquireSRWLockExclusive(&g_mainTimerLock);
    StopTimerBackendLocked();
    ReleaseSRWLockExclusive(&g_mainTimerLock);
}
void MainTimer_NotifyTickHandled(LONG generation) {
    LONG currentGeneration = InterlockedCompareExchange(&g_timerGeneration, 0, 0);
    if (generation == currentGeneration) {
        InterlockedExchange(&g_tickMessagePending, 0);
    }
}
BOOL MainTimer_ShouldHandleTick(LONG generation) {
    BOOL shouldHandle = FALSE;
    AcquireSRWLockShared(&g_mainTimerLock);
    LONG currentGeneration = InterlockedCompareExchange(&g_timerGeneration, 0, 0);
    shouldHandle = generation == currentGeneration &&
                   g_highPrecisionActive &&
                   g_mainTimerId != 0 &&
                   InterlockedCompareExchange(&g_acceptTimerCallbacks, 0, 0) != 0;
    ReleaseSRWLockShared(&g_mainTimerLock);
    return shouldHandle;
}
void MainTimer_SetInterval(UINT intervalMs) {
    UINT normalized = NormalizeInterval(intervalMs);
    AcquireSRWLockExclusive(&g_mainTimerLock);
    if (normalized == g_timerInterval) {
        if (g_mainHwnd && !IsConfiguredTimerRunningLocked()) {
            if (!RestartTimerBackendLocked()) {
                LOG_WARNING("MainTimer: failed to restart stopped timer (interval=%u)",
                            g_timerInterval);
            }
        }
        ReleaseSRWLockExclusive(&g_mainTimerLock);
        return;
    }
    UINT oldInterval = g_timerInterval;
    BOOL wasRunning = IsConfiguredTimerRunningLocked();
    g_timerInterval = normalized;
    if (g_mainHwnd) {
        if (!RestartTimerBackendWithRollbackLocked(oldInterval, wasRunning)) {
            LOG_WARNING("MainTimer: failed to apply interval change (interval=%u)",
                        normalized);
        }
    }
    ReleaseSRWLockExclusive(&g_mainTimerLock);
}
void MainTimer_Cleanup(void) {
    AcquireSRWLockExclusive(&g_mainTimerLock);
    StopTimerBackendLocked();
    g_highPrecisionActive = FALSE;
    g_mainHwnd = NULL;
    ReleaseSRWLockExclusive(&g_mainTimerLock);
}
BOOL MainTimer_IsHighPrecision(void) {
    BOOL highPrecision = FALSE;
    AcquireSRWLockShared(&g_mainTimerLock);
    highPrecision = g_highPrecisionActive;
    ReleaseSRWLockShared(&g_mainTimerLock);
    return highPrecision;
}
BOOL MainTimer_IsRunning(void) {
    BOOL running = FALSE;
    AcquireSRWLockShared(&g_mainTimerLock);
    running = IsConfiguredTimerRunningLocked();
    ReleaseSRWLockShared(&g_mainTimerLock);
    return running;
}
