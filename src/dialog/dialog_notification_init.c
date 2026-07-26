/**
 * @file dialog_notification_init.c
 * @brief Initializes notification settings state and controls.
 */

#include "dialog/dialog_notification_settings_internal.h"
#include "config.h"
#include "config/config_defaults.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_language.h"
#include "dialog/dialog_modern.h"
#include "dialog/dialog_notification_audio.h"
#include "dialog/dialog_procedure.h"
#include "notification.h"
#include "../resource/resource.h"

#include <stdio.h>
#include <stdlib.h>

INT_PTR DialogNotificationInternal_OnInit(HWND hwndDlg) {
    NotificationSettingsState* state =
        (NotificationSettingsState*)calloc(1, sizeof(*state));
    if (!state) {
        DestroyWindow(hwndDlg);
        return TRUE;
    }
    state->isInitializing = TRUE;
    state->originalVolume = g_AppConfig.notification.sound.volume;
    SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)state);

    Dialog_InitializeInstance(DIALOG_INSTANCE_NOTIFICATION_FULL, hwndDlg);
    DialogNotificationInternal_SetDialog(hwndDlg);
    ApplyDialogLanguage(hwndDlg, CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG);

    wchar_t wideText[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE] = {0};
    MultiByteToWideChar(
        CP_UTF8, 0, g_AppConfig.notification.messages.timeout_message, -1,
        wideText, sizeof(wideText) / sizeof(wchar_t));
    SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);

    SYSTEMTIME st = {0};
    GetLocalTime(&st);
    CheckDlgButton(
        hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK,
        g_AppConfig.notification.display.disabled ? BST_CHECKED : BST_UNCHECKED);
    EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT),
                 !g_AppConfig.notification.display.disabled);

    int totalSeconds = g_AppConfig.notification.display.timeout_ms / 1000;
    st.wHour = (WORD)(totalSeconds / 3600);
    st.wMinute = (WORD)((totalSeconds % 3600) / 60);
    st.wSecond = (WORD)(totalSeconds % 60);
    SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT,
                       DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);

    HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
    int notificationOpacity = DialogNotificationInternal_ClampOpacity(
        g_AppConfig.notification.display.max_opacity);
    SendMessage(hwndOpacitySlider, TBM_SETRANGE, TRUE,
                MAKELONG(MIN_VISIBLE_OPACITY, 100));
    SendMessage(hwndOpacitySlider, TBM_SETPOS, TRUE, notificationOpacity);

    wchar_t opacityText[16];
    _snwprintf_s(opacityText, 16, _TRUNCATE, L"%d%%", notificationOpacity);
    SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);

    HWND hwndRadiusSlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER);
    SendMessage(hwndRadiusSlider, TBM_SETRANGE, TRUE,
                MAKELONG(MIN_NOTIFICATION_CORNER_RADIUS,
                         MAX_NOTIFICATION_CORNER_RADIUS));
    SendMessage(hwndRadiusSlider, TBM_SETPOS, TRUE,
                g_AppConfig.notification.display.corner_radius);

    wchar_t radiusText[16];
    _snwprintf_s(radiusText, 16, _TRUNCATE, L"%dpx",
                 g_AppConfig.notification.display.corner_radius);
    SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_RADIUS_TEXT, radiusText);

    int notificationFontPercent = g_AppConfig.notification.display.font_size;
    if (notificationFontPercent <= 0) {
        notificationFontPercent = DEFAULT_NOTIFICATION_FONT_SIZE;
    }
    if (notificationFontPercent < MIN_NOTIFICATION_FONT_SIZE) {
        notificationFontPercent = MIN_NOTIFICATION_FONT_SIZE;
    }
    if (notificationFontPercent > MAX_NOTIFICATION_FONT_SIZE) {
        notificationFontPercent = MAX_NOTIFICATION_FONT_SIZE;
    }

    HWND hwndFontSizeSlider =
        GetDlgItem(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER);
    SendMessage(hwndFontSizeSlider, TBM_SETRANGE, TRUE,
                MAKELONG(MIN_NOTIFICATION_FONT_SIZE,
                         MAX_NOTIFICATION_FONT_SIZE));
    SendMessage(hwndFontSizeSlider, TBM_SETPOS, TRUE,
                notificationFontPercent);

    wchar_t fontSizeText[16];
    _snwprintf_s(fontSizeText, 16, _TRUNCATE, L"%d%%",
                 notificationFontPercent);
    SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_TEXT, fontSizeText);

    switch (g_AppConfig.notification.display.type) {
        case NOTIFICATION_TYPE_CATIME:
            CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME, BST_CHECKED);
            break;
        case NOTIFICATION_TYPE_OS:
            CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_OS, BST_CHECKED);
            break;
        case NOTIFICATION_TYPE_SYSTEM_MODAL:
            CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL,
                           BST_CHECKED);
            break;
    }

    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
    PopulateNotificationSoundComboBox(
        hwndCombo, g_AppConfig.notification.sound.sound_file);

    HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
    SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessage(hwndSlider, TBM_SETPOS, TRUE,
                g_AppConfig.notification.sound.volume);

    wchar_t volumeText[16];
    _snwprintf_s(volumeText, 16, _TRUNCATE, L"%d%%",
                 g_AppConfig.notification.sound.volume);
    SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);

    state->isPlaying = FALSE;
    SetupAudioPlaybackCallback(hwndDlg);
    NotificationSoundCache_SetNotifyWindow(hwndDlg);
    DialogNotificationInternal_SetDialog(hwndDlg);
    DialogNotificationInternal_Layout(hwndDlg);
    MoveDialogToPrimaryScreen(hwndDlg);
    state->isInitializing = FALSE;
    DialogNotificationInternal_RefreshPreview(hwndDlg);
    return TRUE;
}
