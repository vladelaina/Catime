                    IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
                    RECT rect;
                    if (GetWindowRect(g_hwndPreviewNotification, &rect)) {
                        if (!WriteConfigNotificationWindow(rect.left, rect.top,
                                                           rect.right - rect.left,
                                                           rect.bottom - rect.top)) {
                            LOG_WARNING("Failed to save notification preview window placement");
                            Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_EDIT1);
                            return TRUE;
                        }
                    }
                }

                wchar_t wTimeout[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE] = {0};

                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));

                char timeout_msg[NOTIFICATION_MESSAGE_BUFFER_SIZE] = {0};

                if (!WideToUtf8(wTimeout, timeout_msg, sizeof(timeout_msg))) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_EDIT1);
                    return TRUE;
                }

                SYSTEMTIME st = {0};

                BOOL isDisabled = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                int timeoutMs = g_AppConfig.notification.display.timeout_ms;
                if (SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID) {
                    int totalSeconds = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;

                    if (totalSeconds == 0) {
                        timeoutMs = 0;
                    } else if (!isDisabled) {
                        timeoutMs = totalSeconds * 1000;
                    }
                }

                HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
                int opacity = ClampNotificationOpacityForDialog(
                    (int)SendMessage(hwndOpacitySlider, TBM_GETPOS, 0, 0));

                HWND hwndRadiusSlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER);
                int cornerRadius = (int)SendMessage(hwndRadiusSlider, TBM_GETPOS, 0, 0);

                HWND hwndFontSizeSlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER);
                int fontPercent = (int)SendMessage(hwndFontSizeSlider, TBM_GETPOS, 0, 0);

                NotificationType notifType = NOTIFICATION_TYPE_CATIME;
                if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME)) {
                    notifType = NOTIFICATION_TYPE_CATIME;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_OS)) {
                    notifType = NOTIFICATION_TYPE_OS;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL)) {
                    notifType = NOTIFICATION_TYPE_SYSTEM_MODAL;
                }

                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                char soundFile[MAX_PATH] = {0};
                if (!GetSelectedNotificationSoundFile(hwndCombo, soundFile, sizeof(soundFile))) {
                    return TRUE;
                }

                HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);

                if (!WriteConfigNotificationSettings(timeout_msg, timeoutMs, opacity,
                                                     notifType, cornerRadius, fontPercent, isDisabled,
                                                     soundFile, volume)) {
                    Dialog_ShowErrorAndRefocus(hwndDlg, IDC_NOTIFICATION_EDIT1);
                    return TRUE;
                }

                ClosePreviewNotification();
                CleanupAudioPlayback(state && state->isPlaying);
                if (state) state->isPlaying = FALSE;

                if (state) state->isInitializing = TRUE;

                DestroyWindow(hwndDlg);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                ClosePreviewNotification();

                if (state) SetAudioVolume(state->originalVolume);

                CleanupAudioPlayback(state && state->isPlaying);
                if (state) state->isPlaying = FALSE;

                if (state) state->isInitializing = TRUE;

                DestroyWindow(hwndDlg);
                return TRUE;
            } else if (LOWORD(wParam) == IDC_TEST_SOUND_BUTTON) {
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                if (state) {
                    HandleSoundTestButton(hwndDlg, hwndCombo, hwndSlider,
                                          &state->isPlaying);
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDC_OPEN_SOUND_DIR_BUTTON) {
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                HandleSoundDirButton(hwndDlg, hwndCombo);
                return TRUE;
            } else if (LOWORD(wParam) == IDC_NOTIFICATION_SOUND_COMBO && HIWORD(wParam) == CBN_DROPDOWN) {
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                HandleSoundComboDropdown(hwndCombo);
                return TRUE;
            } else if (LOWORD(wParam) == IDC_NOTIFICATION_SOUND_COMBO && HIWORD(wParam) == CBN_CLOSEUP) {
                if (state && state->soundComboRefreshPending) {
                    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                    RefreshNotificationSoundComboBox(hwndCombo);
                    state->soundComboRefreshPending = FALSE;
                }
                return TRUE;
            }
            break;

        case WM_NOTIFICATION_SOUND_PLAYBACK_COMPLETE:
            if (state) state->isPlaying = FALSE;
            SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(NULL, L"Test"));
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
                ClosePreviewNotification();
                if (state) SetAudioVolume(state->originalVolume);
                CleanupAudioPlayback(state && state->isPlaying);
                if (state) {
                    state->isPlaying = FALSE;
                    state->isInitializing = TRUE;
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            ClosePreviewNotification();
            if (state) SetAudioVolume(state->originalVolume);
            CleanupAudioPlayback(state && state->isPlaying);

            if (state) state->isInitializing = TRUE;

            DestroyWindow(hwndDlg);
            return TRUE;

        case WM_DESTROY:
            CleanupAudioPlayback(state && state->isPlaying);
            if (state) state->isPlaying = FALSE;
            ClosePreviewNotification();
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            NotificationSoundCache_SetNotifyWindow(NULL);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_NOTIFICATION_FULL, hwndDlg);
            g_hwndNotificationSettingsDialog = NULL;
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, 0);
            free(state);
            break;
    }
    return FALSE;
}
