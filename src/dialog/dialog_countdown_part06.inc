            break;

        case WM_MOUSELEAVE:
            if (state && GetFocus() != hwnd &&
                state->hoverPart == COUNTDOWN_HOVER_EDIT) {
                state->hoverPart = COUNTDOWN_HOVER_NONE;
                InvalidateRect(parent, NULL, FALSE);
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                SendMessageW(parent, WM_COMMAND,
                             MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED),
                             (LPARAM)hwnd);
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                SendMessageW(parent, WM_CLOSE, 0, 0);
                return 0;
            }
            if (wParam == VK_TAB) {
                CountdownMoveFocus(state, hwnd, GetKeyState(VK_SHIFT) < 0);
                return 0;
            }
            if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
                SendMessageW(hwnd, EM_SETSEL, 0, -1);
                return 0;
            }
            break;

        case WM_CHAR:
            if (wParam == VK_RETURN || wParam == VK_TAB) {
                return 0;
            }
            if (wParam >= 0x20 &&
                !CountdownIsAllowedInputChar((wchar_t)wParam)) {
                return 0;
            }
            break;

        case WM_PASTE: {
            wchar_t filtered[256] = {0};
            BOOL textAvailable = FALSE;
            if (OpenClipboard(hwnd)) {
                HANDLE data = GetClipboardData(CF_UNICODETEXT);
                if (data) {
                    const wchar_t* source = (const wchar_t*)GlobalLock(data);
                    if (source) {
                        CountdownCopyAllowedInput(filtered,
                                                  _countof(filtered), source);
                        textAvailable = TRUE;
                        GlobalUnlock(data);
                    }
                }
                CloseClipboard();
            }
            if (textAvailable) {
                SendMessageW(hwnd, EM_REPLACESEL, TRUE, (LPARAM)filtered);
            }
            return 0;
        }

        case WM_SETTEXT: {
            const wchar_t* source = (const wchar_t*)lParam;
            if (!source) break;
            wchar_t filtered[256] = {0};
            CountdownCopyAllowedInput(filtered, _countof(filtered), source);
            return DefSubclassProc(hwnd, msg, wParam, (LPARAM)filtered);
        }

        case WM_IME_CHAR:
            if (!CountdownIsAllowedInputChar((wchar_t)wParam)) {
                return 0;
            }
            break;

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, CountdownEditSubclassProc,
                                 COUNTDOWN_EDIT_SUBCLASS_ID);
            break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK CountdownButtonSubclassProc(HWND hwnd, UINT msg,
                                                    WPARAM wParam, LPARAM lParam,
                                                    UINT_PTR subclassId,
                                                    DWORD_PTR refData) {
    (void)subclassId;
    CountdownDialogState* state = (CountdownDialogState*)refData;
    CountdownHoverPart part = CountdownPartForButton(state, hwnd);
    HWND parent = GetParent(hwnd);

    switch (msg) {
        case WM_MOUSEMOVE:
            if (state && state->hoverPart != part) {
                state->hoverPart = part;
                CountdownTrackMouse(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;

        case WM_MOUSELEAVE:
            if (state && state->hoverPart == part && GetCapture() != hwnd) {
                state->hoverPart = COUNTDOWN_HOVER_NONE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;

        case WM_LBUTTONDOWN:
            if (state) {
                state->pressedPart = part;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;

        case WM_LBUTTONUP:
            if (state) {
                state->pressedPart = COUNTDOWN_HOVER_NONE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;

        case WM_CAPTURECHANGED:
            if (state && state->pressedPart == part) {
                state->pressedPart = COUNTDOWN_HOVER_NONE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN || wParam == VK_SPACE) {
                if (part == COUNTDOWN_HOVER_CLOSE) {
                    SendMessageW(parent, WM_CLOSE, 0, 0);
                } else if (part == COUNTDOWN_HOVER_START) {
                    SendMessageW(parent, WM_COMMAND,
                                 MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED),
                                 (LPARAM)hwnd);
                } else if (part == COUNTDOWN_HOVER_CANCEL) {
                    SendMessageW(parent, WM_COMMAND,
                                 MAKEWPARAM(IDCANCEL, BN_CLICKED),
                                 (LPARAM)hwnd);
                }
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                SendMessageW(parent, WM_CLOSE, 0, 0);
                return 0;
            }
            if (wParam == VK_TAB) {
                CountdownMoveFocus(state, hwnd, GetKeyState(VK_SHIFT) < 0);
                return 0;
            }
            break;

        case WM_SETCURSOR:
            SetCursor(LoadCursorW(NULL,
                                  (part == COUNTDOWN_HOVER_CLOSE ||
                                   part == COUNTDOWN_HOVER_START ||
                                   part == COUNTDOWN_HOVER_CANCEL) ?
                                      IDC_HAND : IDC_ARROW));
            return TRUE;

        case WM_ERASEBKGND:
            return 1;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, NULL, FALSE);
            break;

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, CountdownButtonSubclassProc,
                                 COUNTDOWN_BUTTON_SUBCLASS_ID);
            break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static void CountdownReleaseModifiers(void) {
    INPUT inputs[8] = {0};
    WORD keys[8] = {
        VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL,
        VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN
    };

    for (int i = 0; i < (int)_countof(keys); i++) {
        inputs[i].type = INPUT_KEYBOARD;
        inputs[i].ki.wVk = keys[i];
        inputs[i].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    SendInput((UINT)_countof(inputs), inputs, sizeof(INPUT));
}

static BOOL CountdownRegisterWindowClass(void) {
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSEXW existing = {0};
    existing.cbSize = sizeof(existing);
    if (GetClassInfoExW(instance, COUNTDOWN_WINDOW_CLASS_NAME, &existing)) {
        return TRUE;
    }

    WNDCLASSEXW windowClass = {0};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    windowClass.lpfnWndProc = CountdownDialogProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.hbrBackground = NULL;
    windowClass.lpszClassName = COUNTDOWN_WINDOW_CLASS_NAME;
    if (RegisterClassExW(&windowClass)) {
        return TRUE;
    }
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

static BOOL CountdownCreateControls(HWND hwnd, CountdownDialogState* state) {
    if (!hwnd || !state) {
        return FALSE;
    }

    CountdownLoadTexts(state);
    CountdownRefreshPalette(state);

    DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                       ES_AUTOHSCROLL | ES_LEFT | ES_NOHIDESEL;
    state->hwndEdit = CreateWindowExW(
        0, L"EDIT", L"", editStyle,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)CLOCK_IDC_EDIT,
        GetModuleHandleW(NULL), NULL);

    DWORD buttonStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                         BS_OWNERDRAW | BS_NOTIFY;
    state->hwndCancel = CreateWindowExW(
        0, L"BUTTON", state->cancelText, buttonStyle,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDCANCEL,
        GetModuleHandleW(NULL), NULL);
    state->hwndStart = CreateWindowExW(
        0, L"BUTTON", state->startText, buttonStyle | BS_DEFPUSHBUTTON,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)CLOCK_IDC_BUTTON_OK,
        GetModuleHandleW(NULL), NULL);
    state->hwndClose = CreateWindowExW(
        0, L"BUTTON", state->cancelText, buttonStyle,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)COUNTDOWN_CLOSE_BUTTON_ID,
        GetModuleHandleW(NULL), NULL);

    if (!state->hwndEdit || !state->hwndCancel || !state->hwndStart ||
        !state->hwndClose) {
        return FALSE;
    }

    DialogModern_ApplyTheme(hwnd, state->darkMode);
    DialogModern_ApplyTheme(state->hwndEdit, state->darkMode);

    SendMessageW(state->hwndEdit, EM_SETLIMITTEXT,
                 (WPARAM)(_countof(inputText) - 1), 0);
    SendMessageW(state->hwndEdit, EM_SETMARGINS,
                 EC_LEFTMARGIN | EC_RIGHTMARGIN,
                 MAKELONG(CountdownScaleValue(state, 2),
                          CountdownScaleValue(state, 2)));
    SendMessageW(state->hwndEdit, EM_SETCUEBANNER, TRUE,
                 (LPARAM)L"25m 30s");

    SetWindowSubclass(state->hwndEdit, CountdownEditSubclassProc,
                      COUNTDOWN_EDIT_SUBCLASS_ID, (DWORD_PTR)state);
    SetWindowSubclass(state->hwndCancel, CountdownButtonSubclassProc,
                      COUNTDOWN_BUTTON_SUBCLASS_ID, (DWORD_PTR)state);
    SetWindowSubclass(state->hwndStart, CountdownButtonSubclassProc,
                      COUNTDOWN_BUTTON_SUBCLASS_ID, (DWORD_PTR)state);
    SetWindowSubclass(state->hwndClose, CountdownButtonSubclassProc,
                      COUNTDOWN_BUTTON_SUBCLASS_ID, (DWORD_PTR)state);

    CountdownBuildFonts(state);
    CountdownUpdateTextMetrics(hwnd, state);
    CountdownEnsureContentWidth(hwnd, state);
    CountdownLayout(hwnd, state);
    CountdownUpdatePreview(hwnd, state);
