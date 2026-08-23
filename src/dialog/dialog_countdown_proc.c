#include "dialog_countdown_internal.h"

LRESULT CALLBACK CountdownDialogProc(HWND hwnd, UINT msg,
                                            WPARAM wParam, LPARAM lParam) {
    CountdownDialogState* state =
        (CountdownDialogState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_NCCREATE: {
            CountdownDialogState* newState =
                (CountdownDialogState*)calloc(1, sizeof(*newState));
            if (!newState) {
                return FALSE;
            }
            const CREATESTRUCTW* create = (const CREATESTRUCTW*)lParam;
            const CountdownInputState* input = create
                ? (const CountdownInputState*)create->lpCreateParams : NULL;
            if (input) {
                newState->input = *input;
            } else {
                newState->input.dialogId = CLOCK_IDD_DIALOG1;
                newState->input.pomodoroTimeIndex = -1;
            }
            newState->dpi = CountdownGetDpi(hwnd);
            newState->selectAllOnNextFocus = TRUE;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)newState);
            return TRUE;
        }

        case WM_CREATE:
            state = (CountdownDialogState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (!state || !CountdownCreateControls(hwnd, state)) {
                return -1;
            }
            return 0;

        case WM_PAINT:
            if (!state) break;
            CountdownHandlePaint(hwnd, state);
            return 0;

        case WM_PRINTCLIENT:
            if (state) {
                CountdownPaint(hwnd, state, (HDC)wParam);
                return 0;
            }
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_DRAWITEM:
            if (state && lParam) {
                CountdownDrawButton((const DRAWITEMSTRUCT*)lParam, state);
                return TRUE;
            }
            break;

        case WM_COMMAND: {
            int controlId = LOWORD(wParam);
            int notification = HIWORD(wParam);
            if (!state) break;
            if (controlId == CLOCK_IDC_BUTTON_OK &&
                (notification == BN_CLICKED || notification == 0)) {
                CountdownSubmit(hwnd, state);
                return 0;
            }
            if (controlId == IDCANCEL &&
                (notification == BN_CLICKED || notification == 0)) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (controlId == COUNTDOWN_CLOSE_BUTTON_ID &&
                (notification == BN_CLICKED || notification == 0)) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (controlId == CLOCK_IDC_EDIT) {
                if (notification == EN_CHANGE) {
                    CountdownSanitizeEditText(state->hwndEdit, state);
                    state->showValidationError = FALSE;
                    CountdownUpdatePreview(hwnd, state);
                    return 0;
                }
                if (notification == EN_SETFOCUS || notification == EN_KILLFOCUS) {
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            break;
        }

        case WM_CTLCOLORSTATIC:
            if (state) {
                SetBkMode((HDC)wParam, TRANSPARENT);
                SetTextColor((HDC)wParam, state->textColor);
                return (LRESULT)state->editBrush;
            }
            break;

        case WM_CTLCOLOREDIT:
            if (state) {
                SetTextColor((HDC)wParam, state->textColor);
                SetBkColor((HDC)wParam, state->fieldColor);
                SetBkMode((HDC)wParam, OPAQUE);
                return (LRESULT)state->editBrush;
            }
            break;

        case WM_CTLCOLORBTN:
            if (state) {
                SetBkMode((HDC)wParam, TRANSPARENT);
                return (LRESULT)state->editBrush;
            }
            break;

        case WM_TIMER:
            if (wParam == INPUT_FOCUS_TIMER_ID && state) {
                KillTimer(hwnd, INPUT_FOCUS_TIMER_ID);
                if (!Dialog_HasFocusWithin(hwnd)) {
                    SetForegroundWindow(hwnd);
                    SetFocus(state->hwndEdit);
                    SendMessageW(state->hwndEdit, EM_SETSEL, 0, -1);
                }
                return 0;
            }
            break;

        case WM_APP + 200:
            if (state && state->hwndEdit && IsWindow(state->hwndEdit)) {
                SetForegroundWindow(hwnd);
                SetFocus(state->hwndEdit);
                SendMessageW(state->hwndEdit, EM_SETSEL, 0, -1);
            }
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (wParam == VK_RETURN && state) {
                CountdownSubmit(hwnd, state);
                return 0;
            }
            break;

        case WM_LBUTTONDOWN:
            if (state) {
                CountdownClearChildFocus(hwnd);
            }
            break;

        case WM_NCHITTEST: {
            if (state) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd, &point);
                if (PtInRect(&state->closeFrame, point)) {
                    return HTCLIENT;
                }
                if (point.y < CountdownScaleValue(state, 86)) {
                    return HTCAPTION;
                }
            }
            return HTCLIENT;
        }

        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONDBLCLK:
            if (state && wParam == HTCAPTION) {
                CountdownClearChildFocus(hwnd);
            }
            break;

        case WM_MOUSEMOVE:
            if (state) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                BOOL titleHoverChanged = CountdownUpdateTitleHover(
                    hwnd, state, point);
                CountdownHoverPart hover =
                    PtInRect(&state->closeFrame, point) ?
                        COUNTDOWN_HOVER_CLOSE : COUNTDOWN_HOVER_NONE;
                if (hover != state->hoverPart || titleHoverChanged) {
                    state->hoverPart = hover;
                    CountdownTrackMouse(hwnd);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            break;

        case WM_NCMOUSEMOVE:
            if (state) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd, &point);
                CountdownUpdateTitleHover(hwnd, state, point);
                CountdownTrackNonClientMouse(hwnd);
            }
            break;

        case WM_MOUSELEAVE:
            if (state) {
                CountdownRefreshTitleHoverFromCursor(hwnd, state);
                if (state->hoverPart == COUNTDOWN_HOVER_CLOSE) {
                    state->hoverPart = COUNTDOWN_HOVER_NONE;
                    InvalidateRect(hwnd, &state->closeFrame, FALSE);
                }
            }
            break;

        case WM_NCMOUSELEAVE:
            if (state) {
                CountdownRefreshTitleHoverFromCursor(hwnd, state);
            }
            break;

        case WM_DPICHANGED:
            if (state) {
                state->dpi = HIWORD(wParam) ? HIWORD(wParam) : 96;
                RECT* suggested = (RECT*)lParam;
                if (suggested) {
                    SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                                 suggested->right - suggested->left,
                                 suggested->bottom - suggested->top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                }
                CountdownRefreshPalette(state);
                CountdownBuildFonts(state);
                CountdownUpdateTextMetrics(hwnd, state);
                CountdownEnsureContentWidth(hwnd, state);
                CountdownLayout(hwnd, state);
                return 0;
            }
            break;

        case WM_DISPLAYCHANGE:
            Dialog_EnsureWindowVisible(hwnd);
            return 0;

        case WM_SIZE:
            if (state) {
                CountdownUpdateTextMetrics(hwnd, state);
                CountdownLayout(hwnd, state);
                return 0;
            }
            break;

        case WM_SETTINGCHANGE:
            if (wParam == SPI_SETWORKAREA) {
                Dialog_EnsureWindowVisible(hwnd);
            }
            /* Fall through to refresh theme-dependent colors. */
        case WM_THEMECHANGED:
            if (state) {
                CountdownRefreshPalette(state);
                RedrawWindow(hwnd, NULL, NULL,
                             RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_NCDESTROY:
            if (state) {
                KillTimer(hwnd, INPUT_FOCUS_TIMER_ID);
                DialogInstanceType instanceType =
                    DialogInput_GetInstanceType(state->input.dialogId);
                Dialog_UnregisterInstanceForWindow(instanceType, hwnd);
                if (state->input.dialogId == CLOCK_IDD_DIALOG1 &&
                    g_hwndInputDialog == hwnd) {
                    g_hwndInputDialog = NULL;
                }
                if (state->editBrush) DeleteObject(state->editBrush);
                CountdownDestroyFonts(state);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                free(state);
            }
            break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
