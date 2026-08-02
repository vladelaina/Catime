/**
 * @file dialog_countdown.c
 * @brief Public entry point for the custom countdown input window.
 */

#include "dialog_countdown_internal.h"

static void ShowModernTimeInputDialog(HWND hwndParent,
                                      DialogInstanceType instanceType,
                                      DWORD dialogId,
                                      int pomodoroTimeIndex) {
    if (Dialog_IsOpen(instanceType)) {
        HWND existing = Dialog_GetInstance(instanceType);
        SetForegroundWindow(existing);
        return;
    }

    if (!DialogInput_IsValidParentWindow(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateCustomTimeInputDialog(
        hwndParent, dialogId, pomodoroTimeIndex);
    if (!hwndDlg) {
        LOG_WARNING("TimeInputDialog: custom window creation failed; using resource fallback");
        hwndDlg = DialogInput_CreateResourceDialog(
            hwndParent, (int)dialogId, dialogId, pomodoroTimeIndex);
    }

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOWNORMAL);
        UpdateWindow(hwndDlg);
        SetForegroundWindow(hwndDlg);
    }
}

void ShowCountdownInputDialog(HWND hwndParent) {
    ShowModernTimeInputDialog(hwndParent, DIALOG_INSTANCE_INPUT,
                              CLOCK_IDD_DIALOG1, -1);
}

void ShowPomodoroTimeEditDialog(HWND hwndParent, int timeIndex) {
    ShowModernTimeInputDialog(hwndParent, DIALOG_INSTANCE_POMODORO_TIME,
                              CLOCK_IDD_POMODORO_TIME_DIALOG, timeIndex);
}
