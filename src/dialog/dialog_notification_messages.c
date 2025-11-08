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

            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL1,
                           GetLocalizedString(L"Countdown timeout message:", L"Countdown timeout message:"));

            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"OK", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"Cancel", L"Cancel"));

            HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);

            Dialog_SubclassEdit(hEdit1, ctx);

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

                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));

                char timeout_msg[256] = {0};

                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1,
                                    timeout_msg, sizeof(timeout_msg), NULL, NULL);

                extern void WriteConfigNotificationMessages(const char* timeout);
                WriteConfigNotificationMessages(timeout_msg);

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

                if (hEdit1) Dialog_UnsubclassEdit(hEdit1, ctx);

                Dialog_FreeContext(ctx);
            }
            Dialog_UnregisterInstance(DIALOG_INSTANCE_NOTIFICATION_MSG);
            break;
    }

    return FALSE;
}

