/**
 * @file hotkey_dialog_input.c
 * @brief Dialog-level interception that prevents configured hotkeys firing
 */
#include "hotkey_internal.h"

static BYTE GetCurrentModifiers(UINT message) {
    BYTE modifiers = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= HOTKEYF_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= HOTKEYF_CONTROL;
    if (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP ||
        (GetKeyState(VK_MENU) & 0x8000)) {
        modifiers |= HOTKEYF_ALT;
    }
    return modifiers;
}

static BOOL FocusIsHotkeyControl(void) {
    HWND focus = GetFocus();
    return focus && Hotkey_IsEditControl(GetDlgCtrlID(focus));
}

LRESULT CALLBACK HotkeyDialogSubclassProc(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData) {
    UNREFERENCED_PARAMETER(refData);

    if (message == WM_NCDESTROY) {
        LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        RemoveWindowSubclass(hwnd, HotkeyDialogSubclassProc, subclassId);
        return result;
    }
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN ||
        message == WM_KEYUP || message == WM_SYSKEYUP) {
        BYTE virtualKey = (BYTE)wParam;
        if (!Hotkey_IsModifierKey(virtualKey)) {
            WORD combination = MAKEWORD(
                virtualKey, GetCurrentModifiers(message));
            if (Hotkey_IsExistingEvent(combination) &&
                !FocusIsHotkeyControl()) {
                return 0;
            }
        }
    }

    switch (message) {
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            if (!FocusIsHotkeyControl()) return 0;
            break;
        case WM_KEYDOWN:
        case WM_KEYUP:
            if (Hotkey_IsModifierKey((BYTE)wParam) &&
                !FocusIsHotkeyControl()) {
                return 0;
            }
            break;
        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}
