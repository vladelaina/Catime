/**
 * @file dialog_notification_messages.c
 * @brief Notification message text configuration dialog
 */

#include "dialog/dialog_notification.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "config.h"
#include "../resource/resource.h"
#include <string.h>

/* ============================================================================
 * Notification Messages Dialog
 * ============================================================================ */

void ShowNotificationMessagesDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_NOTIFICATION_MSG)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_NOTIFICATION_MSG);
        SetForegroundWindow(existing);
        return;
    }

    ReadNotificationMessagesConfig();

    DialogBoxW(GetModuleHandle(NULL),
              MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_MESSAGES_DIALOG),
              hwndParent,
              NotificationMessagesDlgProc);
}

INT_PTR CALLBACK NotificationMessagesDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogContext* ctx = Dialog_GetContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_NOTIFICATION_MSG, hwndDlg);

            ctx = Dialog_CreateContext();
            if (!ctx) return FALSE;
            Dialog_SetContext(hwndDlg, ctx);

            Dialog_ApplyTopmost(hwndDlg);
            Dialog_CenterOnPrimaryScreen(hwndDlg);

            ReadNotificationMessagesConfig();

            wchar_t wideText[100];

            MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.timeout_message, -1,
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);

            MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.pomodoro_message, -1,
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);

            MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.cycle_complete_message, -1,
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);

            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL1,
                           GetLocalizedString(L"Countdown timeout message:", L"Countdown timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL2,
                           GetLocalizedString(L"Pomodoro timeout message:", L"Pomodoro timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL3,
                           GetLocalizedString(L"Pomodoro cycle complete message:", L"Pomodoro cycle complete message:"));

            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"OK", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"Cancel", L"Cancel"));

            HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);
            HWND hEdit2 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT2);
            HWND hEdit3 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT3);

            Dialog_SubclassEdit(hEdit1, ctx);
            Dialog_SubclassEdit(hEdit2, ctx);
            Dialog_SubclassEdit(hEdit3, ctx);

            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_EDIT1, EM_SETSEL, 0, -1);
            SetFocus(hEdit1);

            return FALSE;
        }

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN: {
            INT_PTR result;
            if (Dialog_HandleColorMessages(msg, wParam, ctx, &result)) {
                return result;
            }
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                wchar_t wTimeout[256] = {0};
                wchar_t wPomodoro[256] = {0};
                wchar_t wCycle[256] = {0};

                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wPomodoro, sizeof(wPomodoro)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wCycle, sizeof(wCycle)/sizeof(wchar_t));

                char timeout_msg[256] = {0};
                char pomodoro_msg[256] = {0};
                char cycle_complete_msg[256] = {0};

                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1,
                                    timeout_msg, sizeof(timeout_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wPomodoro, -1,
                                    pomodoro_msg, sizeof(pomodoro_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wCycle, -1,
                                    cycle_complete_msg, sizeof(cycle_complete_msg), NULL, NULL);

                extern void WriteConfigNotificationMessages(const char* timeout, const char* pomodoro, const char* cycle);
                WriteConfigNotificationMessages(timeout_msg, pomodoro_msg, cycle_complete_msg);

                EndDialog(hwndDlg, IDOK);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            break;

        case WM_DESTROY:
            if (ctx) {
                HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);
                HWND hEdit2 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT2);
                HWND hEdit3 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT3);

                if (hEdit1) Dialog_UnsubclassEdit(hEdit1, ctx);
                if (hEdit2) Dialog_UnsubclassEdit(hEdit2, ctx);
                if (hEdit3) Dialog_UnsubclassEdit(hEdit3, ctx);

                Dialog_FreeContext(ctx);
            }
            Dialog_UnregisterInstance(DIALOG_INSTANCE_NOTIFICATION_MSG);
            break;
    }

    return FALSE;
}

