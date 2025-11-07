/**
 * @file tray_animation_timer.c
 * @brief High-precision timer implementation
 */

#include "tray/tray_animation_timer.h"
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

/* Timer constants */
#define TRAY_UPDATE_INTERVAL_MS 50
#define SYSTEM_TIMER_RESOLUTION_MS 1

/* Global timer state */
static MMRESULT g_mmTimerId = 0;
static BOOL g_useHighPrecisionTimer = FALSE;
static AnimationTimerCallback g_callback = NULL;
static void* g_userData = NULL;
static HWND g_timerHwnd = NULL;
static UINT g_internalInterval = 10;

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
    
    if (g_callback) {
        g_callback(g_userData);
    }
}

/**
 * @brief SetTimer fallback callback
 */
static void CALLBACK FallbackTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)hwnd; (void)msg; (void)id; (void)time;
    
    if (g_callback) {
        g_callback(g_userData);
    }
}

/**
 * @brief Initialize high-precision timer
 */
BOOL InitializeAnimationTimer(HWND hwnd, UINT internalIntervalMs,
                               AnimationTimerCallback callback, void* userData) {
    if (!hwnd || !callback) return FALSE;
    
    g_timerHwnd = hwnd;
    g_callback = callback;
    g_userData = userData;
    g_internalInterval = internalIntervalMs > 0 ? internalIntervalMs : 10;
    
    /* Try to set system timer resolution */
    MMRESULT mmRes = timeBeginPeriod(SYSTEM_TIMER_RESOLUTION_MS);
    if (mmRes != TIMERR_NOERROR) {
        mmRes = timeBeginPeriod(2);
        if (mmRes != TIMERR_NOERROR) {
            g_useHighPrecisionTimer = FALSE;
            
            /* Fallback to SetTimer */
            if (SetTimer(hwnd, 1, g_internalInterval, FallbackTimerProc) == 0) {
                return FALSE;
            }
            return TRUE;
        }
    }
    
    /* Create multimedia timer */
    g_mmTimerId = timeSetEvent(
        g_internalInterval,
        1,
        MMTimerCallback,
        0,
        TIME_PERIODIC | TIME_KILL_SYNCHRONOUS
    );
    
    if (g_mmTimerId == 0) {
        timeEndPeriod(SYSTEM_TIMER_RESOLUTION_MS);
        g_useHighPrecisionTimer = FALSE;
        
        /* Fallback to SetTimer */
        if (SetTimer(hwnd, 1, g_internalInterval, FallbackTimerProc) == 0) {
            return FALSE;
        }
        return TRUE;
    }
    
    g_useHighPrecisionTimer = TRUE;
    return TRUE;
}

/**
 * @brief Cleanup timer
 */
void CleanupAnimationTimer(void) {
    if (g_mmTimerId != 0) {
        timeKillEvent(g_mmTimerId);
        g_mmTimerId = 0;
    }
    
    if (g_timerHwnd) {
        KillTimer(g_timerHwnd, 1);
    }
    
    if (g_useHighPrecisionTimer) {
        timeEndPeriod(SYSTEM_TIMER_RESOLUTION_MS);
        g_useHighPrecisionTimer = FALSE;
    }
    
    g_callback = NULL;
    g_userData = NULL;
    g_timerHwnd = NULL;
}

/**
 * @brief Check timer type
 */
BOOL IsUsingHighPrecisionTimer(void) {
    return g_useHighPrecisionTimer;
}

