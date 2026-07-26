/**
 * @file dialog_countdown.c
 * @brief Public entry point for the custom countdown input window.
 */

#include "dialog_countdown_internal.h"

void ShowCountdownInputDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_INPUT)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_INPUT);
        SetForegroundWindow(existing);
        return;
    }

    if (!DialogInput_IsValidParentWindow(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateCustomCountdownDialog(hwndParent);
    if (!hwndDlg) {
        LOG_WARNING("CountdownDialog: custom window creation failed; using resource fallback");
        hwndDlg = DialogInput_CreateResourceDialog(hwndParent, CLOCK_IDD_DIALOG1,
                                            CLOCK_IDD_DIALOG1, -1);
    }

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOWNORMAL);
        UpdateWindow(hwndDlg);
        SetForegroundWindow(hwndDlg);
    }
}
