/**
 * @file dialog_notification_controls.c
 * @brief Handles notification settings slider changes.
 */

#include "dialog/dialog_notification_settings_internal.h"
#include "audio_player.h"
#include "config.h"
#include "dialog/dialog_notification_audio.h"
#include "language.h"
#include "../resource/resource.h"

#include <stdio.h>

INT_PTR DialogNotificationInternal_OnHScroll(
    HWND hwndDlg, NotificationSettingsState* state, LPARAM lParam) {
    if (GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER) == (HWND)lParam) {
        int volume = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

        wchar_t volumeText[16];
        _snwprintf_s(volumeText, 16, _TRUNCATE, L"%d%%", volume);
        SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
        SetAudioVolume(volume);

        if (state && !state->isInitializing && !state->isPlaying) {
            HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
            char soundFile[MAX_PATH] = {0};
            if (!GetSelectedNotificationSoundFile(
                    hwndCombo, soundFile, sizeof(soundFile))) {
                return TRUE;
            }

            if (soundFile[0] != '\0' &&
                PreviewNotificationSoundFile(hwndDlg, soundFile)) {
                SetAudioVolume(volume);
                SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON,
                                GetLocalizedString(NULL, L"Stop"));
                state->isPlaying = TRUE;
            }
        }
        return TRUE;
    }

    if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT) ==
        (HWND)lParam) {
        int opacity = DialogNotificationInternal_ClampOpacity(
            (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0));
        wchar_t opacityText[16];
        _snwprintf_s(opacityText, 16, _TRUNCATE, L"%d%%", opacity);
        SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
        DialogNotificationInternal_RefreshPreview(hwndDlg);
        return TRUE;
    }

    if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER) ==
        (HWND)lParam) {
        int cornerRadius =
            (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
        wchar_t radiusText[16];
        _snwprintf_s(radiusText, 16, _TRUNCATE, L"%dpx", cornerRadius);
        SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_RADIUS_TEXT, radiusText);
        DialogNotificationInternal_RefreshPreview(hwndDlg);
        return TRUE;
    }

    if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER) ==
        (HWND)lParam) {
        int fontPercent =
            (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
        wchar_t fontSizeText[16];
        _snwprintf_s(fontSizeText, 16, _TRUNCATE, L"%d%%", fontPercent);
        SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_TEXT,
                        fontSizeText);
        DialogNotificationInternal_RefreshPreview(hwndDlg);
        return TRUE;
    }

    return FALSE;
}
