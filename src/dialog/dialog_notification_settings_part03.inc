}

/**
 * @brief Update opacity slider and text in settings dialog
 * @param opacity New opacity value (10-100)
 *
 * @details Called from preview window when opacity changes via mouse wheel
 */
void UpdateNotificationOpacityControls(int opacity) {
    if (!IsCurrentNotificationSettingsDialog(g_hwndNotificationSettingsDialog)) {
        return;
    }

    opacity = ClampNotificationOpacityForDialog(opacity);

    HWND hwndOpacitySlider = GetDlgItem(g_hwndNotificationSettingsDialog, IDC_NOTIFICATION_OPACITY_EDIT);
    if (hwndOpacitySlider) {
        SendMessage(hwndOpacitySlider, TBM_SETPOS, TRUE, opacity);

        wchar_t opacityText[16];
        _snwprintf_s(opacityText, 16, _TRUNCATE, L"%d%%", opacity);
        SetDlgItemTextW(g_hwndNotificationSettingsDialog, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
    }
}

/**
 * @brief Update font percentage slider and text in settings dialog
 * @param fontPercent Text height as a percentage of the notification window height
 *
 * @details Called from preview window when font percentage changes via Ctrl + mouse wheel
 */
void UpdateNotificationFontPercentControls(int fontPercent) {
    if (!IsCurrentNotificationSettingsDialog(g_hwndNotificationSettingsDialog)) {
        return;
    }

    HWND hwndFontSizeSlider = GetDlgItem(g_hwndNotificationSettingsDialog, IDC_NOTIFICATION_FONT_SIZE_SLIDER);
    if (hwndFontSizeSlider) {
        SendMessage(hwndFontSizeSlider, TBM_SETPOS, TRUE, fontPercent);

        wchar_t fontSizeText[16];
        _snwprintf_s(fontSizeText, 16, _TRUNCATE, L"%d%%", fontPercent);
        SetDlgItemTextW(g_hwndNotificationSettingsDialog, IDC_NOTIFICATION_FONT_SIZE_TEXT, fontSizeText);
    }
}

/* ============================================================================
 * Full Notification Settings Dialog
 * ============================================================================ */

void ShowNotificationSettingsDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_NOTIFICATION_FULL)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_NOTIFICATION_FULL);
        SetForegroundWindow(existing);
        return;
    }

    if (!IsValidNotificationSettingsParent(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateDialogW(GetModuleHandle(NULL),
              MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG),
              hwndParent,
              NotificationSettingsDlgProc);

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}

INT_PTR CALLBACK NotificationSettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    NotificationSettingsState* state = GetNotificationSettingsState(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            state = (NotificationSettingsState*)calloc(1, sizeof(*state));
            if (!state) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            state->isInitializing = TRUE;
            state->originalVolume = g_AppConfig.notification.sound.volume;
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)state);

            Dialog_InitializeInstance(DIALOG_INSTANCE_NOTIFICATION_FULL, hwndDlg);
            g_hwndNotificationSettingsDialog = hwndDlg;

            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG);

            wchar_t wideText[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE] = {0};

            MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.timeout_message, -1,
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);

            SYSTEMTIME st = {0};
            GetLocalTime(&st);

            CheckDlgButton(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK,
                          g_AppConfig.notification.display.disabled ? BST_CHECKED : BST_UNCHECKED);

            EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT),
                        !g_AppConfig.notification.display.disabled);

            int totalSeconds = g_AppConfig.notification.display.timeout_ms / 1000;
            st.wHour = (WORD)(totalSeconds / 3600);
            st.wMinute = (WORD)((totalSeconds % 3600) / 60);
            st.wSecond = (WORD)(totalSeconds % 60);

            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_SETSYSTEMTIME,
                              GDT_VALID, (LPARAM)&st);

            HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
            int notificationOpacity = ClampNotificationOpacityForDialog(
                g_AppConfig.notification.display.max_opacity);
            SendMessage(hwndOpacitySlider, TBM_SETRANGE, TRUE, MAKELONG(MIN_VISIBLE_OPACITY, 100));
            SendMessage(hwndOpacitySlider, TBM_SETPOS, TRUE, notificationOpacity);

            wchar_t opacityText[16];
            _snwprintf_s(opacityText, 16, _TRUNCATE, L"%d%%", notificationOpacity);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);

            HWND hwndRadiusSlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER);
            SendMessage(hwndRadiusSlider, TBM_SETRANGE, TRUE,
                        MAKELONG(MIN_NOTIFICATION_CORNER_RADIUS, MAX_NOTIFICATION_CORNER_RADIUS));
            SendMessage(hwndRadiusSlider, TBM_SETPOS, TRUE, g_AppConfig.notification.display.corner_radius);

            wchar_t radiusText[16];
            _snwprintf_s(radiusText, 16, _TRUNCATE, L"%dpx", g_AppConfig.notification.display.corner_radius);
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

            HWND hwndFontSizeSlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER);
            SendMessage(hwndFontSizeSlider, TBM_SETRANGE, TRUE,
                        MAKELONG(MIN_NOTIFICATION_FONT_SIZE, MAX_NOTIFICATION_FONT_SIZE));
            SendMessage(hwndFontSizeSlider, TBM_SETPOS, TRUE, notificationFontPercent);

            wchar_t fontSizeText[16];
            _snwprintf_s(fontSizeText, 16, _TRUNCATE, L"%d%%", notificationFontPercent);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_TEXT, fontSizeText);

            switch (g_AppConfig.notification.display.type) {
                case NOTIFICATION_TYPE_CATIME:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME, BST_CHECKED);
                    break;
                case NOTIFICATION_TYPE_OS:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_OS, BST_CHECKED);
                    break;
                case NOTIFICATION_TYPE_SYSTEM_MODAL:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL, BST_CHECKED);
                    break;
            }

            HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
            PopulateNotificationSoundComboBox(hwndCombo, g_AppConfig.notification.sound.sound_file);

            HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
            SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
            SendMessage(hwndSlider, TBM_SETPOS, TRUE, g_AppConfig.notification.sound.volume);

            wchar_t volumeText[16];
            _snwprintf_s(volumeText, 16, _TRUNCATE, L"%d%%", g_AppConfig.notification.sound.volume);
            SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);

            state->isPlaying = FALSE;

            SetupAudioPlaybackCallback(hwndDlg);
            NotificationSoundCache_SetNotifyWindow(hwndDlg);

            g_hwndNotificationSettingsDialog = hwndDlg;

            LayoutNotificationSettingsDialog(hwndDlg);

            MoveDialogToPrimaryScreen(hwndDlg);

            state->isInitializing = FALSE;

            RefreshPreviewFromControls(hwndDlg);

            return TRUE;
        }

        case WM_HSCROLL: {
            if (GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER) == (HWND)lParam) {
                int volume = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

                wchar_t volumeText[16];
                _snwprintf_s(volumeText, 16, _TRUNCATE, L"%d%%", volume);
                SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);

                SetAudioVolume(volume);

                if (state && !state->isInitializing && !state->isPlaying) {
                    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                    char soundFile[MAX_PATH] = {0};
                    if (!GetSelectedNotificationSoundFile(hwndCombo, soundFile, sizeof(soundFile))) {
                        return TRUE;
                    }

                    if (soundFile[0] != '\0') {
                        if (PreviewNotificationSoundFile(hwndDlg, soundFile)) {
                            SetAudioVolume(volume);
                            SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON,
                                            GetLocalizedString(NULL, L"Stop"));
                            state->isPlaying = TRUE;
                        }
                    }
                }

                return TRUE;
            }
            else if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT) == (HWND)lParam) {
                int opacity = ClampNotificationOpacityForDialog(
                    (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0));

                wchar_t opacityText[16];
                _snwprintf_s(opacityText, 16, _TRUNCATE, L"%d%%", opacity);
                SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);

                RefreshPreviewFromControls(hwndDlg);

                return TRUE;
            }
            else if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER) == (HWND)lParam) {
                int cornerRadius = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

                wchar_t radiusText[16];
                _snwprintf_s(radiusText, 16, _TRUNCATE, L"%dpx", cornerRadius);
                SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_RADIUS_TEXT, radiusText);

                RefreshPreviewFromControls(hwndDlg);

                return TRUE;
            }
            else if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER) == (HWND)lParam) {
                int fontPercent = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

                wchar_t fontSizeText[16];
                _snwprintf_s(fontSizeText, 16, _TRUNCATE, L"%d%%", fontPercent);
                SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_TEXT, fontSizeText);

                RefreshPreviewFromControls(hwndDlg);

                return TRUE;
            }
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_NOTIFICATION_EDIT1 && HIWORD(wParam) == EN_CHANGE) {
                if (state && !state->isInitializing) {
                    wchar_t newText[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE];
                    GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, newText, sizeof(newText)/sizeof(wchar_t));

                    if (newText[0] == L'\0') {
                        wcscpy_s(newText, NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE, L" ");
                    }
                    UpdatePreviewNotificationText(hwndDlg, newText);
                }
                return TRUE;
            }
            else if (LOWORD(wParam) == IDC_DISABLE_NOTIFICATION_CHECK && HIWORD(wParam) == BN_CLICKED) {
                BOOL isChecked = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !isChecked);
                return TRUE;
            }
            else if (LOWORD(wParam) == IDOK) {
                if (g_hwndPreviewNotification &&
                    IsWindow(g_hwndPreviewNotification) &&
