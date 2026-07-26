/**
 * @file dialog_countdown_controls.c
 * @brief Child control navigation and subclasses.
 */

#include "dialog_countdown_internal.h"

void CountdownMoveFocus(CountdownDialogState* state, HWND current,
                               BOOL reverse) {
    if (!state) {
        return;
    }

    HWND controls[4] = {
        state->hwndEdit,
        state->hwndStart,
        state->hwndCancel,
        state->hwndClose
    };
    int currentIndex = 0;
    for (int i = 0; i < (int)_countof(controls); i++) {
        if (controls[i] == current) {
            currentIndex = i;
            break;
        }
    }

    int step = reverse ? -1 : 1;
    int nextIndex = currentIndex;
    for (int i = 0; i < (int)_countof(controls); i++) {
        nextIndex = (nextIndex + step + (int)_countof(controls)) %
                    (int)_countof(controls);
        if (controls[nextIndex] && IsWindowVisible(controls[nextIndex]) &&
            IsWindowEnabled(controls[nextIndex])) {
            SetFocus(controls[nextIndex]);
            return;
        }
    }
}

void CountdownClearChildFocus(HWND hwnd) {
    HWND focused = GetFocus();
    if (focused && focused != hwnd && IsChild(hwnd, focused)) {
        SetFocus(hwnd);
    }
}

LRESULT CALLBACK CountdownEditSubclassProc(HWND hwnd, UINT msg,
                                                  WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR subclassId,
                                                  DWORD_PTR refData) {
    (void)subclassId;
    CountdownDialogState* state = (CountdownDialogState*)refData;
    HWND parent = GetParent(hwnd);

    switch (msg) {
        case WM_SETFOCUS:
            if (state) {
                state->hoverPart = COUNTDOWN_HOVER_EDIT;
                InvalidateRect(parent, NULL, FALSE);
                if (state->selectAllOnNextFocus) {
                    state->selectAllOnNextFocus = FALSE;
                    PostMessageW(hwnd, EM_SETSEL, 0, -1);
                }
            }
            break;

        case WM_KILLFOCUS:
            if (state && state->hoverPart == COUNTDOWN_HOVER_EDIT) {
                state->hoverPart = COUNTDOWN_HOVER_NONE;
                InvalidateRect(parent, NULL, FALSE);
            }
            break;

        case WM_MOUSEMOVE:
            if (state && state->hoverPart != COUNTDOWN_HOVER_EDIT) {
                state->hoverPart = COUNTDOWN_HOVER_EDIT;
                CountdownTrackMouse(hwnd);
                InvalidateRect(parent, NULL, FALSE);
            }
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

LRESULT CALLBACK CountdownButtonSubclassProc(HWND hwnd, UINT msg,
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
