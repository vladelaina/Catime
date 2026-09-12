/**
 * @file dialog_notification_commands.c
 * @brief Handles notification settings commands and cleanup.
 */

#include "dialog/dialog_notification_settings_internal.h"
#include "audio_player.h"
#include "config.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_notification_audio.h"
#include "log.h"
#include "utils/string_convert.h"
#include "../resource/resource.h"

#include <stdlib.h>
#include <wchar.h>

static void SaveNotificationSettings(
    HWND hwndDlg, NotificationSettingsState* state) {
    if (!DialogNotificationInternal_SavePreviewPlacement()) {
        LOG_WARNING("Failed to save notification preview window placement");
        Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_EDIT1);
        return;
    }

    wchar_t wTimeout[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE] = {0};
    GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout,
                    sizeof(wTimeout) / sizeof(wchar_t));

    char timeoutMessage[NOTIFICATION_MESSAGE_BUFFER_SIZE] = {0};
    if (!WideToUtf8(wTimeout, timeoutMessage, sizeof(timeoutMessage))) {
        Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_EDIT1);
        return;
    }

    SYSTEMTIME st = {0};
    BOOL isDisabled = IsDlgButtonChecked(
        hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED;
    int timeoutMs = g_AppConfig.notification.display.timeout_ms;
    if (SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT,
                           DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID) {
        int totalSeconds = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
        if (totalSeconds == 0) {
            timeoutMs = 0;
        } else if (!isDisabled) {
            timeoutMs = totalSeconds * 1000;
        }
    }

    int opacity = DialogNotificationInternal_ClampOpacity(
        (int)SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT,
                                TBM_GETPOS, 0, 0));
    int cornerRadius = (int)SendDlgItemMessage(
        hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER, TBM_GETPOS, 0, 0);
    int fontPercent = (int)SendDlgItemMessage(
        hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER, TBM_GETPOS, 0, 0);

    NotificationType notificationType = NOTIFICATION_TYPE_CATIME;
    if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME)) {
        notificationType = NOTIFICATION_TYPE_CATIME;
    } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_OS)) {
        notificationType = NOTIFICATION_TYPE_OS;
    } else if (IsDlgButtonChecked(
                   hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL)) {
        notificationType = NOTIFICATION_TYPE_SYSTEM_MODAL;
    }

    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
    char soundFile[MAX_PATH] = {0};
    if (!GetSelectedNotificationSoundFile(
            hwndCombo, soundFile, sizeof(soundFile))) {
        return;
    }

    int volume = (int)SendDlgItemMessage(
        hwndDlg, IDC_VOLUME_SLIDER, TBM_GETPOS, 0, 0);
    BOOL useForPomodoro = IsDlgButtonChecked(
        hwndDlg, IDC_NOTIFICATION_USE_FOR_POMODORO) == BST_CHECKED;
    if (!WriteConfigNotificationSettings(
            timeoutMessage, timeoutMs, opacity, notificationType,
            cornerRadius, fontPercent, isDisabled, soundFile, volume,
            useForPomodoro)) {
        Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_EDIT1);
        return;
    }

    DialogNotificationInternal_ClosePreview();
    CleanupAudioPlayback(state && state->isPlaying);
    if (state) {
        state->isPlaying = FALSE;
        state->isInitializing = TRUE;
    }
    DestroyWindow(hwndDlg);
}

void DialogNotificationInternal_CloseWithoutSaving(
    HWND hwndDlg, NotificationSettingsState* state, BOOL clearPlayingState) {
    DialogNotificationInternal_ClosePreview();
    if (state) SetAudioVolume(state->originalVolume);
    CleanupAudioPlayback(state && state->isPlaying);
    if (state) {
        if (clearPlayingState) state->isPlaying = FALSE;
        state->isInitializing = TRUE;
    }
    DestroyWindow(hwndDlg);
}

INT_PTR DialogNotificationInternal_OnCommand(
    HWND hwndDlg, NotificationSettingsState* state, WPARAM wParam) {
    WORD controlId = LOWORD(wParam);
    WORD notificationCode = HIWORD(wParam);

    if (controlId == IDC_NOTIFICATION_EDIT1 &&
        notificationCode == EN_CHANGE) {
        if (state && !state->isInitializing) {
            wchar_t newText[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE];
            GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, newText,
                            sizeof(newText) / sizeof(wchar_t));
            if (newText[0] == L'\0') {
                wcscpy_s(newText, NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE, L" ");
            }
            DialogNotificationInternal_UpdatePreviewText(hwndDlg, newText);
        }
        return TRUE;
    }

    if (controlId == IDC_DISABLE_NOTIFICATION_CHECK &&
        notificationCode == BN_CLICKED) {
        BOOL isChecked = IsDlgButtonChecked(
            hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED;
        EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT),
                     !isChecked);
        return TRUE;
    }
    if (controlId == IDC_NOTIFICATION_USE_FOR_POMODORO &&
        notificationCode == BN_CLICKED) {
        BOOL enabled = IsDlgButtonChecked(
            hwndDlg, IDC_NOTIFICATION_USE_FOR_POMODORO) == BST_CHECKED;
        ShowWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_POMODORO_VARIABLES),
                   enabled ? SW_SHOW : SW_HIDE);
        return TRUE;
    }

    if (controlId == IDOK) {
        SaveNotificationSettings(hwndDlg, state);
        return TRUE;
    }
    if (controlId == IDCANCEL) {
        DialogNotificationInternal_CloseWithoutSaving(hwndDlg, state, TRUE);
        return TRUE;
    }
    if (controlId == IDC_TEST_SOUND_BUTTON) {
        HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
        HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
        if (state) {
            HandleSoundTestButton(hwndDlg, hwndCombo, hwndSlider,
                                  &state->isPlaying);
        }
        return TRUE;
    }
    if (controlId == IDC_OPEN_SOUND_DIR_BUTTON) {
        HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
        HandleSoundDirButton(hwndDlg, hwndCombo);
        return TRUE;
    }
    if (controlId == IDC_NOTIFICATION_SOUND_COMBO &&
        notificationCode == CBN_DROPDOWN) {
        HandleSoundComboDropdown(
            GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO));
        return TRUE;
    }
    if (controlId == IDC_NOTIFICATION_SOUND_COMBO &&
        notificationCode == CBN_CLOSEUP) {
        if (state && state->soundComboRefreshPending) {
            RefreshNotificationSoundComboBox(
                GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO));
            state->soundComboRefreshPending = FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

void DialogNotificationInternal_OnDestroy(
    HWND hwndDlg, NotificationSettingsState* state) {
    CleanupAudioPlayback(state && state->isPlaying);
    if (state) state->isPlaying = FALSE;
    DialogNotificationInternal_ClosePreview();
    SetAudioPlaybackCompleteCallback(NULL, NULL);
    NotificationSoundCache_SetNotifyWindow(NULL);
    Dialog_UnregisterInstanceForWindow(
        DIALOG_INSTANCE_NOTIFICATION_FULL, hwndDlg);
    DialogNotificationInternal_SetDialog(NULL);
    SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, 0);
    free(state);
}
