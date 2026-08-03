/**
 * @file hotkey_control.c
 * @brief Modern centered painting and keyboard behavior for hotkey controls
 */
#include "hotkey_internal.h"

#include "config.h"
#include "dialog/dialog_modern.h"
#include "language.h"

#include <strsafe.h>

static void PaintCentered(HWND hwnd, HDC hdc) {
    RECT client = {0};
    DialogModernPalette palette;
    HBRUSH brush;
    HFONT font;
    HGDIOBJ oldFont;
    int oldMode;
    COLORREF oldColor;
    WORD hotkey;
    wchar_t displayText[64] = {0};

    if (!hwnd || !hdc) return;
    GetClientRect(hwnd, &client);
    DialogModern_CopyPalette(GetParent(hwnd), &palette);
    brush = CreateSolidBrush(palette.field);
    if (brush) {
        FillRect(hdc, &client, brush);
        DeleteObject(brush);
    }

    font = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
    oldFont = font ? SelectObject(hdc, font) : NULL;
    oldMode = SetBkMode(hdc, TRANSPARENT);
    oldColor = SetTextColor(hdc, palette.text);
    hotkey = (WORD)SendMessageW(hwnd, HKM_GETHOTKEY, 0, 0);
    if (hotkey == 0) {
        const wchar_t* none = GetLocalizedString(NULL, L"None");
        if (FAILED(StringCchCopyW(displayText, _countof(displayText),
                                  none && none[0] ? none : L"None"))) {
            displayText[_countof(displayText) - 1] = L'\0';
        }
        SetTextColor(hdc, palette.mutedText);
    } else {
        char text[64] = {0};
        HotkeyToString(hotkey, text, sizeof(text));
        if (MultiByteToWideChar(CP_UTF8, 0, text, -1, displayText,
                                _countof(displayText)) <= 0) {
            displayText[0] = L'\0';
        }
    }

    RECT textRect = client;
    InflateRect(&textRect,
                -DialogModern_Scale(DialogModern_GetDpi(hwnd), 8), 0);
    DrawTextW(hdc, displayText, -1, &textRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE |
              DT_END_ELLIPSIS | DT_NOPREFIX);
    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldMode);
    if (oldFont) SelectObject(hdc, oldFont);
}

static BOOL MoveDialogFocus(HWND control, BOOL reverse) {
    HWND dialog = control ? GetParent(control) : NULL;
    HWND next;

    if (!dialog) return FALSE;
    next = GetNextDlgTabItem(dialog, control, reverse);
    if (!next || next == control || !IsWindowVisible(next) ||
        !IsWindowEnabled(next)) {
        return FALSE;
    }
    SetFocus(next);
    return TRUE;
}

LRESULT CALLBACK HotkeyControlSubclassProc(
    HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    UNREFERENCED_PARAMETER(dwRefData);

    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT paint = {0};
            HDC hdc = BeginPaint(hwnd, &paint);
            if (hdc) PaintCentered(hwnd, hdc);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_PRINTCLIENT:
            PaintCentered(hwnd, (HDC)wParam);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            InvalidateRect(hwnd, NULL, FALSE);
            if ((uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) &&
                Hotkey_IsImeVirtualKey((BYTE)wParam)) {
                return 0;
            }
            break;
        case WM_SYSKEYDOWN:
            InvalidateRect(hwnd, NULL, FALSE);
            if (Hotkey_IsImeVirtualKey((BYTE)wParam)) {
                return 0;
            }
            break;
        case WM_KEYDOWN:
            InvalidateRect(hwnd, NULL, FALSE);
            if (Hotkey_IsImeVirtualKey((BYTE)wParam)) {
                return 0;
            }
            if (wParam == VK_TAB && GetKeyState(VK_CONTROL) >= 0 &&
                GetKeyState(VK_MENU) >= 0 &&
                MoveDialogFocus(hwnd, GetKeyState(VK_SHIFT) < 0)) {
                return 0;
            }
            break;
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_UNICHAR:
        case WM_IME_CHAR:
        case WM_IME_STARTCOMPOSITION:
        case WM_IME_COMPOSITION:
        case WM_IME_ENDCOMPOSITION:
        case WM_IME_KEYDOWN:
        case WM_IME_KEYUP:
            return 0;
        case HKM_SETHOTKEY:
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, HotkeyControlSubclassProc,
                                 uIdSubclass);
            break;
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS;
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}
