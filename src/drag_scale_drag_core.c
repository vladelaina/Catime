/**
 * @file drag_scale_drag_core.c
 * @brief Throttled absolute-position updates for edit-mode dragging.
 */

#include "drag_scale_internal.h"
#include "config.h"
#include "drawing/drawing_render.h"
#include "log.h"

void ResetDragApplyThrottle(void) {
    g_lastDragApplyTick = 0;
}

BOOL ShouldApplyDragMoveNow(DWORD now) {
    if (g_lastDragApplyTick == 0) {
        return TRUE;
    }
    return TickElapsedMs(now, g_lastDragApplyTick) >=
           EDIT_DRAG_APPLY_INTERVAL_MS;
}

BOOL ApplyDragPositionForCursor(HWND hwnd, POINT cursorPos) {
    if (!g_dragAnchorValid) {
        return FALSE;
    }

    int newX = g_dragStartWindowRect.left +
               (cursorPos.x - g_dragStartCursorPos.x);
    int newY = g_dragStartWindowRect.top +
               (cursorPos.y - g_dragStartCursorPos.y);

    RECT beforeRect = {0};
    BOOL alreadyAtTarget = GetWindowRect(hwnd, &beforeRect) &&
                           beforeRect.left == newX &&
                           beforeRect.top == newY;
    if (alreadyAtTarget) {
        CLOCK_LAST_MOUSE_POS = cursorPos;
        CLOCK_WINDOW_POS_X = newX;
        CLOCK_WINDOW_POS_Y = newY;
        RecordManualEditPosition(hwnd, newX, newY);
        return TRUE;
    }

    BOOL moved = SetWindowPos(hwnd, NULL, newX, newY, 0, 0,
                              SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (moved) {
        CLOCK_LAST_MOUSE_POS = cursorPos;
        CLOCK_WINDOW_POS_X = newX;
        CLOCK_WINDOW_POS_Y = newY;
        g_lastDragApplyTick = GetTickCount();
        RecordManualEditPosition(hwnd, newX, newY);
        return TRUE;
    }
    return FALSE;
}

void StopDragApplyTimer(HWND hwnd) {
    if (g_dragApplyTimer == 0) {
        return;
    }

    HWND timerHwnd = g_dragApplyTimerHwnd ? g_dragApplyTimerHwnd : hwnd;
    if ((!hwnd || timerHwnd == hwnd) && timerHwnd && IsWindow(timerHwnd)) {
        KillTimer(timerHwnd, EDIT_DRAG_APPLY_TIMER_ID);
    }

    if (!hwnd || timerHwnd == hwnd) {
        g_dragApplyTimer = 0;
        g_dragApplyTimerHwnd = NULL;
    }
}

static VOID CALLBACK DragApplyTimerProc(HWND hwnd, UINT msg,
                                        UINT_PTR idEvent, DWORD dwTime) {
    (void)dwTime;
    if (msg != WM_TIMER ||
        idEvent != EDIT_DRAG_APPLY_TIMER_ID ||
        hwnd != g_dragApplyTimerHwnd) {
        return;
    }

    StopDragApplyTimer(hwnd);
    if (!CLOCK_EDIT_MODE || !CLOCK_IS_DRAGGING ||
        !g_dragAnchorValid || GetCapture() != hwnd ||
        !IsLeftButtonPhysicallyDown()) {
        return;
    }

    POINT cursorPos = {0};
    if (GetCursorPos(&cursorPos)) {
        ApplyDragPositionForCursor(hwnd, cursorPos);
    }
}

BOOL EnsureDragApplyTimer(HWND hwnd) {
    if (!IsValidDragScaleWindow(hwnd)) {
        return FALSE;
    }
    if (g_dragApplyTimer != 0 && g_dragApplyTimerHwnd == hwnd) {
        return TRUE;
    }

    StopDragApplyTimer(NULL);
    g_dragApplyTimer = SetTimer(hwnd, EDIT_DRAG_APPLY_TIMER_ID,
                                EDIT_DRAG_APPLY_INTERVAL_MS,
                                (TIMERPROC)DragApplyTimerProc);
    if (!g_dragApplyTimer) {
        g_dragApplyTimerHwnd = NULL;
        return FALSE;
    }

    g_dragApplyTimerHwnd = hwnd;
    return TRUE;
}

void FinishDragWindow(HWND hwnd, BOOL saveSettings,
                      BOOL refreshAfterDrag, BOOL applyFinalPosition) {
    if (applyFinalPosition && CLOCK_IS_DRAGGING && g_dragAnchorValid &&
        IsValidDragScaleWindow(hwnd)) {
        POINT finalCursor = {0};
        if (GetCursorPos(&finalCursor)) {
            ApplyDragPositionForCursor(hwnd, finalCursor);
        }
    }
    StopDragApplyTimer(hwnd);

    CLOCK_IS_DRAGGING = FALSE;
    ClearDragAnchor();
    ResetDragApplyThrottle();
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }

    if (saveSettings && CLOCK_EDIT_MODE) {
        ScheduleConfigSave(hwnd);
    }
    if (refreshAfterDrag && IsValidDragScaleWindow(hwnd)) {
        RefreshWindow(hwnd, FALSE);
    }
}
