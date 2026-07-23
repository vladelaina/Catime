/**
 * @file hotkey_dialog_state.c
 * @brief Hotkey dialog lifetime and suspended-registration recovery
 */
#include "hotkey_internal.h"

#include "log.h"
#include "window_procedure/window_procedure.h"

#include <stdlib.h>
#include <wchar.h>

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

BOOL Hotkey_IsValidParent(HWND hwnd) {
    DWORD processId = 0;
    wchar_t className[64] = {0};

    if (!hwnd || !IsWindow(hwnd)) return FALSE;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) return FALSE;
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }
    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

HWND Hotkey_GetDialogParent(HWND dialog) {
    HWND parent = dialog ? GetParent(dialog) : NULL;
    return Hotkey_IsValidParent(parent) ? parent : NULL;
}

HotkeyDialogState* Hotkey_GetDialogState(HWND dialog) {
    return (HotkeyDialogState*)GetWindowLongPtr(dialog, GWLP_USERDATA);
}

void Hotkey_SetDialogState(HWND dialog, HotkeyDialogState* state) {
    SetWindowLongPtr(dialog, GWLP_USERDATA, (LONG_PTR)state);
}

HotkeyDialogState* Hotkey_CreateDialogState(HWND parent) {
    HotkeyDialogState* state =
        (HotkeyDialogState*)calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->hwndParent = parent;
    state->backgroundBrush = CreateSolidBrush(DIALOG_BG_COLOR);
    state->buttonBrush = CreateSolidBrush(BUTTON_BG_COLOR);
    return state;
}

void Hotkey_DestroyDialogState(HWND dialog, HotkeyDialogState* state) {
    if (!state) return;
    if (state->backgroundBrush) DeleteObject(state->backgroundBrush);
    if (state->buttonBrush) DeleteObject(state->buttonBrush);
    Hotkey_SetDialogState(dialog, NULL);
    free(state);
}

BOOL Hotkey_EnsureBrush(HBRUSH* brush, COLORREF color) {
    if (!brush) return FALSE;
    if (!*brush) *brush = CreateSolidBrush(color);
    return *brush != NULL;
}

void Hotkey_PostReregister(HWND dialog) {
    HotkeyDialogState* state = Hotkey_GetDialogState(dialog);

    if (!state || !state->hotkeysSuspended || state->reregisterPosted) {
        return;
    }
    if (!Hotkey_IsValidParent(state->hwndParent)) return;
    if (PostMessage(state->hwndParent, WM_APP + 1, 0, 0)) {
        state->reregisterPosted = TRUE;
        return;
    }

    LOG_WARNING("Failed to post hotkey re-register message (error=%lu)",
                GetLastError());
    RegisterGlobalHotkeys(state->hwndParent);
    state->reregisterPosted = TRUE;
}
