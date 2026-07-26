/**
 * @file dialog_modern_control_pointer.c
 * @brief Pointer, capture, repeat timer, and focus handling for controls.
 */

#include "dialog_modern_internal.h"

#define MODERN_RETURN_HANDLED(flag, value) \
    do { *(flag) = TRUE; return (value); } while (0)

LRESULT ModernHandleControlPointerMessage(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    ModernControl* control, ModernDialogState* state, BOOL* handled) {
    *handled = FALSE;
    switch (msg) {
        case WM_MOUSEMOVE:
            if (control && ModernIsDateTimeControl(control)) {
                ModernDateTimeLayout layout = {0};
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                int hit = ModernGetDateTimeLayout(control, &layout)
                    ? ModernDateTimeHitTest(&layout, point)
                    : MODERN_DATETIME_HIT_NONE;
                if (hit != control->dateTimeHotPart) {
                    control->dateTimeHotPart = hit;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            if (control && control->kind == MODERN_CONTROL_SLIDER &&
                control->pressed && GetCapture() == hwnd) {
                ModernSetSliderFromPoint(control, GET_X_LPARAM(lParam),
                                         GET_Y_LPARAM(lParam), TB_THUMBTRACK);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (control && !control->hovered) {
                control->hovered = TRUE;
                ModernTrackMouse(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_MOUSELEAVE:
            if (control && (control->hovered ||
                            control->dateTimeHotPart !=
                                MODERN_DATETIME_HIT_NONE)) {
                control->hovered = FALSE;
                control->dateTimeHotPart = MODERN_DATETIME_HIT_NONE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_LBUTTONDBLCLK:
            if (!control || !ModernIsDateTimeControl(control)) break;
            /* fall through */
        case WM_LBUTTONDOWN:
            if (control && control->kind == MODERN_CONTROL_GROUP) {
                ModernClearFocusedChild(state);
            }
            if (control && ModernIsDateTimeControl(control)) {
                if (!IsWindowEnabled(hwnd)) MODERN_RETURN_HANDLED(handled, 0);
                SetFocus(hwnd);
                ModernDateTimeLayout layout = {0};
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                int hit = ModernGetDateTimeLayout(control, &layout)
                    ? ModernDateTimeHitTest(&layout, point)
                    : MODERN_DATETIME_HIT_NONE;
                control->dateTimeHotPart = hit;
                if (hit >= MODERN_DATETIME_HOUR &&
                    hit <= MODERN_DATETIME_SECOND) {
                    ModernSelectDateTimePart(control, hit);
                } else if (hit == MODERN_DATETIME_STEP_UP ||
                           hit == MODERN_DATETIME_STEP_DOWN) {
                    control->dateTimePressedPart = hit;
                    SetCapture(hwnd);
                    ModernAdjustDateTimePart(
                        control, control->dateTimeSelectedPart,
                        hit == MODERN_DATETIME_STEP_UP ? 1 : -1);
                    ModernStartDateTimeRepeat(control);
                }
                InvalidateRect(hwnd, NULL, FALSE);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (control) {
                control->pressed = TRUE;
                if (control->kind == MODERN_CONTROL_SLIDER) {
                    SetFocus(hwnd);
                    SetCapture(hwnd);
                    ModernSetSliderFromPoint(control, GET_X_LPARAM(lParam),
                                             GET_Y_LPARAM(lParam),
                                             TB_THUMBTRACK);
                    InvalidateRect(hwnd, NULL, FALSE);
                    MODERN_RETURN_HANDLED(handled, 0);
                }
            }
            break;
        case WM_LBUTTONUP:
            if (control && ModernIsDateTimeControl(control)) {
                ModernStopDateTimeRepeat(control);
                control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
                if (GetCapture() == hwnd) ReleaseCapture();
                InvalidateRect(hwnd, NULL, FALSE);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (control && control->kind == MODERN_CONTROL_SLIDER &&
                control->pressed) {
                ModernEndSliderDrag(control, TRUE, GET_X_LPARAM(lParam),
                                    GET_Y_LPARAM(lParam));
                MODERN_RETURN_HANDLED(handled, 0);
            }
            /* fall through */
        case WM_CAPTURECHANGED:
            if (control) {
                if (control->kind == MODERN_CONTROL_SLIDER &&
                    control->pressed && (HWND)lParam != hwnd) {
                    ModernEndSliderDrag(control, FALSE, 0, 0);
                    MODERN_RETURN_HANDLED(handled, 0);
                }
                if (ModernIsDateTimeControl(control)) {
                    ModernStopDateTimeRepeat(control);
                    control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
                }
                control->pressed = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_CANCELMODE:
            if (control && control->kind == MODERN_CONTROL_SLIDER &&
                control->pressed) {
                ModernEndSliderDrag(control, FALSE, 0, 0);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (control && ModernIsDateTimeControl(control)) {
                ModernStopDateTimeRepeat(control);
                control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
                if (GetCapture() == hwnd) ReleaseCapture();
                InvalidateRect(hwnd, NULL, FALSE);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            break;
        case WM_TIMER:
            if (control && ModernIsDateTimeControl(control) &&
                wParam == MODERN_DATETIME_REPEAT_TIMER) {
                if (!control->dateTimeRepeatStarted) {
                    control->dateTimeRepeatStarted = TRUE;
                    SetTimer(hwnd, MODERN_DATETIME_REPEAT_TIMER, 70, NULL);
                }
                if (GetCapture() == hwnd &&
                    control->dateTimeHotPart == control->dateTimePressedPart &&
                    (control->dateTimePressedPart == MODERN_DATETIME_STEP_UP ||
                     control->dateTimePressedPart == MODERN_DATETIME_STEP_DOWN)) {
                    ModernAdjustDateTimePart(
                        control, control->dateTimeSelectedPart,
                        control->dateTimePressedPart == MODERN_DATETIME_STEP_UP
                            ? 1 : -1);
                }
                MODERN_RETURN_HANDLED(handled, 0);
            }
            break;
        case WM_SETFOCUS:
            if (control) {
                ModernEnsureControlVisible(control);
                control->focused = TRUE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_KILLFOCUS:
            if (control) {
                control->focused = FALSE;
                if (ModernIsDateTimeControl(control)) {
                    ModernResetDateTimeInput(control);
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
    }
    return 0;
}

#undef MODERN_RETURN_HANDLED
