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
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

#define NOTIFICATION_DIALOG_MAX_TIMEOUT_MS 60000

/* ============================================================================
 * Notification Display Dialog
 * ============================================================================ */

static BOOL ConvertNotificationInputToUtf8(const wchar_t* source, char* dest, size_t destSize) {
    if (!source || !dest || destSize == 0 || destSize > INT_MAX) {
        return FALSE;
    }

    dest[0] = '\0';
    int required = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    if (required <= 0 || (size_t)required > destSize) {
        return FALSE;
    }

    return WideCharToMultiByte(CP_UTF8, 0, source, -1, dest,
                               (int)destSize, NULL, NULL) > 0;
}

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

static BOOL ParseTimeoutMsInput(const char* text, int* timeoutMs) {
    if (!text || !timeoutMs) return FALSE;

    while (isspace((unsigned char)*text)) text++;
    if (*text == '\0') return FALSE;

    errno = 0;
    char* end = NULL;
    double seconds = strtod(text, &end);
    if (end == text || errno == ERANGE || !isfinite(seconds) || seconds < 0.0) {
        return FALSE;
    }

    while (end && isspace((unsigned char)*end)) end++;
    if (end && *end != '\0') return FALSE;

    double ms = seconds * 1000.0;
    if (ms > (double)NOTIFICATION_DIALOG_MAX_TIMEOUT_MS) {
        return FALSE;
    }

    int roundedMs = (int)(ms + 0.5);
    if (roundedMs > 0 && roundedMs < 100) roundedMs = 100;
    *timeoutMs = roundedMs;
    return TRUE;
}

static BOOL ParseOpacityInput(const char* text, int* opacity) {
    if (!text || !opacity) return FALSE;

    while (isspace((unsigned char)*text)) text++;
    if (*text == '\0') return FALSE;

    errno = 0;
    char* end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || errno == ERANGE || parsed < 1 || parsed > 100 || parsed > INT_MAX) {
        return FALSE;
    }

    while (end && isspace((unsigned char)*end)) end++;
    if (end && *end != '\0') return FALSE;

    *opacity = (int)parsed;
    return TRUE;
}

INT_PTR CALLBACK NotificationDisplayDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    DialogContext* ctx = Dialog_GetContext(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_NOTIFICATION_DISP, hwndDlg);

            ctx = Dialog_CreateContext();
            if (!ctx) {
                Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_NOTIFICATION_DISP, hwndDlg);
                DestroyWindow(hwndDlg);
                return TRUE;
            }
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
                    if (wtimeStr[i] == L'\x3002' || wtimeStr[i] == L'\xFF0C' || wtimeStr[i] == L',' ||
                        wtimeStr[i] == L'\xFF0E' || wtimeStr[i] == L'\x3001') {
                        wtimeStr[i] = L'.';
                    }
                }

                if (!ConvertNotificationInputToUtf8(wtimeStr, timeStr, sizeof(timeStr))) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
                    return TRUE;
                }
                if (!ConvertNotificationInputToUtf8(wopacityStr, opacityStr, sizeof(opacityStr))) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
                    return TRUE;
                }

                int timeInMs = 0;
                int opacity = 0;
                if (!ParseTimeoutMsInput(timeStr, &timeInMs)) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
                    return TRUE;
                }
                if (!ParseOpacityInput(opacityStr, &opacity)) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
                    return TRUE;
                }

                extern BOOL WriteConfigNotificationTimeout(int timeout_ms);
                extern BOOL WriteConfigNotificationOpacity(int opacity);
                if (!WriteConfigNotificationTimeout(timeInMs) ||
                    !WriteConfigNotificationOpacity(opacity)) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
                    return TRUE;
                }

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

                Dialog_DestroyContext(hwndDlg);
            }
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_NOTIFICATION_DISP, hwndDlg);
            break;
    }

    return FALSE;
}

