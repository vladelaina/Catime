/**
 * @file dialog_modern_dialog_proc.c
 * @brief Window subclass procedure for modern resource dialogs.
 */

#include "dialog_modern_internal.h"
LRESULT CALLBACK ModernDialogSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                          LPARAM lParam, UINT_PTR subclassId,
                                          DWORD_PTR refData) {
    (void)subclassId;
    ModernDialogState* state = (ModernDialogState*)refData;

    switch (msg) {
        case WM_SHOWWINDOW:
            return ModernHandleShowWindow(hwnd, wParam, lParam, state);
        case MODERN_DIALOG_FINALIZE_MESSAGE:
            ModernFinalize(state);
            return 0;
        case MODERN_DIALOG_CLEAR_FOCUS_MESSAGE:
            if (state && state->finalized &&
                ModernWindowOwnsFocus((HWND)lParam, GetFocus())) {
                ModernClearFocusedChild(state);
            }
            return 0;
        case WM_TIMER:
            if (ModernHandleBodyScrollTimer(state, wParam)) return 0;
            break;
        case WM_PAINT:
            if (state && state->finalized) {
                PAINTSTRUCT paint = {0};
                HDC hdc = BeginPaint(hwnd, &paint);
                ModernPaintBuffered(state, hdc);
                EndPaint(hwnd, &paint);
                return 0;
            }
            break;
        case WM_PRINTCLIENT:
            if (state && state->finalized) {
                ModernDrawDialog(state, (HDC)wParam);
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            if (state && state->finalized) return 1;
            break;
        case WM_DRAWITEM:
            if (state && lParam) {
                const DRAWITEMSTRUCT* item = (const DRAWITEMSTRUCT*)lParam;
                const ModernControl* control = ModernFindControl(
                    state, item->hwndItem);
                if (!control && item->CtlType == ODT_COMBOBOX &&
                    item->CtlID != 0) {
                    control = ModernFindControl(
                        state, GetDlgItem(state->hwnd, (int)item->CtlID));
                }
                if (control && (control->kind == MODERN_CONTROL_PUSH ||
                                control->kind == MODERN_CONTROL_CLOSE)) {
                    ModernDrawButton(state, item);
                    return TRUE;
                }
                if (control && control->kind == MODERN_CONTROL_COMBO &&
                    !ModernIsDateTimeControl(control)) {
                    ModernDrawComboItem(state, item);
                    return TRUE;
                }
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == MODERN_DIALOG_CLOSE_ID) {
                SendMessageW(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (state && state->finalized && HIWORD(wParam) == CBN_CLOSEUP &&
                lParam) {
                const ModernControl* control =
                    ModernFindControl(state, (HWND)lParam);
                if (control && control->kind == MODERN_CONTROL_COMBO &&
                    !ModernIsDateTimeControl(control) &&
                    ModernCursorIsOverPassiveContent(state)) {
                    PostMessageW(hwnd, MODERN_DIALOG_CLEAR_FOCUS_MESSAGE, 0,
                                 (LPARAM)control->hwnd);
                }
            }
            break;
        case WM_PARENTNOTIFY:
            if (state && state->finalized &&
                LOWORD(wParam) == WM_LBUTTONDOWN) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (ModernPointIsPassiveContent(state, point)) {
                    ModernClearFocusedChild(state);
                }
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                SendMessageW(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        case WM_MOUSEWHEEL:
            if (state && state->finalized &&
                ModernHandleInteractiveWheel(state, wParam, lParam)) {
                return 0;
            }
            if (state && state->finalized && state->bodyScrollMax96 > 0) {
                ModernHandleBodyWheel(state, wParam);
                return 0;
            }
            break;
        case WM_LBUTTONDOWN:
            if (state && state->finalized && state->bodyScrollMax96 > 0) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                RECT track = {0};
                RECT thumb = {0};
                if (ModernGetScrollbarRects(state, &track, &thumb)) {
                    if (PtInRect(&thumb, point)) {
                        ModernBeginBodyScrollDrag(state, point.y);
                        SetCapture(hwnd);
                        return 0;
                    }
                    if (PtInRect(&track, point)) {
                        int direction = point.y < thumb.top ? -1 : 1;
                        ModernSetBodyScrollOffset(
                            state,
                            state->bodyScrollOffset96 +
                                direction * state->bodyViewportHeight96);
                        return 0;
                    }
                }
            }
            if (state && state->finalized) {
                ModernClearFocusedChild(state);
            }
            break;
        case WM_LBUTTONUP:
            if (state && state->scrollBarDragging) {
                ModernEndBodyScrollDrag(state);
                if (GetCapture() == hwnd) ReleaseCapture();
                ModernRefreshBodyScrollbarHover(state);
                return 0;
            }
            break;
        case WM_MOUSEMOVE:
            if (state && state->finalized) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ModernUpdateTitleHover(state, point);
                ModernTrackMouse(hwnd);

                if (state->bodyScrollMax96 <= 0) break;
                RECT track = {0};
                RECT thumb = {0};
                if (ModernGetScrollbarRects(state, &track, &thumb)) {
                    if (state->scrollBarDragging) {
                        int travel = (track.bottom - track.top) -
                                     (thumb.bottom - thumb.top);
                        if (travel > 0) {
                            int offset = state->scrollDragStartOffset96 +
                                MulDiv(point.y - state->scrollDragStartY,
                                       state->bodyScrollMax96, travel);
                            ModernQueueBodyScrollDrag(state, offset);
                        }
                        return 0;
                    }
                    BOOL hovered = PtInRect(&track, point);
                    if (hovered != state->scrollBarHovered) {
                        state->scrollBarHovered = hovered;
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                }
            }
            break;
        case WM_NCMOUSEMOVE:
            if (state && state->finalized) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd, &point);
                ModernUpdateTitleHover(state, point);
                ModernTrackNonClientMouse(hwnd);
            }
            break;
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONDBLCLK:
            if (state && state->finalized && wParam == HTCAPTION) {
                ModernClearFocusedChild(state);
            }
            break;
        case WM_MOUSELEAVE:
            if (state) {
                BOOL repaint = FALSE;
                ModernRefreshTitleHoverFromCursor(state);
                if (state->scrollBarHovered && !state->scrollBarDragging) {
                    state->scrollBarHovered = FALSE;
                    repaint = TRUE;
                }
                if (repaint) InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_NCMOUSELEAVE:
            if (state) ModernRefreshTitleHoverFromCursor(state);
            break;
        case WM_CAPTURECHANGED:
            if (state && state->scrollBarDragging &&
                (HWND)lParam != hwnd) {
                ModernEndBodyScrollDrag(state);
                ModernRefreshBodyScrollbarHover(state);
            }
            break;
        case WM_CANCELMODE:
            if (state && state->scrollBarDragging) {
                ModernEndBodyScrollDrag(state);
                if (GetCapture() == hwnd) ReleaseCapture();
                ModernRefreshBodyScrollbarHover(state);
                return 0;
            }
            break;
        case WM_SETCURSOR:
            if (state && state->bodyScrollMax96 > 0) {
                POINT point = {0};
                GetCursorPos(&point);
                ScreenToClient(hwnd, &point);
                RECT track = {0};
                RECT thumb = {0};
                if (ModernGetScrollbarRects(state, &track, &thumb) &&
                    PtInRect(&track, point)) {
                    SetCursor(LoadCursorW(NULL, IDC_HAND));
                    return TRUE;
                }
            }
            break;
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORBTN: {
            BOOL handled = FALSE;
            LRESULT result = ModernHandleDialogColorMessage(
                msg, wParam, lParam, state, &handled);
            if (handled) return result;
            break;
        }
        case WM_NCHITTEST:
            if (state && state->finalized) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd, &point);
                RECT closeRect = {0};
                if (state->closeButton) GetWindowRect(state->closeButton, &closeRect);
                if (state->closeButton) MapWindowPoints(NULL, hwnd,
                                                       (POINT*)&closeRect, 2);
                if (PtInRect(&closeRect, point)) return HTCLIENT;
                if (point.y < DialogModern_Scale(state->dpi,
                                                 state->headerHeight96)) {
                    return HTCAPTION;
                }
                return HTCLIENT;
            }
            break;
        case WM_DPICHANGED:
            if (state && state->finalized) {
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                ModernHandleDpiChanged(state, wParam, lParam);
                return result;
            }
            break;
        case WM_DISPLAYCHANGE:
            Dialog_EnsureWindowVisible(hwnd);
            break;
        case WM_SIZE:
            if (state && state->finalized && !state->finalizing) {
                ModernSyncClientSizeFromWindow(state);
                ModernLayoutControls(state);
                DialogModern_ApplyWindowShape(hwnd, state->dpi, 20);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_SETTINGCHANGE:
            if (wParam == SPI_SETWORKAREA) {
                Dialog_EnsureWindowVisible(hwnd);
            }
            if (state && state->finalized) {
                DialogModern_Refresh(hwnd);
                return 0;
            }
            break;
        case WM_THEMECHANGED:
            if (state && state->finalized) {
                DialogModern_Refresh(hwnd);
                return 0;
            }
            break;
        case WM_NCDESTROY: {
            ModernDiscardBodyScrollDrag(state);
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            RemovePropW(hwnd, MODERN_DIALOG_STATE_PROP);
            RemoveWindowSubclass(hwnd, ModernDialogSubclassProc,
                                 MODERN_DIALOG_SUBCLASS_ID);
            ModernFreeState(state);
            return result;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}
