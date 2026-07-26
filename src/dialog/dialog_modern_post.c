/**
 * @file dialog_modern_post.c
 * @brief Body-wheel, hover, slider completion, and scrollbar refresh helpers.
 */

#include "dialog_modern_internal.h"

BOOL ModernHandleBodyWheel(ModernDialogState* state, WPARAM wParam) {
    if (!state || state->bodyScrollMax96 <= 0) return FALSE;
    state->bodyWheelDelta += GET_WHEEL_DELTA_WPARAM(wParam);
    int detents = state->bodyWheelDelta / WHEEL_DELTA;
    state->bodyWheelDelta -= detents * WHEEL_DELTA;
    if (detents != 0) {
        ModernSetBodyScrollOffset(
            state, state->bodyScrollOffset96 - detents * 48);
    }
    return TRUE;
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
