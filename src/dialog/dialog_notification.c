/**
 * @file dialog_notification.c
 * @brief Notification configuration dialogs implementation
 */

#include "dialog/dialog_notification.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_procedure.h"
#include "language.h"
#include "dialog/dialog_language.h"
#include "config.h"
#include "notification.h"
#include "audio_player.h"
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

            wchar_t wideText[100];

            MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.timeout_message, -1,
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);

            MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.pomodoro_message, -1,
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);

            MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.cycle_complete_message, -1,
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

            StringCbPrintfW(wbuffer, sizeof(wbuffer), L"%.1f", (float)g_AppConfig.notification.display.timeout_ms / 1000.0f);
            /* Remove trailing .0 */
            if (wcslen(wbuffer) > 2 && wbuffer[wcslen(wbuffer)-2] == L'.' && wbuffer[wcslen(wbuffer)-1] == L'0') {
                wbuffer[wcslen(wbuffer)-2] = L'\0';
            }
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wbuffer);

            StringCbPrintfW(wbuffer, sizeof(wbuffer), L"%d", g_AppConfig.notification.display.max_opacity);
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
 * Full Notification Settings Dialog
 * ============================================================================ */

static HWND g_hwndNotificationSettingsDialog = NULL;

/**
 * @brief Audio playback completion callback for test sound button
 * @param hwnd Dialog window handle
 */
static void OnAudioPlaybackComplete(HWND hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        SetDlgItemTextW(hwnd, IDC_TEST_SOUND_BUTTON, GetLocalizedString(NULL, L"Test"));
        SendMessage(hwnd, WM_APP + 100, 0, 0);
    }
}

/**
 * @brief Populate sound combo box with available audio files
 * @param hwndDlg Dialog window handle
 */
static void PopulateSoundComboBox(HWND hwndDlg) {
    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
    if (!hwndCombo) return;

    SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0);

    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(NULL, L"None"));
    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(NULL, L"System Beep"));

    char audio_path[MAX_PATH];
    GetAudioFolderPath(audio_path, MAX_PATH);
    
    wchar_t wAudioPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);

    wchar_t wSearchPath[MAX_PATH];
    _snwprintf_s(wSearchPath, MAX_PATH, _TRUNCATE, L"%s\\*.*", wAudioPath);

    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(wSearchPath, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            wchar_t* ext = wcsrchr(find_data.cFileName, L'.');
            if (ext && (
                _wcsicmp(ext, L".flac") == 0 ||
                _wcsicmp(ext, L".mp3") == 0 ||
                _wcsicmp(ext, L".wav") == 0
            )) {
                SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)find_data.cFileName);
            }
        } while (FindNextFileW(hFind, &find_data));
        FindClose(hFind);
    }

    if (g_AppConfig.notification.sound.sound_file[0] != '\0') {
        if (strcmp(g_AppConfig.notification.sound.sound_file, "SYSTEM_BEEP") == 0) {
            SendMessage(hwndCombo, CB_SETCURSEL, 1, 0);
        } else {
            wchar_t wSoundFile[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.sound.sound_file, -1, wSoundFile, MAX_PATH);
            
            wchar_t* fileName = wcsrchr(wSoundFile, L'\\');
            if (fileName) fileName++;
            else fileName = wSoundFile;
            
            int index = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)fileName);
            if (index != CB_ERR) {
                SendMessage(hwndCombo, CB_SETCURSEL, index, 0);
            } else {
                SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
            }
        }
    } else {
        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
    }
}

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
            
            MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.pomodoro_message, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);
            
            MultiByteToWideChar(CP_UTF8, 0, g_AppConfig.notification.messages.cycle_complete_message, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);
            
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
            
            PopulateSoundComboBox(hwndDlg);
            
            HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
            SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
            SendMessage(hwndSlider, TBM_SETPOS, TRUE, g_AppConfig.notification.sound.volume);
            
            wchar_t volumeText[16];
            _snwprintf_s(volumeText, 16, _TRUNCATE, L"%d%%", g_AppConfig.notification.sound.volume);
            SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
            
            isPlaying = FALSE;
            
            SetAudioPlaybackCompleteCallback(hwndDlg, OnAudioPlaybackComplete);
            
            g_hwndNotificationSettingsDialog = hwndDlg;
            
            MoveDialogToPrimaryScreen(hwndDlg);
            
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
                
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_DISABLE_NOTIFICATION_CHECK && HIWORD(wParam) == BN_CLICKED) {
                BOOL isChecked = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !isChecked);
                return TRUE;
            }
            else if (LOWORD(wParam) == IDOK) {
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
                
                WriteConfigNotificationMessages(timeout_msg, pomodoro_msg, cycle_complete_msg);
                WriteConfigNotificationSound(soundFile);
                WriteConfigNotificationVolume(volume);
                
                if (isPlaying) {
                    StopNotificationSound();
                    isPlaying = FALSE;
                }
                
                SetAudioPlaybackCompleteCallback(NULL, NULL);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                if (isPlaying) {
                    StopNotificationSound();
                    isPlaying = FALSE;
                }
                
                SetAudioVolume(originalVolume);
                
                SetAudioPlaybackCompleteCallback(NULL, NULL);
                
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDC_TEST_SOUND_BUTTON) {
                if (!isPlaying) {
                    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                    int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                    
                    if (index > 0) {
                        HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                        int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                        SetAudioVolume(volume);
                        
                        wchar_t wFileName[MAX_PATH];
                        SendMessageW(hwndCombo, CB_GETLBTEXT, index, (LPARAM)wFileName);
                        
                        char tempSoundFile[MAX_PATH];
                        strncpy(tempSoundFile, g_AppConfig.notification.sound.sound_file, sizeof(tempSoundFile) - 1);
                        tempSoundFile[sizeof(tempSoundFile) - 1] = '\0';
                        
                        const wchar_t* sysBeepText = GetLocalizedString(NULL, L"System Beep");
                        if (wcscmp(wFileName, sysBeepText) == 0) {
                            strncpy(g_AppConfig.notification.sound.sound_file, "SYSTEM_BEEP", 
                                   sizeof(g_AppConfig.notification.sound.sound_file) - 1);
                            g_AppConfig.notification.sound.sound_file[sizeof(g_AppConfig.notification.sound.sound_file) - 1] = '\0';
                        } else {
                            char audio_path[MAX_PATH];
                            GetAudioFolderPath(audio_path, MAX_PATH);
                            
                            char fileName[MAX_PATH];
                            WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL);
                            
                            snprintf(g_AppConfig.notification.sound.sound_file, 
                                    sizeof(g_AppConfig.notification.sound.sound_file),
                                    "%s\\%s", audio_path, fileName);
                        }
                        
                        if (PlayNotificationSound(hwndDlg)) {
                            SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(NULL, L"Stop"));
                            isPlaying = TRUE;
                        }
                        
                        strncpy(g_AppConfig.notification.sound.sound_file, tempSoundFile, 
                               sizeof(g_AppConfig.notification.sound.sound_file) - 1);
                        g_AppConfig.notification.sound.sound_file[sizeof(g_AppConfig.notification.sound.sound_file) - 1] = '\0';
                    }
                } else {
                    StopNotificationSound();
                    SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(NULL, L"Test"));
                    isPlaying = FALSE;
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDC_OPEN_SOUND_DIR_BUTTON) {
                char audio_path[MAX_PATH];
                GetAudioFolderPath(audio_path, MAX_PATH);
                
                wchar_t wAudioPath[MAX_PATH];
                MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);
                
                ShellExecuteW(hwndDlg, L"open", wAudioPath, NULL, NULL, SW_SHOWNORMAL);
                
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                wchar_t selectedFile[MAX_PATH] = {0};
                if (selectedIndex > 0) {
                    SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
                }
                
                PopulateSoundComboBox(hwndDlg);
                
                if (selectedFile[0] != L'\0') {
                    int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
                    if (newIndex != CB_ERR) {
                        SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
                    } else {
                        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
                    }
                }
                
                return TRUE;
            } else if (LOWORD(wParam) == IDC_NOTIFICATION_SOUND_COMBO && HIWORD(wParam) == CBN_DROPDOWN) {
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                
                int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                wchar_t selectedFile[MAX_PATH] = {0};
                if (selectedIndex > 0) {
                    SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
                }
                
                PopulateSoundComboBox(hwndDlg);
                
                if (selectedFile[0] != L'\0') {
                    int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
                    if (newIndex != CB_ERR) {
                        SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
                    }
                }
                
                return TRUE;
            }
            break;
            
        case WM_APP + 100:
            isPlaying = FALSE;
            return TRUE;
            
        case WM_CLOSE:
            if (isPlaying) {
                StopNotificationSound();
            }
            
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndNotificationSettingsDialog = NULL;
            return TRUE;
            
        case WM_DESTROY:
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            g_hwndNotificationSettingsDialog = NULL;
            break;
    }
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

