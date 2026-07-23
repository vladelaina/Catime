/**
 * @file hotkey.c
 * @brief Modeless modern hotkey settings dialog
 */
#include "hotkey_internal.h"

#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "dialog/dialog_procedure.h"
#include "log.h"
#include "window_procedure/window_procedure.h"

void ShowHotkeySettingsDialog(HWND parent) {
    HWND dialog;

    if (Dialog_IsOpen(DIALOG_INSTANCE_HOTKEY)) {
        SetForegroundWindow(Dialog_GetInstance(DIALOG_INSTANCE_HOTKEY));
        return;
    }
    if (!Hotkey_IsValidParent(parent)) return;
    dialog = CreateDialogW(GetModuleHandle(NULL),
                           MAKEINTRESOURCEW(CLOCK_IDD_HOTKEY_DIALOG),
                           parent, HotkeySettingsDlgProc);
    if (dialog) {
        ShowWindow(dialog, SW_SHOW);
    } else {
        LOG_ERROR("Failed to create hotkey dialog (error=%lu)",
                  GetLastError());
    }
}

static BOOL HandleHotkeyChange(HWND dialog, WORD controlId) {
    WORD hotkey = (WORD)SendDlgItemMessage(
        dialog, controlId, HKM_GETHOTKEY, 0, 0);
    HWND control = GetDlgItem(dialog, controlId);

    if (Hotkey_ValidateAndSanitize(&hotkey)) {
        SendDlgItemMessage(dialog, controlId, HKM_SETHOTKEY, hotkey, 0);
        InvalidateRect(control, NULL, FALSE);
        return TRUE;
    }
    Hotkey_ClearDuplicates(dialog, controlId, hotkey);
    InvalidateRect(control, NULL, FALSE);
    return TRUE;
}

static BOOL HandleCommand(HWND dialog, WPARAM wParam) {
    WORD controlId = LOWORD(wParam);
    WORD notification = HIWORD(wParam);

    if (notification == EN_CHANGE && Hotkey_IsEditControl(controlId)) {
        return HandleHotkeyChange(dialog, controlId);
    }
    if (controlId == IDOK) {
        Hotkey_GetControlValues(dialog);
        Hotkey_ValidateAll();
        if (!Hotkey_SaveConfiguration()) {
            Dialog_ShowErrorAndRefocus(dialog, IDOK);
            return TRUE;
        }
        Hotkey_PostReregister(dialog);
        DestroyWindow(dialog);
        return TRUE;
    }
    if (controlId == IDCANCEL) {
        Hotkey_PostReregister(dialog);
        DestroyWindow(dialog);
        return TRUE;
    }
    return FALSE;
}

static INT_PTR InitializeDialog(HWND dialog) {
    HWND parent = Hotkey_GetDialogParent(dialog);
    HotkeyDialogState* state = Hotkey_CreateDialogState(parent);

    if (!state) {
        LOG_ERROR("Failed to allocate hotkey dialog state");
        DestroyWindow(dialog);
        return TRUE;
    }
    Hotkey_SetDialogState(dialog, state);
    Dialog_InitializeInstance(DIALOG_INSTANCE_HOTKEY, dialog);
    MoveDialogToPrimaryScreen(dialog);
    Hotkey_InitializeLabels(dialog);
    Hotkey_LoadConfiguration();
    Hotkey_SetControlValues(dialog);
    if (parent) {
        UnregisterGlobalHotkeys(parent);
        state->hotkeysSuspended = TRUE;
    }
    Hotkey_SetupControlSubclasses(dialog);
    if (!SetWindowSubclass(dialog, HotkeyDialogSubclassProc,
                           HOTKEY_DIALOG_SUBCLASS_ID, 0)) {
        LOG_WARNING("Failed to subclass hotkey dialog (error=%lu)",
                    GetLastError());
    }
    SetFocus(GetDlgItem(dialog, IDCANCEL));
    return FALSE;
}

INT_PTR CALLBACK HotkeySettingsDlgProc(HWND dialog, UINT message,
                                       WPARAM wParam, LPARAM lParam) {
    HotkeyDialogState* state = Hotkey_GetDialogState(dialog);

    switch (message) {
        case WM_INITDIALOG:
            return InitializeDialog(dialog);
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, DIALOG_BG_COLOR);
            if (state && Hotkey_EnsureBrush(&state->backgroundBrush,
                                            DIALOG_BG_COLOR)) {
                return (INT_PTR)state->backgroundBrush;
            }
            break;
        case WM_CTLCOLORBTN:
            SetBkColor((HDC)wParam, BUTTON_BG_COLOR);
            if (state && Hotkey_EnsureBrush(&state->buttonBrush,
                                            BUTTON_BG_COLOR)) {
                return (INT_PTR)state->buttonBrush;
            }
            break;
        case WM_LBUTTONDOWN: {
            POINT point = {LOWORD(lParam), HIWORD(lParam)};
            HWND hit = ChildWindowFromPoint(dialog, point);
            if (hit && hit != dialog) {
                if (!Hotkey_IsEditControl(GetDlgCtrlID(hit))) {
                    SetFocus(GetDlgItem(dialog, IDOK));
                }
            } else if (hit == dialog) {
                SetFocus(GetDlgItem(dialog, IDOK));
                return TRUE;
            }
            break;
        }
        case WM_COMMAND:
            if (HandleCommand(dialog, wParam)) return TRUE;
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                Hotkey_PostReregister(dialog);
                DestroyWindow(dialog);
                return TRUE;
            }
            break;
        case WM_CLOSE:
            Hotkey_PostReregister(dialog);
            DestroyWindow(dialog);
            return TRUE;
        case WM_DESTROY:
            Hotkey_PostReregister(dialog);
            Hotkey_RemoveControlSubclasses(dialog);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_HOTKEY,
                                               dialog);
            Hotkey_DestroyDialogState(dialog, state);
            break;
    }
    return FALSE;
}
