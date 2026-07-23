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
                return 0;
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
        case WM_ERASEBKGND:
            if (control && (control->kind == MODERN_CONTROL_SLIDER ||
                            ModernIsDateTimeControl(control))) {
                return 1;
            }
            break;
        case WM_PAINT:
            if (control && (control->kind == MODERN_CONTROL_CHECK ||
                            control->kind == MODERN_CONTROL_RADIO ||
                            control->kind == MODERN_CONTROL_GROUP)) {
                ModernPaintChoiceControl(control, NULL);
                return 0;
            }
            if (control && state && ModernIsDateTimeControl(control)) {
                ModernPaintDateTime(control, NULL);
                return 0;
            }
            if (control && control->kind == MODERN_CONTROL_COMBO) {
                ModernPaintCombo(control, NULL);
                ModernDrawFieldOutline(control);
                return 0;
            }
            if (control && control->kind == MODERN_CONTROL_SLIDER) {
                ModernPaintSlider(control, NULL);
                return 0;
            }
            if (control && (control->kind == MODERN_CONTROL_FIELD ||
                            control->kind == MODERN_CONTROL_LIST ||
                            control->kind == MODERN_CONTROL_COMBO)) {
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                ModernDrawFieldOutline(control);
                return result;
            }
            break;
        case WM_PRINTCLIENT:
            if (control && (control->kind == MODERN_CONTROL_CHECK ||
                            control->kind == MODERN_CONTROL_RADIO ||
                            control->kind == MODERN_CONTROL_GROUP)) {
                ModernPaintChoiceControl(control, (HDC)wParam);
                return 0;
            }
            if (control && state && ModernIsDateTimeControl(control)) {
                ModernPaintDateTime(control, (HDC)wParam);
                return 0;
            }
            if (control && control->kind == MODERN_CONTROL_COMBO) {
                ModernPaintCombo(control, (HDC)wParam);
                return 0;
            }
            if (control && control->kind == MODERN_CONTROL_SLIDER) {
                ModernPaintSlider(control, (HDC)wParam);
                return 0;
            }
            break;
        case WM_SIZE: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            ModernApplyFieldRegion(control);
            ModernApplyEditLayout(control);
            ModernHideDateTimeSpinner(control);
            return result;
        }
        case WM_MOUSEWHEEL:
            if (state && ModernHandleInteractiveWheel(state, wParam, lParam)) {
                return 0;
            }
            if (state && state->bodyScrollMax96 > 0 &&
                !ModernControlOwnsVerticalScroll(control)) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int step = DialogModern_Scale(state->dpi, 48);
                int amount = MulDiv(delta, step, WHEEL_DELTA);
                if (amount == 0 && delta != 0) {
                    amount = delta > 0 ? step : -step;
                }
                ModernSetBodyScrollOffset(
                    state,
                    state->bodyScrollOffset96 - amount);
                return 0;
            }
            break;
        case WM_PASTE:
            if (control && ModernIsCompactEdit(control) &&
                ModernPasteCompactEdit(hwnd)) {
                return 0;
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
                return result;
            }
            if (control && ModernIsDateTimeControl(control) &&
                IsWindowEnabled(hwnd)) {
                switch (wParam) {
                    case VK_LEFT:
                        ModernSelectDateTimePart(
                            control, control->dateTimeSelectedPart - 1);
                        return 0;
                    case VK_RIGHT:
                        ModernSelectDateTimePart(
                            control, control->dateTimeSelectedPart + 1);
                        return 0;
                    case VK_UP:
                        ModernAdjustDateTimePart(
                            control, control->dateTimeSelectedPart, 1);
                        return 0;
                    case VK_DOWN:
                        ModernAdjustDateTimePart(
                            control, control->dateTimeSelectedPart, -1);
                        return 0;
                    case VK_HOME:
                        ModernSelectDateTimePart(control, MODERN_DATETIME_HOUR);
                        return 0;
                    case VK_END:
                        ModernSelectDateTimePart(control, MODERN_DATETIME_SECOND);
                        return 0;
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
                return 0;
            }
            if (wParam == VK_ESCAPE && state && state->hwnd) {
                if (control && control->kind == MODERN_CONTROL_COMBO &&
                    SendMessageW(hwnd, CB_GETDROPPEDSTATE, 0, 0)) {
                    break;
                }
                SendMessageW(state->hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (state && state->bodyScrollMax96 > 0 &&
                !ModernControlOwnsVerticalScroll(control) &&
                (wParam == VK_PRIOR || wParam == VK_NEXT)) {
                int page = state->bodyViewportHeight96;
                int direction = wParam == VK_PRIOR ? -1 : 1;
                ModernSetBodyScrollOffset(
                    state,
                    state->bodyScrollOffset96 + direction * page);
                return 0;
            }
            break;
        case WM_CHAR:
            if (control && ModernIsDateTimeControl(control)) {
                if (wParam >= L'0' && wParam <= L'9') {
                    ModernInputDateTimeDigit(control, (int)(wParam - L'0'));
                }
                return 0;
            }
            if (control && ModernIsCompactEdit(control) &&
                (wParam == L'\r' || wParam == L'\n')) {
                return 0;
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
                return TRUE;
            }
            if (control && (control->kind == MODERN_CONTROL_CLOSE ||
                            control->kind == MODERN_CONTROL_PUSH)) {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                return TRUE;
            }
            break;
        case WM_GETDLGCODE:
            if (control && ModernIsDateTimeControl(control)) {
                return DefSubclassProc(hwnd, msg, wParam, lParam) |
                       DLGC_WANTARROWS | DLGC_WANTCHARS;
            }
            break;
        case DTM_SETSYSTEMTIME: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (control && ModernIsDateTimeControl(control)) {
                ModernResetDateTimeInput(control);
                ModernHideDateTimeSpinner(control);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return result;
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
            return result;
        }
        case WM_NCDESTROY:
            if (control && ModernIsDateTimeControl(control)) {
                ModernStopDateTimeRepeat(control);
            }
            RemoveWindowSubclass(hwnd, ModernControlSubclassProc,
                                 MODERN_CONTROL_SUBCLASS_ID);
            break;
    }
    (void)state;
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static void ModernHandleDpiChanged(ModernDialogState* state, WPARAM wParam,
                                   LPARAM lParam) {
    state->dpi = HIWORD(wParam) ? HIWORD(wParam) : 96u;
