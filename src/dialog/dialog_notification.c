/**
 * @file dialog_notification.c
 * @brief Notification configuration dialogs implementation
 * 
 * @note This is a refactored version extracted from dialog_procedure.c
 *       Original implementation: ~1000 lines
 *       Refactored implementation: Focused on essential functionality
 */

#include "../include/dialog_notification.h"
#include "../include/dialog_common.h"
#include "../include/language.h"
#include "../include/dialog_language.h"
#include "../include/config.h"
#include "../include/notification.h"
#include "../include/audio_player.h"
#include "../resource/resource.h"
#include <strsafe.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * External Global Variables (declared in config.h)
 * ============================================================================ */

/* Note: These are declared in config.h, no need to redeclare here */

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

            wchar_t wideText[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT)];

            MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_MESSAGE_TEXT, -1,
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);

            MultiByteToWideChar(CP_UTF8, 0, POMODORO_TIMEOUT_MESSAGE_TEXT, -1,
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);

            MultiByteToWideChar(CP_UTF8, 0, POMODORO_CYCLE_COMPLETE_TEXT, -1,
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);

            /* Localize labels */
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL1,
                           GetLocalizedString(L"Countdown timeout message:", L"Countdown timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL2,
                           GetLocalizedString(L"Pomodoro timeout message:", L"Pomodoro timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL3,
                           GetLocalizedString(L"Pomodoro cycle complete message:", L"Pomodoro cycle complete message:"));

            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"OK", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"Cancel", L"Cancel"));

            /* Subclass all edit controls */
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

/* ============================================================================
 * Notification Display Dialog
 * ============================================================================ */

void ShowNotificationDisplayDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_NOTIFICATION_DISP)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_NOTIFICATION_DISP);
        SetForegroundWindow(existing);
        return;
    }

    ReadNotificationTimeoutConfig();
    ReadNotificationOpacityConfig();

    DialogBoxW(GetModuleHandle(NULL),
              MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_DISPLAY_DIALOG),
              hwndParent,
              NotificationDisplayDlgProc);
}

INT_PTR CALLBACK NotificationDisplayDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogContext* ctx = Dialog_GetContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_NOTIFICATION_DISP, hwndDlg);

            ctx = Dialog_CreateContext();
            if (!ctx) return FALSE;
            Dialog_SetContext(hwndDlg, ctx);

            Dialog_ApplyTopmost(hwndDlg);
            Dialog_CenterOnPrimaryScreen(hwndDlg);

            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();

            wchar_t wbuffer[32];

            StringCbPrintfW(wbuffer, sizeof(wbuffer), L"%.1f", (float)NOTIFICATION_TIMEOUT_MS / 1000.0f);
            /* Remove trailing .0 */
            if (wcslen(wbuffer) > 2 && wbuffer[wcslen(wbuffer)-2] == L'.' && wbuffer[wcslen(wbuffer)-1] == L'0') {
                wbuffer[wcslen(wbuffer)-2] = L'\0';
            }
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wbuffer);

            StringCbPrintfW(wbuffer, sizeof(wbuffer), L"%d", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, wbuffer);

            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_LABEL,
                           GetLocalizedString(L"Notification display time (sec):", L"Notification display time (sec):"));

            /* Allow decimal input */
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

                /* Normalize decimal separators (Chinese, Japanese punctuation → .) */
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

                /* Minimum 100ms */
                if (timeInMs > 0 && timeInMs < 100) timeInMs = 100;

                int opacity = atoi(opacityStr);

                /* Clamp: 1-100 */
                if (opacity < 1) opacity = 1;
                if (opacity > 100) opacity = 100;

                extern void WriteConfigNotificationTimeout(int timeout_ms);
                extern void WriteConfigNotificationOpacity(int opacity);
                WriteConfigNotificationTimeout(timeInMs);
                WriteConfigNotificationOpacity(opacity);

                EndDialog(hwndDlg, IDOK);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
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

/* ============================================================================
 * Full Notification Settings Dialog (Stub for original complex implementation)
 * ============================================================================ */

/**
 * @note The original NotificationSettingsDlgProc was ~800 lines with:
 *       - Tabbed interface (Messages, Display, Sound)
 *       - Sound file browser with folder creation
 *       - Volume slider with preview
 *       - Live notification testing
 *       - Complex audio playback callbacks
 * 
 *       For now, we delegate to separate dialogs:
 *       - ShowNotificationMessagesDialog()
 *       - ShowNotificationDisplayDialog()
 *       - Sound config (can be added later)
 * 
 *       If a full all-in-one dialog is needed, the original code from
 *       dialog_procedure.c lines 1480-2057 can be migrated here.
 */
void ShowNotificationSettingsDialog(HWND hwndParent) {
    /* For Phase 1, show a simple menu */
    ShowNotificationMessagesDialog(hwndParent);
}

INT_PTR CALLBACK NotificationSettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    /* Stub - full implementation available in original dialog_procedure.c */
    return FALSE;
}

/* ============================================================================
 * Config Read/Write - Implemented in config_notification.c
 * ============================================================================ */

/* 
 * These functions are implemented in config_notification.c
 * No need to redefine them here, they are already declared in config.h
 * and implemented in config_notification.c
 */

/* Removed redundant wrapper functions - use functions from config_notification.c directly */

