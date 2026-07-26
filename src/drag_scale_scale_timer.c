/**
 * @file drag_scale_scale_timer.c
 * @brief Adaptive timer for smoothly applying scale targets.
 */

#include "drag_scale_internal.h"
#include "config.h"

static VOID CALLBACK ScaleApplyTimerProc(HWND hwnd, UINT msg,
                                         UINT_PTR idEvent, DWORD dwTime);

static void RefreshScaleApplyTimerInterval(HWND hwnd) {
    if (g_scaleApplyTimer == 0 || g_scaleApplyTimerHwnd != hwnd) {
        return;
    }
    UINT desiredInterval = GetScaleApplyInterval(hwnd);
    if (desiredInterval == g_scaleApplyIntervalMs) {
        return;
    }

    UINT_PTR updatedTimer = SetTimer(hwnd, SCALE_APPLY_TIMER_ID,
                                     desiredInterval,
                                     (TIMERPROC)ScaleApplyTimerProc);
    if (updatedTimer != 0) {
        g_scaleApplyTimer = updatedTimer;
        g_scaleApplyIntervalMs = desiredInterval;
    }
}

static VOID CALLBACK ScaleApplyTimerProc(HWND hwnd, UINT msg,
                                         UINT_PTR idEvent, DWORD dwTime) {
    (void)dwTime;

    if (msg != WM_TIMER ||
        idEvent != SCALE_APPLY_TIMER_ID ||
        hwnd != g_scaleApplyTimerHwnd) {
        return;
    }

    DWORD now = GetTickCount();
    DWORD elapsedMs = g_lastScaleApplyTick != 0
        ? TickElapsedMs(now, g_lastScaleApplyTick)
        : SCALE_APPLY_INTERVAL_MS;
    g_lastScaleApplyTick = now;
    BOOL targetReached = ApplySmoothedScaleTarget(hwnd, elapsedMs);
    if (!targetReached) {
        RefreshScaleApplyTimerInterval(hwnd);
    }
    BOOL inputIdle = TickElapsedMs(now, g_lastScaleWheelTick) >=
                     SCALE_APPLY_IDLE_STOP_MS;
    if (!g_scaleTargetValid ||
        !CLOCK_EDIT_MODE ||
        (inputIdle && targetReached)) {
        if (g_scaleTargetValid && CLOCK_EDIT_MODE &&
            inputIdle && targetReached) {
            ScheduleConfigSave(hwnd);
        }
        StopScaleApplyTimer(hwnd);
    }
}

BOOL EnsureScaleApplyTimer(HWND hwnd) {
    if (!IsValidDragScaleWindow(hwnd)) {
        return FALSE;
    }
    if (g_scaleApplyTimer != 0 && g_scaleApplyTimerHwnd == hwnd) {
        return TRUE;
    }

    StopScaleApplyTimer(hwnd);
    UINT interval = GetScaleApplyInterval(hwnd);
    g_scaleApplyTimer = SetTimer(hwnd, SCALE_APPLY_TIMER_ID,
                                 interval,
                                 (TIMERPROC)ScaleApplyTimerProc);
    if (!g_scaleApplyTimer) {
        return FALSE;
    }

    g_scaleApplyTimerHwnd = hwnd;
    g_scaleApplyIntervalMs = interval;
    g_lastScaleApplyTick = GetTickCount();
    AdvanceScaleGestureSerial();
    return TRUE;
}
