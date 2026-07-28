/**
 * @file drag_scale_drag_api.c
 * @brief Public edit-mode drag entry points and button-state handling.
 */

#include "drag_scale_internal.h"
#include "config.h"
#include "drawing/drawing_render.h"
#include "log.h"

void StartDragWindow(HWND hwnd) {
    if (!CLOCK_EDIT_MODE || CLOCK_IS_DRAGGING) {
        return;
    }
    if (IsDragBlockedUntilLeftUp()) {
        return;
    }
    if (IsScaleWindowGestureActive(hwnd) || IsDragSuppressedAfterScale()) {
        return;
    }

    SetCapture(hwnd);
    if (GetCapture() != hwnd) {
        LOG_WARNING("Failed to capture mouse for edit-mode drag");
        return;
    }

    POINT cursorPos;
    if (!GetCursorPos(&cursorPos) ||
        !SetDragAnchorFromCurrentWindow(hwnd, cursorPos)) {
        ReleaseCapture();
        LOG_WARNING("Failed to initialize edit-mode drag anchor");
        return;
    }

    /* A manual drag supersedes a short-lived scale resize anchor. */
    if (g_pendingScaleResizeAnchorValid) {
        LOG_DEBUG("Discarding stale scale resize anchor before manual drag");
    }
    ForceClearPendingScaleResizeAnchor();

    CLOCK_IS_DRAGGING = TRUE;
    CLOCK_LAST_MOUSE_POS = cursorPos;
    StopDrawingRenderAnimationTimer(hwnd);
}

BOOL TryStartDragWindowFromMouseMove(HWND hwnd) {
    if (!CLOCK_EDIT_MODE || CLOCK_IS_DRAGGING) {
        return FALSE;
    }
    if (!IsLeftButtonPhysicallyDown()) {
        IsDragBlockedUntilLeftUp();
        return FALSE;
    }
    if (GetCapture() && GetCapture() != hwnd) {
        return FALSE;
    }
    if (IsDragBlockedUntilLeftUp()) {
        return FALSE;
    }
    if (IsScaleWindowGestureActive(hwnd) || IsDragSuppressedAfterScale()) {
        return FALSE;
    }

    StartDragWindow(hwnd);
    return CLOCK_IS_DRAGGING;
}

void EndDragWindow(HWND hwnd) {
    if (!IsLeftButtonPhysicallyDown()) {
        ClearDragBlockUntilLeftUp();
    }
    if (!CLOCK_IS_DRAGGING) {
        return;
    }
    FinishDragWindow(hwnd, TRUE, TRUE, TRUE);
}

void CancelDragForScale(HWND hwnd) {
    BlockDragUntilLeftUp(hwnd);
    if (!CLOCK_IS_DRAGGING) {
        return;
    }
    FinishDragWindow(hwnd, FALSE, FALSE, FALSE);
}

/* Absolute cursor anchoring keeps movement aligned when mouse messages coalesce. */
static BOOL HandleDragWindowInternal(HWND hwnd, BOOL leftButtonDown) {
    if (!CLOCK_EDIT_MODE || !CLOCK_IS_DRAGGING) {
        return FALSE;
    }
    if (IsDragBlockedUntilLeftUp()) {
        FinishDragWindow(hwnd, TRUE, TRUE, TRUE);
        return FALSE;
    }
    if (IsScaleWindowGestureActive(hwnd) || IsDragSuppressedAfterScale()) {
        FinishDragWindow(hwnd, FALSE, FALSE, FALSE);
        return FALSE;
    }
    if (!leftButtonDown) {
        FinishDragWindow(hwnd, TRUE, TRUE, TRUE);
        return FALSE;
    }
    if (GetCapture() != hwnd) {
        FinishDragWindow(hwnd, TRUE, TRUE, TRUE);
        return FALSE;
    }

    POINT currentPos;
    if (!GetCursorPos(&currentPos)) {
        return TRUE;
    }
    if (!g_dragAnchorValid &&
        !SetDragAnchorFromCurrentWindow(hwnd, CLOCK_LAST_MOUSE_POS)) {
        return TRUE;
    }

    int deltaFromLastX = currentPos.x - CLOCK_LAST_MOUSE_POS.x;
    int deltaFromLastY = currentPos.y - CLOCK_LAST_MOUSE_POS.y;
    if (deltaFromLastX == 0 && deltaFromLastY == 0) {
        return TRUE;
    }
    ApplyDragPositionForCursor(hwnd, currentPos);
    return TRUE;
}

BOOL HandleDragWindowWithButtonState(HWND hwnd, BOOL leftButtonDown) {
    return HandleDragWindowInternal(hwnd, leftButtonDown);
}

BOOL HandleDragWindow(HWND hwnd) {
    return HandleDragWindowInternal(hwnd, IsLeftButtonPhysicallyDown());
}
