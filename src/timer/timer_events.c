/**
 * @file timer_events.c
 * @brief Timer event dispatcher and lightweight retry handlers.
 */

#include "timer_events_internal.h"

BOOL TimerEvents_HandleRetryTimer(HWND hwnd,
                                  UINT timerId,
                                  int* retryCount,
                                  TimerEvents_RetrySetupCallback callback) {
    if (!retryCount) {
        return FALSE;
    }
    if (*retryCount == 0) {
        *retryCount = MAX_RETRY_ATTEMPTS;
    }

    if (callback) {
        callback(hwnd);
    }

    (*retryCount)--;
    if (*retryCount > 0) {
        if (!SetTimer(hwnd, timerId, RETRY_INTERVAL_MS, NULL)) {
            LOG_WARNING("Retry timer %u failed to reschedule (error: %lu)",
                        timerId, GetLastError());
        }
    } else {
        KillTimer(hwnd, timerId);
    }
    return TRUE;
}

void TimerEvents_SetupTopmostWindow(HWND hwnd) {
    if (CLOCK_WINDOW_TOPMOST) {
        EnsureWindowVisibleWithTopmostState(hwnd);
    }
}

void TimerEvents_SetupVisibilityWindow(HWND hwnd) {
    if (!CLOCK_WINDOW_TOPMOST) {
        EnsureWindowVisibleWithTopmostState(hwnd);
    }
}

BOOL TimerEvents_HandleFontValidation(HWND hwnd) {
    if (CheckAndReloadCurrentFontPath()) {
        InvalidateRect(hwnd, NULL, TRUE);
    }

    if (!SetTimer(hwnd, TIMER_ID_FONT_VALIDATION,
                  FONT_CHECK_INTERVAL_MS, NULL)) {
        LOG_WARNING("Font validation timer failed to reschedule (error: %lu)",
                    GetLastError());
    }
    return TRUE;
}

BOOL TimerEvents_HandleForceRedraw(HWND hwnd) {
    EnsureWindowVisibleWithTopmostState(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
    RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
    return TRUE;
}

BOOL HandleTimerEvent(HWND hwnd, WPARAM wp) {
    static int topmostRetry = 0;
    static int visibilityRetry = 0;

    switch (wp) {
        case TIMER_ID_TOPMOST_RETRY:
            return TimerEvents_HandleRetryTimer(
                hwnd, TIMER_ID_TOPMOST_RETRY, &topmostRetry,
                TimerEvents_SetupTopmostWindow);

        case TIMER_ID_VISIBILITY_RETRY:
            return TimerEvents_HandleRetryTimer(
                hwnd, TIMER_ID_VISIBILITY_RETRY, &visibilityRetry,
                TimerEvents_SetupVisibilityWindow);

        case TIMER_ID_TOPMOST_APPLY_RETRY:
            return HandleTopmostApplyRetry(hwnd);

        case TIMER_ID_TOPMOST_VISIBILITY_RESTORE:
            if (CLOCK_IS_DRAGGING) {
                SetTimer(hwnd, TIMER_ID_TOPMOST_VISIBILITY_RESTORE,
                         100, NULL);
                return TRUE;
            }
            KillTimer(hwnd, TIMER_ID_TOPMOST_VISIBILITY_RESTORE);
            return HandleTopmostVisibilityChange(hwnd, NULL);

        case TIMER_ID_FORCE_REDRAW:
            return HandleDrawingRenderRetryTimer(hwnd);

        case TIMER_ID_EDIT_MODE_REFRESH:
            if (CLOCK_IS_DRAGGING) {
                SetTimer(hwnd, TIMER_ID_EDIT_MODE_REFRESH, 100, NULL);
                return TRUE;
            }
            KillTimer(hwnd, TIMER_ID_EDIT_MODE_REFRESH);
            return TimerEvents_HandleForceRedraw(hwnd);

        case TIMER_ID_FONT_VALIDATION:
            return TimerEvents_HandleFontValidation(hwnd);

        case TIMER_ID_MAIN:
            if (!MainTimer_IsRunning()) {
                return TRUE;
            }
            return TimerEvents_HandleMainTimer(hwnd);

        case TIMER_ID_RENDER_ANIMATION:
            /* Render ticks only request a frame; state advances elsewhere. */
            TimerEvents_RequestWindowRepaint(hwnd);
            return TRUE;

        default:
            return FALSE;
    }
}
