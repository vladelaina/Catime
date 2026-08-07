/**
 * @file dialog_modern_control_keyboard.c
 * @brief Keyboard, cursor, enable, and teardown handling for controls.
 */

#include "dialog_modern_internal.h"

#define MODERN_RETURN_HANDLED(flag, value) \
    do { *(flag) = TRUE; return (value); } while (0)

LRESULT ModernHandleControlKeyboardMessage(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    ModernControl* control, ModernDialogState* state, BOOL* handled) {
    *handled = FALSE;
    switch (msg) {
        case WM_IME_STARTCOMPOSITION:
            if (ModernIsNativeEdit(control)) {
                ModernSetImeCompositionActive(hwnd, TRUE);
            }
            break;
        case WM_IME_COMPOSITION:
            if (ModernIsNativeEdit(control)) {
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                RedrawWindow(hwnd, NULL, NULL,
                             RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
                MODERN_RETURN_HANDLED(handled, result);
            }
            break;
        case WM_IME_ENDCOMPOSITION:
            if (ModernIsNativeEdit(control)) {
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                ModernSetImeCompositionActive(hwnd, FALSE);
                RedrawWindow(hwnd, NULL, NULL,
                             RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
                MODERN_RETURN_HANDLED(handled, result);
            }
            break;
        case WM_IME_SETCONTEXT:
            if (ModernIsNativeEdit(control) && !wParam) {
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                ModernSetImeCompositionActive(hwnd, FALSE);
                RedrawWindow(hwnd, NULL, NULL,
                             RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
                MODERN_RETURN_HANDLED(handled, result);
            }
            break;
        case WM_KEYDOWN:
            if (control && control->kind == MODERN_CONTROL_SLIDER &&
                IsWindowEnabled(hwnd) &&
                (wParam == VK_LEFT || wParam == VK_RIGHT ||
                 wParam == VK_UP || wParam == VK_DOWN ||
                 wParam == VK_HOME || wParam == VK_END ||
                 wParam == VK_PRIOR || wParam == VK_NEXT)) {
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                InvalidateRect(hwnd, NULL, FALSE);
                MODERN_RETURN_HANDLED(handled, result);
            }
            if (control && ModernIsDateTimeControl(control) &&
                IsWindowEnabled(hwnd)) {
                switch (wParam) {
                    case VK_LEFT:
                        ModernSelectDateTimePart(
                            control, control->dateTimeSelectedPart - 1);
                        MODERN_RETURN_HANDLED(handled, 0);
                    case VK_RIGHT:
                        ModernSelectDateTimePart(
                            control, control->dateTimeSelectedPart + 1);
                        MODERN_RETURN_HANDLED(handled, 0);
                    case VK_UP:
                        ModernAdjustDateTimePart(
                            control, control->dateTimeSelectedPart, 1);
                        MODERN_RETURN_HANDLED(handled, 0);
                    case VK_DOWN:
                        ModernAdjustDateTimePart(
                            control, control->dateTimeSelectedPart, -1);
                        MODERN_RETURN_HANDLED(handled, 0);
                    case VK_HOME:
                        ModernSelectDateTimePart(control, MODERN_DATETIME_HOUR);
                        MODERN_RETURN_HANDLED(handled, 0);
                    case VK_END:
                        ModernSelectDateTimePart(control, MODERN_DATETIME_SECOND);
                        MODERN_RETURN_HANDLED(handled, 0);
                }
            }
            if (control && ModernIsCompactEdit(control) &&
                wParam == VK_RETURN && state && state->hwnd) {
                LRESULT defaultId = SendMessageW(state->hwnd, DM_GETDEFID,
                                                 0, 0);
                if (HIWORD(defaultId) == DC_HASDEFID) {
                    int id = LOWORD(defaultId);
                    HWND button = GetDlgItem(state->hwnd, id);
                    if (button && IsWindowVisible(button) &&
                        IsWindowEnabled(button)) {
                        SendMessageW(state->hwnd, WM_COMMAND,
                                     MAKEWPARAM(id, BN_CLICKED),
                                     (LPARAM)button);
                    }
                }
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (wParam == VK_ESCAPE && state && state->hwnd) {
                if (control && control->kind == MODERN_CONTROL_COMBO &&
                    SendMessageW(hwnd, CB_GETDROPPEDSTATE, 0, 0)) {
                    break;
                }
                SendMessageW(state->hwnd, WM_CLOSE, 0, 0);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (state && state->bodyScrollMax96 > 0 &&
                !ModernControlOwnsVerticalScroll(control) &&
                (wParam == VK_PRIOR || wParam == VK_NEXT)) {
                int page = state->bodyViewportHeight96;
                int direction = wParam == VK_PRIOR ? -1 : 1;
                ModernSetBodyScrollOffset(
                    state,
                    state->bodyScrollOffset96 + direction * page);
                MODERN_RETURN_HANDLED(handled, 0);
            }
            break;
        case WM_CHAR:
            if (control && ModernIsDateTimeControl(control)) {
                if (wParam >= L'0' && wParam <= L'9') {
                    ModernInputDateTimeDigit(control, (int)(wParam - L'0'));
                }
                MODERN_RETURN_HANDLED(handled, 0);
            }
            if (control && ModernIsCompactEdit(control) &&
                (wParam == L'\r' || wParam == L'\n')) {
                MODERN_RETURN_HANDLED(handled, 0);
            }
            break;
        case WM_SETCURSOR:
            if (control && ModernIsDateTimeControl(control)) {
                LPCWSTR cursor = !IsWindowEnabled(hwnd)
                    ? IDC_ARROW
                    : (control->dateTimeHotPart == MODERN_DATETIME_STEP_UP ||
                       control->dateTimeHotPart == MODERN_DATETIME_STEP_DOWN
                           ? IDC_HAND : IDC_IBEAM);
                SetCursor(LoadCursorW(NULL, cursor));
                MODERN_RETURN_HANDLED(handled, TRUE);
            }
            if (control && (control->kind == MODERN_CONTROL_CLOSE ||
                            control->kind == MODERN_CONTROL_PUSH)) {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                MODERN_RETURN_HANDLED(handled, TRUE);
            }
            break;
        case WM_GETDLGCODE:
            if (control && ModernIsDateTimeControl(control)) {
                MODERN_RETURN_HANDLED(handled, DefSubclassProc(hwnd, msg, wParam, lParam) |
                       DLGC_WANTARROWS | DLGC_WANTCHARS);
            }
            break;
        case DTM_SETSYSTEMTIME: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (control && ModernIsDateTimeControl(control)) {
                ModernResetDateTimeInput(control);
                ModernHideDateTimeSpinner(control);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            MODERN_RETURN_HANDLED(handled, result);
        }
        case WM_ENABLE: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (control) {
                if (!wParam && ModernIsDateTimeControl(control)) {
                    ModernStopDateTimeRepeat(control);
                    control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
                }
                ModernHideDateTimeSpinner(control);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            MODERN_RETURN_HANDLED(handled, result);
        }
        case WM_NCDESTROY:
            ModernSetImeCompositionActive(hwnd, FALSE);
            if (control && ModernIsDateTimeControl(control)) {
                ModernStopDateTimeRepeat(control);
            }
            if (control && control->kind == MODERN_CONTROL_FEEDBACK) {
                RemovePropW(hwnd, MODERN_FEEDBACK_STATE_PROP);
            }
            RemoveWindowSubclass(hwnd, ModernControlSubclassProc,
                                 MODERN_CONTROL_SUBCLASS_ID);
            break;
    }
    return 0;
}

#undef MODERN_RETURN_HANDLED
