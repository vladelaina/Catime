/**
 * @file dialog_notification_display.c
 * @brief Notification display settings dialog (timeout and opacity)
 */

#include "dialog/dialog_notification.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "config.h"
#include "../resource/resource.h"
#include <strsafe.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Notification Display Dialog
 * ============================================================================ */

void ShowNotificationDisplayDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_NOTIFICATION_DISP)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_NOTIFICATION_DISP);
        SetForegroundWindow(existing);
        return;
    }

    HWND hwndDlg = CreateDialogW(GetModuleHandle(NULL),
              MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_DISPLAY_DIALOG),
              hwndParent,
              NotificationDisplayDlgProc);
    
    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}

INT_PTR CALLBACK NotificationDisplayDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogContext* ctx = Dialog_GetContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_NOTIFICATION_DISP, hwndDlg);

            ctx = Dialog_CreateContext();
            if (!ctx) return FALSE;
            Dialog_SetContext(hwndDlg, ctx);

            Dialog_CenterOnPrimaryScreen(hwndDlg);

            wchar_t wbuffer[32];

            StringCbPrintfW(wbuffer, sizeof(wbuffer), L"%.1f", (float)g_AppConfig.notification.display.timeout_ms / 1000.0f);
            if (wcslen(wbuffer) > 2 && wbuffer[wcslen(wbuffer)-2] == L'.' && wbuffer[wcslen(wbuffer)-1] == L'0') {
                wbuffer[wcslen(wbuffer)-2] = L'\0';
            }
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wbuffer);

            StringCbPrintfW(wbuffer, sizeof(wbuffer), L"%d", g_AppConfig.notification.display.max_opacity);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, wbuffer);

            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_LABEL,
                           GetLocalizedString(L"Notification display time (sec):", L"Notification display time (sec):"));

            HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
            LONG style = GetWindowLong(hEditTime, GWL_STYLE);
            SetWindowLong(hEditTime, GWL_STYLE, style & ~ES_NUMBER);

            Dialog_SubclassEdit(hEditTime, ctx);
            SetFocus(hEditTime);

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
                char timeStr[32] = {0};
                char opacityStr[32] = {0};

                wchar_t wtimeStr[32], wopacityStr[32];
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wtimeStr, sizeof(wtimeStr)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, wopacityStr, sizeof(wopacityStr)/sizeof(wchar_t));

                for (int i = 0; wtimeStr[i] != L'\0'; i++) {
                    if (wtimeStr[i] == L'。' || wtimeStr[i] == L'，' || wtimeStr[i] == L',' ||
                        wtimeStr[i] == L'．' || wtimeStr[i] == L'、') {
                        wtimeStr[i] = L'.';
                    }
                }

                WideCharToMultiByte(CP_UTF8, 0, wtimeStr, -1, timeStr, sizeof(timeStr), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wopacityStr, -1, opacityStr, sizeof(opacityStr), NULL, NULL);

                float timeInSeconds = atof(timeStr);
                int timeInMs = (int)(timeInSeconds * 1000.0f);

                if (timeInMs > 0 && timeInMs < 100) timeInMs = 100;

                int opacity = atoi(opacityStr);

                if (opacity < 1) opacity = 1;
                if (opacity > 100) opacity = 100;

                extern void WriteConfigNotificationTimeout(int timeout_ms);
                extern void WriteConfigNotificationOpacity(int opacity);
                WriteConfigNotificationTimeout(timeInMs);
                WriteConfigNotificationOpacity(opacity);

                DestroyWindow(hwndDlg);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;

        case WM_DESTROY:
            if (ctx) {
                HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
                HWND hEditOpacity = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);

                if (hEditTime) Dialog_UnsubclassEdit(hEditTime, ctx);
                if (hEditOpacity) Dialog_UnsubclassEdit(hEditOpacity, ctx);

                Dialog_FreeContext(ctx);
            }
            Dialog_UnregisterInstance(DIALOG_INSTANCE_NOTIFICATION_DISP);
            break;
    }

    return FALSE;
}

