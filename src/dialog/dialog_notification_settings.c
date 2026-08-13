/**
 * @file dialog_notification_settings.c
 * @brief Coordinates the notification settings dialog and dispatches messages.
 */

#include "dialog/dialog_notification_settings_internal.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "dialog/dialog_notification_audio.h"
#include "dialog/dialog_procedure.h"
#include "language.h"
#include "../resource/resource.h"

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

static HWND g_notificationSettingsDialog = NULL;

NotificationSettingsState* DialogNotificationInternal_GetState(HWND hwndDlg) {
    return hwndDlg ? (NotificationSettingsState*)GetWindowLongPtrW(
                         hwndDlg, GWLP_USERDATA)
                   : NULL;
}

BOOL DialogNotificationInternal_IsCurrentProcessWindow(HWND hwnd) {
    DWORD processId = 0;
    if (!hwnd) return FALSE;
    GetWindowThreadProcessId(hwnd, &processId);
    return processId == GetCurrentProcessId();
}

static BOOL IsWindowOfClass(HWND hwnd, const wchar_t* className) {
    if (!hwnd || !IsWindow(hwnd) || !className) {
        return FALSE;
    }

    wchar_t actualClass[64] = {0};
    if (GetClassNameW(hwnd, actualClass, _countof(actualClass)) == 0) {
        return FALSE;
    }

    return wcscmp(actualClass, className) == 0;
}

BOOL DialogNotificationInternal_IsValidParent(HWND hwnd) {
    return DialogNotificationInternal_IsCurrentProcessWindow(hwnd) &&
           IsWindowOfClass(hwnd, CATIME_MAIN_WINDOW_CLASS_NAME);
}

HWND DialogNotificationInternal_GetParent(HWND hwndDlg) {
    HWND hwndParent = Dialog_GetOwnerWindow(hwndDlg);
    return DialogNotificationInternal_IsValidParent(hwndParent) ? hwndParent : NULL;
}

BOOL DialogNotificationInternal_IsCurrentDialog(HWND hwnd) {
    return hwnd &&
           hwnd == g_notificationSettingsDialog &&
           IsWindow(hwnd) &&
           Dialog_IsOpen(DIALOG_INSTANCE_NOTIFICATION_FULL);
}

HWND DialogNotificationInternal_GetDialog(void) {
    return g_notificationSettingsDialog;
}

void DialogNotificationInternal_SetDialog(HWND hwndDlg) {
    g_notificationSettingsDialog = hwndDlg;
}

void ShowNotificationSettingsDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_NOTIFICATION_FULL)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_NOTIFICATION_FULL);
        SetForegroundWindow(existing);
        return;
    }

    if (!DialogNotificationInternal_IsValidParent(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateDialogW(GetModuleHandle(NULL),
              MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG),
              hwndParent,
              NotificationSettingsDlgProc);

    if (hwndDlg) {
        DialogModern_ShowPaintedWindow(hwndDlg, SW_SHOW);
    }
}

INT_PTR CALLBACK NotificationSettingsDlgProc(HWND hwndDlg, UINT msg,
                                             WPARAM wParam, LPARAM lParam) {
    NotificationSettingsState* state = DialogNotificationInternal_GetState(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG:
            return DialogNotificationInternal_OnInit(hwndDlg);

        case WM_HSCROLL:
            return DialogNotificationInternal_OnHScroll(hwndDlg, state, lParam);

        case WM_COMMAND:
            return DialogNotificationInternal_OnCommand(hwndDlg, state, wParam);

        case WM_NOTIFICATION_SOUND_PLAYBACK_COMPLETE:
            if (state) state->isPlaying = FALSE;
            SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON,
                            GetLocalizedString(NULL, L"Test"));
            return TRUE;

        case WM_NOTIFICATION_SOUND_CACHE_UPDATED: {
            HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
            if (hwndCombo) {
                if (SendMessage(hwndCombo, CB_GETDROPPEDSTATE, 0, 0)) {
                    if (state) state->soundComboRefreshPending = TRUE;
                } else {
                    RefreshNotificationSoundComboBox(hwndCombo);
                    if (state) state->soundComboRefreshPending = FALSE;
                }
            }
            return TRUE;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DialogNotificationInternal_CloseWithoutSaving(
                    hwndDlg, state, TRUE);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            DialogNotificationInternal_CloseWithoutSaving(
                hwndDlg, state, FALSE);
            return TRUE;

        case WM_DESTROY:
            DialogNotificationInternal_OnDestroy(hwndDlg, state);
            break;
    }
    return FALSE;
}
