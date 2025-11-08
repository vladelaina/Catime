/**
 * @file dialog_notification_settings.c
 * @brief Comprehensive notification settings dialog (all-in-one)
 */

#include "dialog/dialog_notification.h"
#include "dialog/dialog_notification_audio.h"
#include "dialog/dialog_procedure.h"
#include "dialog/dialog_language.h"
#include "config.h"
#include "audio_player.h"
#include "notification.h"
#include "../resource/resource.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

static HWND g_hwndNotificationSettingsDialog = NULL;
static HWND g_hwndPreviewNotification = NULL;

/* ============================================================================
 * Preview Notification Helper
 * ============================================================================ */

static HWND FindPreviewNotificationWindow(void) {
    return FindWindowW(NOTIFICATION_CLASS_NAME, L"Catime Notification");
}

static void UpdatePreviewOpacity(int opacity) {
    if (!g_hwndPreviewNotification || !IsWindow(g_hwndPreviewNotification)) {
        g_hwndPreviewNotification = FindPreviewNotificationWindow();
    }
    
    if (g_hwndPreviewNotification && IsWindow(g_hwndPreviewNotification)) {
        BYTE alphaValue = (BYTE)((opacity * 255) / 100);
        SetLayeredWindowAttributes(g_hwndPreviewNotification, 0, alphaValue, LWA_ALPHA);
    }
}

static void ShowOpacityPreviewNotification(HWND hwndParent, int initialOpacity, const wchar_t* message) {
    extern void ShowToastNotification(HWND hwnd, const wchar_t* message);
    
    if (g_hwndPreviewNotification && IsWindow(g_hwndPreviewNotification)) {
        return;
    }
    
    int originalOpacity = g_AppConfig.notification.display.max_opacity;
    int originalTimeout = g_AppConfig.notification.display.timeout_ms;
    
    g_AppConfig.notification.display.max_opacity = initialOpacity;
    g_AppConfig.notification.display.timeout_ms = 999999;
    
    wchar_t previewMessage[256];
    if (message && message[0] != L'\0') {
        wcsncpy(previewMessage, message, sizeof(previewMessage)/sizeof(wchar_t) - 1);
        previewMessage[sizeof(previewMessage)/sizeof(wchar_t) - 1] = L'\0';
    } else {
        MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.timeout_message, -1, 
                           previewMessage, sizeof(previewMessage)/sizeof(wchar_t));
    }
    
    ShowToastNotification(hwndParent, previewMessage);
    
    g_hwndPreviewNotification = FindPreviewNotificationWindow();
    
    if (g_hwndPreviewNotification && IsWindow(g_hwndPreviewNotification)) {
        KillTimer(g_hwndPreviewNotification, NOTIFICATION_TIMER_ID);
        KillTimer(g_hwndPreviewNotification, ANIMATION_TIMER_ID);
        
        BYTE alphaValue = (BYTE)((initialOpacity * 255) / 100);
        SetLayeredWindowAttributes(g_hwndPreviewNotification, 0, alphaValue, LWA_ALPHA);
    }
    
    g_AppConfig.notification.display.max_opacity = originalOpacity;
    g_AppConfig.notification.display.timeout_ms = originalTimeout;
}

typedef struct {
    wchar_t* messageText;
    int windowWidth;
    BYTE opacity;
} PreviewNotificationData;

static void UpdatePreviewNotificationText(HWND hwndDlg, const wchar_t* newText) {
    if (!g_hwndPreviewNotification || !IsWindow(g_hwndPreviewNotification)) {
        HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
        int currentOpacity = (int)SendMessage(hwndOpacitySlider, TBM_GETPOS, 0, 0);
        
        ShowOpacityPreviewNotification(GetParent(hwndDlg), currentOpacity, newText);
        return;
    }
    
    PreviewNotificationData* data = (PreviewNotificationData*)GetWindowLongPtr(g_hwndPreviewNotification, GWLP_USERDATA);
    if (data && data->messageText) {
        size_t newLen = wcslen(newText) + 1;
        wchar_t* newBuffer = (wchar_t*)realloc(data->messageText, newLen * sizeof(wchar_t));
        if (newBuffer) {
            data->messageText = newBuffer;
            wcscpy(data->messageText, newText);
            InvalidateRect(g_hwndPreviewNotification, NULL, TRUE);
            UpdateWindow(g_hwndPreviewNotification);
        }
    }
}

static void ClosePreviewNotification(void) {
    if (g_hwndPreviewNotification && IsWindow(g_hwndPreviewNotification)) {
        DestroyWindow(g_hwndPreviewNotification);
        g_hwndPreviewNotification = NULL;
    }
}

/* ============================================================================
 * Full Notification Settings Dialog
 * ============================================================================ */

void ShowNotificationSettingsDialog(HWND hwndParent) {
    if (!g_hwndNotificationSettingsDialog) {
        ReadNotificationMessagesConfig();
        ReadNotificationTimeoutConfig();
        ReadNotificationOpacityConfig();
        ReadNotificationTypeConfig();
        ReadNotificationSoundConfig();
        ReadNotificationVolumeConfig();
        
        DialogBoxW(GetModuleHandle(NULL), 
                  MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG), 
                  hwndParent, 
                  NotificationSettingsDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationSettingsDialog);
    }
}

INT_PTR CALLBACK NotificationSettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static BOOL isPlaying = FALSE;
    static int originalVolume = 0;
    static BOOL isInitializing = TRUE;
    
    switch (msg) {
        case WM_INITDIALOG: {
            ReadNotificationMessagesConfig();
            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            ReadNotificationTypeConfig();
            ReadNotificationSoundConfig();
            ReadNotificationVolumeConfig();
            
            originalVolume = g_AppConfig.notification.sound.volume;
            
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG);
            
            wchar_t wideText[256];
            
            MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.timeout_message, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);
            
            SYSTEMTIME st = {0};
            GetLocalTime(&st);
            
            ReadNotificationDisabledConfig();
            
            CheckDlgButton(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK, 
                          g_AppConfig.notification.display.disabled ? BST_CHECKED : BST_UNCHECKED);
            
            EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), 
                        !g_AppConfig.notification.display.disabled);
            
            int totalSeconds = g_AppConfig.notification.display.timeout_ms / 1000;
            st.wHour = totalSeconds / 3600;
            st.wMinute = (totalSeconds % 3600) / 60;
            st.wSecond = totalSeconds % 60;
            
            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_SETSYSTEMTIME, 
                              GDT_VALID, (LPARAM)&st);

            HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
            SendMessage(hwndOpacitySlider, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
            SendMessage(hwndOpacitySlider, TBM_SETPOS, TRUE, g_AppConfig.notification.display.max_opacity);
            
            wchar_t opacityText[16];
            _snwprintf_s(opacityText, 16, _TRUNCATE, L"%d%%", g_AppConfig.notification.display.max_opacity);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
            
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
            
            g_hwndNotificationSettingsDialog = hwndDlg;
            
            MoveDialogToPrimaryScreen(hwndDlg);
            
            isInitializing = FALSE;
            
            return TRUE;
        }
        
        case WM_HSCROLL: {
            if (GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER) == (HWND)lParam) {
                int volume = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                
                wchar_t volumeText[16];
                _snwprintf_s(volumeText, 16, _TRUNCATE, L"%d%%", volume);
                SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
                
                SetAudioVolume(volume);
                
                return TRUE;
            }
            else if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT) == (HWND)lParam) {
                int opacity = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                
                wchar_t opacityText[16];
                _snwprintf_s(opacityText, 16, _TRUNCATE, L"%d%%", opacity);
                SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
                
                wchar_t currentMessage[256];
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, currentMessage, sizeof(currentMessage)/sizeof(wchar_t));
                
                ShowOpacityPreviewNotification(GetParent(hwndDlg), opacity, currentMessage[0] != L'\0' ? currentMessage : NULL);
                UpdatePreviewOpacity(opacity);
                
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_NOTIFICATION_EDIT1 && HIWORD(wParam) == EN_CHANGE) {
                if (!isInitializing) {
                    wchar_t newText[256];
                    GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, newText, sizeof(newText)/sizeof(wchar_t));
                    
                    if (newText[0] == L'\0') {
                        wcscpy(newText, L" ");
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
                ClosePreviewNotification();
                
                wchar_t wTimeout[256] = {0};
                
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));
                
                char timeout_msg[256] = {0};
                
                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1, 
                                   timeout_msg, sizeof(timeout_msg), NULL, NULL);
                
                SYSTEMTIME st = {0};
                
                BOOL isDisabled = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                
                WriteConfigNotificationDisabled(isDisabled);
                
                if (SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID) {
                    int totalSeconds = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
                    
                    if (totalSeconds == 0) {
                        WriteConfigNotificationTimeout(0);
                    } else if (!isDisabled) {
                        WriteConfigNotificationTimeout(totalSeconds * 1000);
                    }
                }

                HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
                int opacity = (int)SendMessage(hwndOpacitySlider, TBM_GETPOS, 0, 0);
                if (opacity >= 1 && opacity <= 100) {
                    WriteConfigNotificationOpacity(opacity);
                }
                
                NotificationType notifType = NOTIFICATION_TYPE_CATIME;
                if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME)) {
                    notifType = NOTIFICATION_TYPE_CATIME;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_OS)) {
                    notifType = NOTIFICATION_TYPE_OS;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL)) {
                    notifType = NOTIFICATION_TYPE_SYSTEM_MODAL;
                }
                WriteConfigNotificationType(notifType);
                
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                char soundFile[MAX_PATH] = {0};
                
                if (index > 0) {
                    wchar_t wFileName[MAX_PATH];
                    SendMessageW(hwndCombo, CB_GETLBTEXT, index, (LPARAM)wFileName);
                    
                    const wchar_t* sysBeepText = GetLocalizedString(NULL, L"System Beep");
                    if (wcscmp(wFileName, sysBeepText) == 0) {
                        strncpy(soundFile, "SYSTEM_BEEP", sizeof(soundFile) - 1);
                        soundFile[sizeof(soundFile) - 1] = '\0';
                    } else {
                        char audio_path[MAX_PATH];
                        GetAudioFolderPath(audio_path, MAX_PATH);
                        
                        char fileName[MAX_PATH];
                        WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL);
                        
                        snprintf(soundFile, sizeof(soundFile), "%s\\%s", audio_path, fileName);
                    }
                }
                
                HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                
                WriteConfigNotificationMessages(timeout_msg);
                WriteConfigNotificationSound(soundFile);
                WriteConfigNotificationVolume(volume);
                
                CleanupAudioPlayback(isPlaying);
                isPlaying = FALSE;
                
                isInitializing = TRUE;
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                ClosePreviewNotification();
                
                SetAudioVolume(originalVolume);
                
                CleanupAudioPlayback(isPlaying);
                isPlaying = FALSE;
                
                isInitializing = TRUE;
                
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationSettingsDialog = NULL;
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
            
        case WM_APP + 100:
            isPlaying = FALSE;
            return TRUE;
            
        case WM_CLOSE:
            ClosePreviewNotification();
            CleanupAudioPlayback(isPlaying);
            
            isInitializing = TRUE;
            
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndNotificationSettingsDialog = NULL;
            return TRUE;
            
        case WM_DESTROY:
            ClosePreviewNotification();
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            g_hwndNotificationSettingsDialog = NULL;
            isInitializing = TRUE;
            break;
    }
    return FALSE;
}

