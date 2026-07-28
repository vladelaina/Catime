/**
 * @file drag_scale_drag_core.c
 * @brief Immediate absolute-position updates for edit-mode dragging.
 */

#include "drag_scale_internal.h"
#include "config.h"
#include "drawing/drawing_render.h"
#include "log.h"

BOOL ApplyDragPositionForCursor(HWND hwnd, POINT cursorPos) {
    if (!g_dragAnchorValid) {
        return FALSE;
    }

    int newX = g_dragStartWindowRect.left +
               (cursorPos.x - g_dragStartCursorPos.x);
    int newY = g_dragStartWindowRect.top +
               (cursorPos.y - g_dragStartCursorPos.y);

    BOOL moved = SetWindowPos(hwnd, NULL, newX, newY, 0, 0,
                              SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (moved) {
        CLOCK_LAST_MOUSE_POS = cursorPos;
        CLOCK_WINDOW_POS_X = newX;
        CLOCK_WINDOW_POS_Y = newY;
        RecordManualEditPosition(hwnd, newX, newY);
        return TRUE;
    }
    return FALSE;
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
    CLOCK_IS_DRAGGING = FALSE;
    ClearDragAnchor();
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
