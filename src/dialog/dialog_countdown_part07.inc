
    SetWindowTextW(hwnd, state->title);
    Dialog_InitializeInstance(DIALOG_INSTANCE_INPUT, hwnd);
    g_hwndInputDialog = hwnd;

    SetFocus(state->hwndEdit);
    PostMessageW(hwnd, WM_APP + 200, 0, 0);
    PostMessageW(hwnd, WM_APP + 103, 0, 0);
    if (!SetTimer(hwnd, INPUT_FOCUS_TIMER_ID,
                  INPUT_FOCUS_TIMER_DELAY_MS, NULL)) {
        LOG_WARNING("CountdownDialog: failed to start focus timer (error=%lu)",
                    GetLastError());
    }
    return TRUE;
}

static HWND CreateCustomCountdownDialog(HWND hwndParent) {
    if (!CountdownRegisterWindowClass()) {
        return NULL;
    }

    UINT dpi = CountdownGetDpi(hwndParent);
    int width = MulDiv(COUNTDOWN_BASE_WIDTH, (int)dpi, 96);
    int height = MulDiv(COUNTDOWN_BASE_HEIGHT, (int)dpi, 96);
    HMONITOR monitor = MonitorFromWindow(hwndParent, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {0};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
        int workWidth = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
        int workHeight = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
        int availableWidth = workWidth - MulDiv(24, (int)dpi, 96);
        int availableHeight = workHeight - MulDiv(24, (int)dpi, 96);
        int minimumWidth = MulDiv(360, (int)dpi, 96);
        int minimumHeight = MulDiv(330, (int)dpi, 96);
        if (availableWidth < MulDiv(260, (int)dpi, 96)) {
            availableWidth = MulDiv(260, (int)dpi, 96);
        }
        if (availableHeight < MulDiv(280, (int)dpi, 96)) {
            availableHeight = MulDiv(280, (int)dpi, 96);
        }
        if (width < minimumWidth) width = minimumWidth;
        if (height < minimumHeight) height = minimumHeight;
        if (width > availableWidth) width = availableWidth;
        if (height > availableHeight) height = availableHeight;

        int x = monitorInfo.rcWork.left +
                ((monitorInfo.rcWork.right - monitorInfo.rcWork.left) - width) / 2;
        int y = monitorInfo.rcWork.top +
                ((monitorInfo.rcWork.bottom - monitorInfo.rcWork.top) - height) / 2;
        return CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
            COUNTDOWN_WINDOW_CLASS_NAME, L"Set Countdown",
            WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            x, y, width, height, hwndParent, NULL,
            GetModuleHandleW(NULL), NULL);
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;
    return CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
        COUNTDOWN_WINDOW_CLASS_NAME, L"Set Countdown",
        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        x, y, width, height, hwndParent, NULL,
        GetModuleHandleW(NULL), NULL);
}

static LRESULT CALLBACK CountdownDialogProc(HWND hwnd, UINT msg,
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
            newState->input.dialogId = CLOCK_IDD_DIALOG1;
            newState->input.pomodoroTimeIndex = -1;
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

        case WM_PAINT: {
            if (!state) break;
            PAINTSTRUCT paint = {0};
            HDC target = BeginPaint(hwnd, &paint);
            RECT client = {0};
            GetClientRect(hwnd, &client);
            int width = client.right - client.left;
            int height = client.bottom - client.top;
            HDC buffer = CreateCompatibleDC(target);
            HBITMAP bitmap = buffer ? CreateCompatibleBitmap(target, width, height) : NULL;
            HGDIOBJ oldBitmap = buffer && bitmap ? SelectObject(buffer, bitmap) : NULL;
            if (buffer && bitmap) {
                CountdownPaint(hwnd, state, buffer);
                BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
                SelectObject(buffer, oldBitmap);
                DeleteObject(bitmap);
                DeleteDC(buffer);
            } else {
                CountdownPaint(hwnd, state, target);
                if (bitmap) DeleteObject(bitmap);
                if (buffer) DeleteDC(buffer);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }

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

        case WM_APP + 103:
            CountdownReleaseModifiers();
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
