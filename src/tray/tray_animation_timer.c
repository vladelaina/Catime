/**
 * @file tray_animation_timer.c
 * @brief High-precision timer implementation
 */

#include "tray/tray_animation_timer.h"
#include "utils/finite_double.h"
#include <mmsystem.h>
#include <math.h>
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
static volatile LONG g_timerGeneration = 0;

#define TIMER_CALLBACK_DRAIN_SPIN_LIMIT 64
#define TIMER_CALLBACK_DRAIN_TIMEOUT_MS 2000

static BOOL WaitForTimerCallbacksToDrain(void) {
    DWORD spins = 0;
    ULONGLONG startedAt = GetTickCount64();
    while (InterlockedCompareExchange(&g_activeCallbacks, 0, 0) != 0) {
        if (GetTickCount64() - startedAt >= TIMER_CALLBACK_DRAIN_TIMEOUT_MS) {
            return FALSE;
        }
        Sleep(spins++ < TIMER_CALLBACK_DRAIN_SPIN_LIMIT ? 0 : 1);
    }
    return TRUE;
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

static void InvokeAnimationTimerCallback(LONG expectedGeneration) {
    InterlockedIncrement(&g_activeCallbacks);
    if (InterlockedCompareExchange(&g_acceptCallbacks, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_timerGeneration, 0, 0) != expectedGeneration) {
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

static BOOL CleanupAnimationTimerLocked(void) {
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

    BOOL callbacksDrained = WaitForTimerCallbacksToDrain();

    if (timerResolutionMs > 0) {
        timeEndPeriod(timerResolutionMs);
    }

    if (callbacksDrained) {
        g_callback = NULL;
        g_userData = NULL;
    }
    return callbacksDrained;
}

/**
 * @brief Initialize frame rate controller
 */
void FrameRateController_Init(FrameRateController* ctrl, UINT targetMs) {
    if (!ctrl) return;

    ctrl->targetInterval = targetMs > 0 ? targetMs : TRAY_UPDATE_INTERVAL_MS;
    ctrl->trayAccumulatorMs = 0.0;
}

/**
 * @brief Check if tray should update
 */
BOOL FrameRateController_ShouldUpdateTray(FrameRateController* ctrl, double elapsedMs) {
    if (!ctrl || ctrl->targetInterval == 0 ||
        !DoubleIsFiniteStrict(elapsedMs) || elapsedMs <= 0.0) {
        return FALSE;
    }

    ctrl->trayAccumulatorMs += elapsedMs;
    if (!DoubleIsFiniteStrict(ctrl->trayAccumulatorMs) ||
        ctrl->trayAccumulatorMs < 0.0) {
        ctrl->trayAccumulatorMs = 0.0;
        return FALSE;
    }

    if (ctrl->trayAccumulatorMs >= (double)ctrl->targetInterval) {
        ctrl->trayAccumulatorMs = fmod(ctrl->trayAccumulatorMs,
                                       (double)ctrl->targetInterval);
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

    InvokeAnimationTimerCallback((LONG)dwUser);
}

/**
 * @brief SetTimer fallback callback
 */
static void CALLBACK FallbackTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)time;

    BOOL isCurrentTimer = FALSE;
    LONG timerGeneration = 0;
    AcquireSRWLockShared(&g_timerLifecycleLock);
    isCurrentTimer = g_timerActive &&
                     !g_useHighPrecisionTimer &&
                     g_timerHwnd == hwnd &&
                     g_fallbackTimerId == id &&
                     msg == WM_TIMER &&
                     IsWindow(hwnd);
    timerGeneration = InterlockedCompareExchange(&g_timerGeneration, 0, 0);
    ReleaseSRWLockShared(&g_timerLifecycleLock);

    if (!isCurrentTimer) {
        return;
    }

    InvokeAnimationTimerCallback(timerGeneration);
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
        if (!CleanupAnimationTimerLocked()) {
            ReleaseSRWLockExclusive(&g_timerLifecycleLock);
            return FALSE;
        }
    } else if (InterlockedCompareExchange(&g_activeCallbacks, 0, 0) != 0) {
        ReleaseSRWLockExclusive(&g_timerLifecycleLock);
        return FALSE;
    }

    ResetTimerStateLocked();
    g_timerHwnd = hwnd;
    g_callback = callback;
    g_userData = userData;
    g_internalInterval = internalIntervalMs > 0 ? internalIntervalMs : 10;
    LONG timerGeneration = InterlockedIncrement(&g_timerGeneration);
    if (timerGeneration == 0) {
        timerGeneration = InterlockedIncrement(&g_timerGeneration);
    }
    InterlockedExchange(&g_acceptCallbacks, 1);

    UINT requestedResolutionMs = ClampTimerResolutionToDeviceCaps(
        ChooseTimerResolutionMs(g_internalInterval));

    MMRESULT mmRes = timeBeginPeriod(requestedResolutionMs);
    if (mmRes != TIMERR_NOERROR) {
        g_useHighPrecisionTimer = FALSE;

        /* Fallback to SetTimer */
        if (!StartFallbackAnimationTimerLocked(hwnd)) {
            InterlockedExchange(&g_acceptCallbacks, 0);
            (void)WaitForTimerCallbacksToDrain();
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
        (DWORD_PTR)(ULONG)timerGeneration,
        TIME_PERIODIC
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
            (void)WaitForTimerCallbacksToDrain();
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
BOOL CleanupAnimationTimer(void) {
    AcquireSRWLockExclusive(&g_timerLifecycleLock);
    BOOL callbacksDrained = CleanupAnimationTimerLocked();
    ReleaseSRWLockExclusive(&g_timerLifecycleLock);
    return callbacksDrained;
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
