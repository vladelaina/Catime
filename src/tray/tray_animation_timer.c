/**
 * @file tray_animation_timer.c
 * @brief High-precision timer implementation
 */

#include "tray/tray_animation_timer.h"
#include <mmsystem.h>
#include "../../resource/resource.h"

#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#endif

/* Timer constants */
#define TRAY_UPDATE_INTERVAL_MS 50
#define TIMER_RESOLUTION_MIN_MS 1
#define TIMER_RESOLUTION_MAX_MS 10

/* Global timer state */
static MMRESULT g_mmTimerId = 0;
static BOOL g_useHighPrecisionTimer = FALSE;
static AnimationTimerCallback g_callback = NULL;
static void* g_userData = NULL;
static HWND g_timerHwnd = NULL;
static UINT_PTR g_fallbackTimerId = 0;
static volatile LONG g_fallbackTimerSerial = 0;
static UINT g_internalInterval = 10;
static UINT g_timerResolutionMs = 0;
static BOOL g_timerActive = FALSE;
static SRWLOCK g_timerLifecycleLock = SRWLOCK_INIT;
static volatile LONG g_acceptCallbacks = 0;
static volatile LONG g_activeCallbacks = 0;

#define TIMER_CALLBACK_DRAIN_SPIN_LIMIT 64

static void WaitForTimerCallbacksToDrain(void) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(&g_activeCallbacks, 0, 0) != 0) {
        Sleep(spins++ < TIMER_CALLBACK_DRAIN_SPIN_LIMIT ? 0 : 1);
    }
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

static void ResetTimerStateLocked(void) {
    g_mmTimerId = 0;
    g_useHighPrecisionTimer = FALSE;
    g_callback = NULL;
    g_userData = NULL;
    g_timerHwnd = NULL;
    g_fallbackTimerId = 0;
    g_timerResolutionMs = 0;
    g_timerActive = FALSE;
}

static void InvokeAnimationTimerCallback(void) {
    if (InterlockedCompareExchange(&g_acceptCallbacks, 0, 0) == 0) {
        return;
    }

    InterlockedIncrement(&g_activeCallbacks);
    if (InterlockedCompareExchange(&g_acceptCallbacks, 0, 0) == 0) {
        InterlockedDecrement(&g_activeCallbacks);
        return;
    }

    AnimationTimerCallback callback = g_callback;
    void* userData = g_userData;
    if (callback) {
        callback(userData);
    }

    InterlockedDecrement(&g_activeCallbacks);
}

static void CleanupAnimationTimerLocked(void) {
    InterlockedExchange(&g_acceptCallbacks, 0);

    MMRESULT mmTimerId = g_mmTimerId;
    HWND timerHwnd = g_timerHwnd;
    UINT_PTR fallbackTimerId = g_fallbackTimerId;
    UINT timerResolutionMs = g_timerResolutionMs;

    g_mmTimerId = 0;
    g_timerHwnd = NULL;
    g_fallbackTimerId = 0;
    g_timerResolutionMs = 0;
    g_useHighPrecisionTimer = FALSE;
    g_timerActive = FALSE;

    if (mmTimerId != 0) {
        timeKillEvent(mmTimerId);
    }

    if (timerHwnd && fallbackTimerId != 0) {
        KillTimer(timerHwnd, fallbackTimerId);
    }

    WaitForTimerCallbacksToDrain();

    if (timerResolutionMs > 0) {
        timeEndPeriod(timerResolutionMs);
    }

    g_callback = NULL;
    g_userData = NULL;
}

/**
 * @brief Initialize frame rate controller
 */
void FrameRateController_Init(FrameRateController* ctrl, UINT targetMs) {
    if (!ctrl) return;

    ctrl->targetInterval = targetMs > 0 ? targetMs : TRAY_UPDATE_INTERVAL_MS;
    ctrl->effectiveInterval = ctrl->targetInterval;
    ctrl->framePosition = 0.0;
    ctrl->internalAccumulator = 0;
    ctrl->lastUpdateTime = 0;
    ctrl->consecutiveLateUpdates = 0;
}

/**
 * @brief Check if frame should advance
 */
BOOL FrameRateController_ShouldAdvanceFrame(FrameRateController* ctrl,
                                            UINT deltaMs, UINT baseDelay) {
    if (!ctrl || baseDelay == 0) return FALSE;

    double frameAdvancement = (double)deltaMs / (double)baseDelay;
    ctrl->framePosition += frameAdvancement;

    if (ctrl->framePosition >= 1.0) {
        ctrl->framePosition -= 1.0;
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Record latency and adapt interval
 */
void FrameRateController_RecordLatency(FrameRateController* ctrl, UINT actualElapsed) {
    UNREFERENCED_PARAMETER(actualElapsed);
    if (!ctrl) return;

    DWORD currentTime = GetTickCount();

    if (ctrl->lastUpdateTime == 0) {
        ctrl->lastUpdateTime = currentTime;
        return;
    }

    DWORD realElapsed = currentTime - ctrl->lastUpdateTime;
    ctrl->lastUpdateTime = currentTime;

    /* Slow down if consistently late (>150% target for 3+ updates) */
    if (realElapsed > ctrl->effectiveInterval * 3 / 2) {
        ctrl->consecutiveLateUpdates++;

        if (ctrl->consecutiveLateUpdates >= 3) {
            if (ctrl->effectiveInterval < 200) {
                ctrl->effectiveInterval += 10;
            }
            ctrl->consecutiveLateUpdates = 0;
        }
    }
    /* Speed back up if performing well (<80% target) */
    else if (realElapsed < ctrl->effectiveInterval * 4 / 5) {
        ctrl->consecutiveLateUpdates = 0;

        if (ctrl->effectiveInterval > ctrl->targetInterval) {
            ctrl->effectiveInterval -= 2;
            if (ctrl->effectiveInterval < ctrl->targetInterval) {
                ctrl->effectiveInterval = ctrl->targetInterval;
            }
        }
    }
    else {
        ctrl->consecutiveLateUpdates = 0;
    }
}

/**
 * @brief Check if tray should update
 */
BOOL FrameRateController_ShouldUpdateTray(FrameRateController* ctrl) {
    if (!ctrl) return FALSE;

    ctrl->internalAccumulator += g_internalInterval;

    if (ctrl->internalAccumulator >= ctrl->effectiveInterval) {
        ctrl->internalAccumulator = 0;
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Multimedia timer callback (worker thread)
 */
static void CALLBACK MMTimerCallback(UINT uTimerID, UINT uMsg,
                                     DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    (void)uTimerID; (void)uMsg; (void)dwUser; (void)dw1; (void)dw2;

    InvokeAnimationTimerCallback();
}

/**
 * @brief SetTimer fallback callback
 */
static void CALLBACK FallbackTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)time;

    BOOL isCurrentTimer = FALSE;
    AcquireSRWLockShared(&g_timerLifecycleLock);
    isCurrentTimer = g_timerActive &&
                     !g_useHighPrecisionTimer &&
                     g_timerHwnd == hwnd &&
                     g_fallbackTimerId == id &&
                     msg == WM_TIMER &&
                     IsWindow(hwnd);
    ReleaseSRWLockShared(&g_timerLifecycleLock);

    if (!isCurrentTimer) {
        return;
    }

    InvokeAnimationTimerCallback();
}

static UINT_PTR NextFallbackTimerId(void) {
    LONG serial = InterlockedIncrement(&g_fallbackTimerSerial);
    return ((UINT_PTR)TIMER_ID_TRAY_ANIMATION << 16) |
           (UINT_PTR)(serial & 0xFFFF);
}

static BOOL StartFallbackAnimationTimerLocked(HWND hwnd) {
    UINT_PTR timerId = NextFallbackTimerId();
    if (timerId == 0) {
        timerId = TIMER_ID_TRAY_ANIMATION;
    }

    if (SetTimer(hwnd, timerId, g_internalInterval, FallbackTimerProc) == 0) {
        g_fallbackTimerId = 0;
        return FALSE;
    }

    g_fallbackTimerId = timerId;
    g_timerActive = TRUE;
    return TRUE;
}

/**
 * @brief Initialize high-precision timer
 */
BOOL InitializeAnimationTimer(HWND hwnd, UINT internalIntervalMs,
                               AnimationTimerCallback callback, void* userData) {
    if (!hwnd || !callback) return FALSE;

    AcquireSRWLockExclusive(&g_timerLifecycleLock);

    if (g_timerActive) {
        CleanupAnimationTimerLocked();
    }

    ResetTimerStateLocked();
    g_timerHwnd = hwnd;
    g_callback = callback;
    g_userData = userData;
    g_internalInterval = internalIntervalMs > 0 ? internalIntervalMs : 10;
    InterlockedExchange(&g_acceptCallbacks, 1);

    UINT requestedResolutionMs = ClampTimerResolutionToDeviceCaps(
        ChooseTimerResolutionMs(g_internalInterval));

    MMRESULT mmRes = timeBeginPeriod(requestedResolutionMs);
    if (mmRes != TIMERR_NOERROR) {
        g_useHighPrecisionTimer = FALSE;

        /* Fallback to SetTimer */
        if (!StartFallbackAnimationTimerLocked(hwnd)) {
            InterlockedExchange(&g_acceptCallbacks, 0);
            WaitForTimerCallbacksToDrain();
            ResetTimerStateLocked();
            ReleaseSRWLockExclusive(&g_timerLifecycleLock);
            return FALSE;
        }
        ReleaseSRWLockExclusive(&g_timerLifecycleLock);
        return TRUE;
    }

    g_timerResolutionMs = requestedResolutionMs;

    /* Create multimedia timer */
    g_mmTimerId = timeSetEvent(
        g_internalInterval,
        g_timerResolutionMs,
        MMTimerCallback,
        0,
        TIME_PERIODIC | TIME_KILL_SYNCHRONOUS
    );

    if (g_mmTimerId == 0) {
        if (g_timerResolutionMs > 0) {
            timeEndPeriod(g_timerResolutionMs);
            g_timerResolutionMs = 0;
        }
        g_useHighPrecisionTimer = FALSE;

        /* Fallback to SetTimer */
        if (!StartFallbackAnimationTimerLocked(hwnd)) {
            InterlockedExchange(&g_acceptCallbacks, 0);
            WaitForTimerCallbacksToDrain();
            ResetTimerStateLocked();
            ReleaseSRWLockExclusive(&g_timerLifecycleLock);
            return FALSE;
        }
        ReleaseSRWLockExclusive(&g_timerLifecycleLock);
        return TRUE;
    }

    g_useHighPrecisionTimer = TRUE;
    g_timerActive = TRUE;
    ReleaseSRWLockExclusive(&g_timerLifecycleLock);
    return TRUE;
}

/**
 * @brief Cleanup timer
 */
void CleanupAnimationTimer(void) {
    AcquireSRWLockExclusive(&g_timerLifecycleLock);
    CleanupAnimationTimerLocked();
    ReleaseSRWLockExclusive(&g_timerLifecycleLock);
}

BOOL IsAnimationTimerActive(void) {
    BOOL active = FALSE;
    AcquireSRWLockShared(&g_timerLifecycleLock);
    active = g_timerActive;
    ReleaseSRWLockShared(&g_timerLifecycleLock);
    return active;
}

/**
 * @brief Check timer type
 */
BOOL IsUsingHighPrecisionTimer(void) {
    BOOL useHighPrecisionTimer = FALSE;
    AcquireSRWLockShared(&g_timerLifecycleLock);
    useHighPrecisionTimer = g_useHighPrecisionTimer;
    ReleaseSRWLockShared(&g_timerLifecycleLock);
    return useHighPrecisionTimer;
}
