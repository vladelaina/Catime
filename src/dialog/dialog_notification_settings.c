/**
 * @file dialog_notification_settings.c
 * @brief Comprehensive notification settings dialog (all-in-one)
 */

#include "dialog/dialog_notification.h"
#include "dialog/dialog_notification_audio.h"
#include "dialog/dialog_procedure.h"
#include "dialog/dialog_language.h"
#include "dialog/dialog_common.h"
#include "config.h"
#include "config/config_defaults.h"
#include "audio_player.h"
#include "notification.h"
#include "log.h"
#include "../resource/resource.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <wchar.h>

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

/* ============================================================================
 * Global State
 * ============================================================================ */

static HWND g_hwndNotificationSettingsDialog = NULL;
static HWND g_hwndPreviewNotification = NULL;

static BOOL IsCurrentProcessWindow(HWND hwnd) {
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

static BOOL IsValidNotificationSettingsParent(HWND hwnd) {
    return IsCurrentProcessWindow(hwnd) &&
           IsWindowOfClass(hwnd, CATIME_MAIN_WINDOW_CLASS_NAME);
}

static HWND GetNotificationSettingsParent(HWND hwndDlg) {
    HWND hwndParent = hwndDlg ? GetParent(hwndDlg) : NULL;
    return IsValidNotificationSettingsParent(hwndParent) ? hwndParent : NULL;
}

static BOOL IsCurrentNotificationSettingsDialog(HWND hwnd) {
    return hwnd &&
           hwnd == g_hwndNotificationSettingsDialog &&
           IsWindow(hwnd) &&
           Dialog_IsOpen(DIALOG_INSTANCE_NOTIFICATION_FULL);
}

static BOOL ConvertNotificationSettingsTextToUtf8(const wchar_t* source, char* dest, size_t destSize) {
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

/* ============================================================================
 * Preview Notification Helper
 * ============================================================================ */

static HWND FindPreviewNotificationWindow(void) {
    HWND hwnd = NULL;

    while ((hwnd = FindWindowExW(NULL, hwnd, NOTIFICATION_CLASS_NAME, L"Catime Notification")) != NULL) {
        if (IsCurrentProcessWindow(hwnd) && IsToastNotificationPreviewWindow(hwnd)) {
            return hwnd;
        }
    }

    return NULL;
}

static void UpdatePreviewOpacity(int opacity) {
    if (!g_hwndPreviewNotification ||
        !IsWindow(g_hwndPreviewNotification) ||
        !IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        g_hwndPreviewNotification = FindPreviewNotificationWindow();
    }

    if (g_hwndPreviewNotification &&
        IsWindow(g_hwndPreviewNotification) &&
        IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        SetToastNotificationOpacity(g_hwndPreviewNotification, opacity);
    }
}

static void UpdatePreviewCornerRadius(int cornerRadius) {
    if (!g_hwndPreviewNotification ||
        !IsWindow(g_hwndPreviewNotification) ||
        !IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        g_hwndPreviewNotification = FindPreviewNotificationWindow();
    }

    if (g_hwndPreviewNotification &&
        IsWindow(g_hwndPreviewNotification) &&
        IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        SetToastNotificationCornerRadius(g_hwndPreviewNotification, cornerRadius);
    }
}

static void UpdatePreviewFontPercent(int fontPercent) {
    if (!g_hwndPreviewNotification ||
        !IsWindow(g_hwndPreviewNotification) ||
        !IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        g_hwndPreviewNotification = FindPreviewNotificationWindow();
    }

    if (g_hwndPreviewNotification &&
        IsWindow(g_hwndPreviewNotification) &&
        IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        SetToastNotificationFontPercent(g_hwndPreviewNotification, fontPercent);
    }
}

static int GetTrackbarPosition(HWND hwndDlg, int controlId, int fallback) {
    HWND hwndTrackbar = GetDlgItem(hwndDlg, controlId);
    if (!hwndTrackbar) {
        return fallback;
    }

    return (int)SendMessage(hwndTrackbar, TBM_GETPOS, 0, 0);
}

static int ClampNotificationOpacityForDialog(int opacity) {
    if (opacity < MIN_VISIBLE_OPACITY) {
        return MIN_VISIBLE_OPACITY;
    }
    if (opacity > MAX_OPACITY) {
        return MAX_OPACITY;
    }
    return opacity;
}

static void EnsurePreviewNotification(HWND hwndDlg, const wchar_t* message) {
    HWND hwndParent = GetNotificationSettingsParent(hwndDlg);
    if (!IsValidNotificationSettingsParent(hwndParent)) {
        return;
    }

    /* Reuse the existing preview window when it is still open. */
    if (g_hwndPreviewNotification &&
        IsWindow(g_hwndPreviewNotification) &&
        IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        return;
    }

    /* Also check by finding window in case handle became stale */
    HWND existingPreview = FindPreviewNotificationWindow();
    if (existingPreview && IsWindow(existingPreview)) {
        g_hwndPreviewNotification = existingPreview;
        return;
    }

    wchar_t previewMessage[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE] = {0};
    if (message && message[0] != L'\0') {
        wcsncpy(previewMessage, message, sizeof(previewMessage)/sizeof(wchar_t) - 1);
        previewMessage[sizeof(previewMessage)/sizeof(wchar_t) - 1] = L'\0';
    } else {
        MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.timeout_message, -1,
                           previewMessage, sizeof(previewMessage)/sizeof(wchar_t));
    }

    int initialOpacity = ClampNotificationOpacityForDialog(
        GetTrackbarPosition(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT,
                            g_AppConfig.notification.display.max_opacity));
    ShowToastNotificationPreview(hwndParent, previewMessage, initialOpacity);

    g_hwndPreviewNotification = FindPreviewNotificationWindow();
}

static void UpdatePreviewAppearanceFromControls(HWND hwndDlg) {
    UpdatePreviewOpacity(ClampNotificationOpacityForDialog(
        GetTrackbarPosition(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT,
                            g_AppConfig.notification.display.max_opacity)));
    UpdatePreviewCornerRadius(GetTrackbarPosition(hwndDlg, IDC_NOTIFICATION_RADIUS_SLIDER,
                                                  g_AppConfig.notification.display.corner_radius));
    UpdatePreviewFontPercent(GetTrackbarPosition(hwndDlg, IDC_NOTIFICATION_FONT_SIZE_SLIDER,
                                                 g_AppConfig.notification.display.font_size));
}

static void RefreshPreviewFromControls(HWND hwndDlg) {
    wchar_t currentMessage[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE];
    GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, currentMessage,
                    sizeof(currentMessage) / sizeof(wchar_t));

    EnsurePreviewNotification(hwndDlg, currentMessage[0] != L'\0' ? currentMessage : NULL);
    UpdatePreviewAppearanceFromControls(hwndDlg);
}

static void UpdatePreviewNotificationText(HWND hwndDlg, const wchar_t* newText) {
    if (!g_hwndPreviewNotification ||
        !IsWindow(g_hwndPreviewNotification) ||
        !IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        EnsurePreviewNotification(hwndDlg, newText);
        UpdatePreviewAppearanceFromControls(hwndDlg);
        return;
    }

    SetToastNotificationMessage(g_hwndPreviewNotification, newText);
}

static void ClosePreviewNotification(void) {
    if (g_hwndPreviewNotification &&
        IsWindow(g_hwndPreviewNotification) &&
        IsToastNotificationPreviewWindow(g_hwndPreviewNotification)) {
        DestroyWindow(g_hwndPreviewNotification);
        g_hwndPreviewNotification = NULL;
    }
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
    static BOOL isPlaying = FALSE;
    static int originalVolume = 0;
    static BOOL isInitializing = TRUE;

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_NOTIFICATION_FULL, hwndDlg);
            g_hwndNotificationSettingsDialog = hwndDlg;

            originalVolume = g_AppConfig.notification.sound.volume;

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

            isPlaying = FALSE;

            SetupAudioPlaybackCallback(hwndDlg);
            NotificationSoundCache_SetNotifyWindow(hwndDlg);

            g_hwndNotificationSettingsDialog = hwndDlg;

            MoveDialogToPrimaryScreen(hwndDlg);

            isInitializing = FALSE;

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

                if (!isInitializing && !isPlaying) {
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
                            isPlaying = TRUE;
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
                if (!isInitializing) {
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

                ClosePreviewNotification();

                wchar_t wTimeout[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE] = {0};

                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));

                char timeout_msg[NOTIFICATION_MESSAGE_BUFFER_SIZE] = {0};

                if (!ConvertNotificationSettingsTextToUtf8(wTimeout, timeout_msg, sizeof(timeout_msg))) {
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

                CleanupAudioPlayback(isPlaying);
                isPlaying = FALSE;

                isInitializing = TRUE;

                DestroyWindow(hwndDlg);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                ClosePreviewNotification();

                SetAudioVolume(originalVolume);

                CleanupAudioPlayback(isPlaying);
                isPlaying = FALSE;

                isInitializing = TRUE;

                DestroyWindow(hwndDlg);
                return TRUE;
            } else if (LOWORD(wParam) == IDC_TEST_SOUND_BUTTON) {
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                HandleSoundTestButton(hwndDlg, hwndCombo, hwndSlider, &isPlaying);
                return TRUE;
            } else if (LOWORD(wParam) == IDC_OPEN_SOUND_DIR_BUTTON) {
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                HandleSoundDirButton(hwndDlg, hwndCombo);
                return TRUE;
            } else if (LOWORD(wParam) == IDC_NOTIFICATION_SOUND_COMBO && HIWORD(wParam) == CBN_DROPDOWN) {
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                HandleSoundComboDropdown(hwndCombo);
                return TRUE;
            }
            break;

        case WM_NOTIFICATION_SOUND_PLAYBACK_COMPLETE:
            isPlaying = FALSE;
            SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(NULL, L"Test"));
            return TRUE;

        case WM_NOTIFICATION_SOUND_CACHE_UPDATED: {
            HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
            if (hwndCombo && !SendMessage(hwndCombo, CB_GETDROPPEDSTATE, 0, 0)) {
                RefreshNotificationSoundComboBox(hwndCombo);
            }
            return TRUE;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                ClosePreviewNotification();
                SetAudioVolume(originalVolume);
                CleanupAudioPlayback(isPlaying);
                isPlaying = FALSE;
                isInitializing = TRUE;
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            ClosePreviewNotification();
            SetAudioVolume(originalVolume);
            CleanupAudioPlayback(isPlaying);

            isInitializing = TRUE;

            DestroyWindow(hwndDlg);
            return TRUE;

        case WM_DESTROY:
            CleanupAudioPlayback(isPlaying);
            isPlaying = FALSE;
            ClosePreviewNotification();
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            NotificationSoundCache_SetNotifyWindow(NULL);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_NOTIFICATION_FULL, hwndDlg);
            g_hwndNotificationSettingsDialog = NULL;
            isInitializing = TRUE;
            break;
    }
    return FALSE;
}
