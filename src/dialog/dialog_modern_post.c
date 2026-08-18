/**
 * @file dialog_modern_post.c
 * @brief Body-wheel, hover, slider completion, and scrollbar refresh helpers.
 */

#include "dialog_modern_internal.h"

static void ModernStopBodyScrollTimer(ModernDialogState* state) {
    if (!state || !state->scrollUpdateTimerActive) return;
    KillTimer(state->hwnd, MODERN_SCROLL_DRAG_TIMER);
    state->scrollUpdateTimerActive = FALSE;
}

static void ModernCommitPendingBodyScroll(ModernDialogState* state) {
    if (!state || !state->scrollUpdatePending) return;
    int offset = state->scrollPendingOffset96;
    state->scrollUpdatePending = FALSE;
    ModernSetBodyScrollOffset(state, offset);
}

static void ModernQueueBodyScrollUpdate(ModernDialogState* state,
                                        int offset96) {
    if (!state) return;
    state->scrollPendingOffset96 = offset96;
    state->scrollUpdatePending = TRUE;
    if (!state->scrollUpdateTimerActive) {
        state->scrollUpdateTimerActive = SetTimer(
            state->hwnd, MODERN_SCROLL_DRAG_TIMER, 16, NULL) != 0;
        if (!state->scrollUpdateTimerActive) {
            ModernCommitPendingBodyScroll(state);
        }
    }
}

BOOL ModernHandleBodyWheel(ModernDialogState* state, WPARAM wParam) {
    if (!state || state->bodyScrollMax96 <= 0) return FALSE;
    state->bodyWheelDelta += GET_WHEEL_DELTA_WPARAM(wParam);
    int detents = state->bodyWheelDelta / WHEEL_DELTA;
    state->bodyWheelDelta -= detents * WHEEL_DELTA;
    if (detents != 0) {
        int offset = state->scrollUpdatePending
            ? state->scrollPendingOffset96
            : state->bodyScrollOffset96;
        ModernQueueBodyScrollUpdate(state, offset - detents * 48);
    }
    return TRUE;
}

BOOL ModernHandleBodyScrollTimer(ModernDialogState* state, WPARAM timerId) {
    if (!state || timerId != MODERN_SCROLL_DRAG_TIMER) return FALSE;
    ModernCommitPendingBodyScroll(state);
    if (!state->scrollBarDragging) {
        ModernStopBodyScrollTimer(state);
    }
    return TRUE;
}

void ModernBeginBodyScrollDrag(ModernDialogState* state, int pointerY) {
    if (!state) return;
    ModernCommitPendingBodyScroll(state);
    ModernStopBodyScrollTimer(state);
    state->scrollBarDragging = TRUE;
    state->scrollUpdatePending = FALSE;
    state->scrollUpdateTimerActive = SetTimer(
        state->hwnd, MODERN_SCROLL_DRAG_TIMER, 16, NULL) != 0;
    state->scrollDragStartY = pointerY;
    state->scrollDragStartOffset96 = state->bodyScrollOffset96;
}

void ModernQueueBodyScrollDrag(ModernDialogState* state, int offset96) {
    ModernQueueBodyScrollUpdate(state, offset96);
}

void ModernEndBodyScrollDrag(ModernDialogState* state) {
    if (!state) return;
    ModernCommitPendingBodyScroll(state);
    ModernStopBodyScrollTimer(state);
    state->scrollBarDragging = FALSE;
}

void ModernDiscardBodyScrollDrag(ModernDialogState* state) {
    if (!state) return;
    state->scrollUpdatePending = FALSE;
    ModernStopBodyScrollTimer(state);
    state->scrollBarDragging = FALSE;
}

void ModernRefreshControlHover(ModernControl* control) {
    if (!control || !control->hwnd) return;
    POINT point = {0};
    RECT windowRect = {0};
    BOOL hovered = GetCursorPos(&point) &&
                   GetWindowRect(control->hwnd, &windowRect) &&
                   PtInRect(&windowRect, point);
    control->hovered = hovered;
}

void ModernEndSliderDrag(ModernControl* control, BOOL commitPosition,
                                int x, int y) {
    if (!control || control->kind != MODERN_CONTROL_SLIDER ||
        !control->pressed || !control->hwnd) {
        return;
    }
    HWND hwnd = control->hwnd;
    if (commitPosition) {
        ModernSetSliderFromPoint(control, x, y, TB_THUMBPOSITION);
    }
    control->pressed = FALSE;
    if (GetCapture() == hwnd) ReleaseCapture();
    ModernRefreshControlHover(control);
    InvalidateRect(hwnd, NULL, FALSE);
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    UINT message = (style & TBS_VERT) ? WM_VSCROLL : WM_HSCROLL;
    SendMessageW(GetParent(hwnd), message, MAKEWPARAM(TB_ENDTRACK, 0),
                 (LPARAM)hwnd);
}

void ModernRefreshBodyScrollbarHover(ModernDialogState* state) {
    if (!state || !state->hwnd) return;
    POINT point = {0};
    RECT track = {0};
    RECT thumb = {0};
    BOOL hovered = GetCursorPos(&point) &&
                   ScreenToClient(state->hwnd, &point) &&
                   ModernGetScrollbarRects(state, &track, &thumb) &&
                   PtInRect(&track, point);
    state->scrollBarHovered = hovered;
    InvalidateRect(state->hwnd, NULL, FALSE);
}

LRESULT ModernHandleShowWindow(HWND hwnd, WPARAM wParam, LPARAM lParam,
                               ModernDialogState* state) {
    if (wParam && state && !state->finalized) {
        ModernFinalize(state);
    }

    LRESULT result = DefSubclassProc(hwnd, WM_SHOWWINDOW, wParam, lParam);
    if (wParam && state && ModernGetState(hwnd) == state &&
        state->finalized) {
        RedrawWindow(hwnd, NULL, NULL,
                     RDW_INVALIDATE | RDW_NOERASE | RDW_FRAME |
                         RDW_ALLCHILDREN | RDW_UPDATENOW);
        ModernReleaseFirstShowGuard(state);
    }
    return result;
}
