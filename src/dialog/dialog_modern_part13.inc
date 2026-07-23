    ModernRebuildResources(state);
    for (size_t i = 0; i < state->controlCount; i++) {
        ModernSetControlFont(state, &state->controls[i]);
    }
    RECT* suggested = (RECT*)lParam;
    int width = DialogModern_Scale(state->dpi,
                                   state->desiredClientWidth96);
    int height = DialogModern_Scale(state->dpi,
                                    state->desiredClientHeight96);
    HMONITOR monitor = suggested ?
        MonitorFromRect(suggested, MONITOR_DEFAULTTONEAREST) :
        MonitorFromWindow(state->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        int maxWidth = info.rcWork.right - info.rcWork.left -
                       DialogModern_Scale(state->dpi, 24);
        int maxHeight = info.rcWork.bottom - info.rcWork.top -
                        DialogModern_Scale(state->dpi, 24);
        if (width > maxWidth) width = maxWidth;
        if (height > maxHeight) height = maxHeight;
    }
    ModernCommitClientSize(state, width, height);
    int x = suggested ? suggested->left : 0;
    int y = suggested ? suggested->top : 0;
    SetWindowPos(state->hwnd, NULL, x, y, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE |
                 (suggested ? 0 : SWP_NOMOVE));
    ModernLayoutControls(state);
    DialogModern_ApplyWindowShape(state->hwnd, state->dpi, 20);
    RedrawWindow(state->hwnd, NULL, NULL,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static void ModernFreeState(ModernDialogState* state) {
    if (!state) return;
    ModernDeleteFonts(state);
    ModernDeleteBrushes(state);
    free(state->controls);
    free(state);
}

static LRESULT CALLBACK ModernDialogSubclassProc(HWND hwnd, UINT msg,
                                                 WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR subclassId,
                                                 DWORD_PTR refData) {
    (void)subclassId;
    ModernDialogState* state = (ModernDialogState*)refData;

    switch (msg) {
        case WM_SHOWWINDOW:
            if (wParam && state && !state->finalized) ModernFinalize(state);
            break;
        case MODERN_DIALOG_FINALIZE_MESSAGE:
            ModernFinalize(state);
            return 0;
        case MODERN_DIALOG_CLEAR_FOCUS_MESSAGE:
            if (state && state->finalized &&
                ModernWindowOwnsFocus((HWND)lParam, GetFocus())) {
                ModernClearFocusedChild(state);
            }
            return 0;
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
                ModernControl* control = ModernFindControl(state,
                                                            item->hwndItem);
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
                ModernControl* control = ModernFindControl(state, (HWND)lParam);
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
        case WM_LBUTTONDOWN:
            if (state && state->finalized && state->bodyScrollMax96 > 0) {
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                RECT track = {0};
                RECT thumb = {0};
                if (ModernGetScrollbarRects(state, &track, &thumb)) {
                    if (PtInRect(&thumb, point)) {
                        state->scrollBarDragging = TRUE;
                        state->scrollDragStartY = point.y;
                        state->scrollDragStartOffset96 =
                            state->bodyScrollOffset96;
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
                state->scrollBarDragging = FALSE;
                if (GetCapture() == hwnd) ReleaseCapture();
                InvalidateRect(hwnd, NULL, FALSE);
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
                            ModernSetBodyScrollOffset(state, offset);
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
            if (state) {
                ModernRefreshTitleHoverFromCursor(state);
            }
            break;
        case WM_CAPTURECHANGED:
            if (state && state->scrollBarDragging &&
                (HWND)lParam != hwnd) {
                state->scrollBarDragging = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_SETCURSOR:
            if (state && state->bodyScrollMax96 > 0) {
                POINT point = {0};
                GetCursorPos(&point);
                ScreenToClient(hwnd, &point);
                RECT track = {0};
                RECT thumb = {0};
