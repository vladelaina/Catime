/**
 * @file dialog_notification_audio.c
 * @brief Audio file management for notification settings
 */

#include "dialog/dialog_notification_audio.h"
#include "config.h"
#include "language.h"
#include "audio_player.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Static Helper Functions
 * ============================================================================ */

/**
 * @brief Audio playback completion callback
 * @param hwnd Dialog window handle
 */
static void OnAudioPlaybackComplete(HWND hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        SetDlgItemTextW(hwnd, IDC_TEST_SOUND_BUTTON, GetLocalizedString(NULL, L"Test"));
        SendMessage(hwnd, WM_APP + 100, 0, 0);
    }
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void PopulateNotificationSoundComboBox(HWND hwndCombo, const char* currentFile) {
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

    if (currentFile && currentFile[0] != '\0') {
        if (strcmp(currentFile, "SYSTEM_BEEP") == 0) {
            SendMessage(hwndCombo, CB_SETCURSEL, 1, 0);
        } else {
            wchar_t wSoundFile[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, currentFile, -1, wSoundFile, MAX_PATH);
            
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

BOOL HandleSoundTestButton(HWND hwndDlg, HWND hwndCombo, HWND hwndSlider, BOOL* isPlaying) {
    if (!hwndDlg || !hwndCombo || !hwndSlider || !isPlaying) return FALSE;

    if (!(*isPlaying)) {
        int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
        
        if (index > 0) {
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
                *isPlaying = TRUE;
            }
            
            strncpy(g_AppConfig.notification.sound.sound_file, tempSoundFile, 
                   sizeof(g_AppConfig.notification.sound.sound_file) - 1);
            g_AppConfig.notification.sound.sound_file[sizeof(g_AppConfig.notification.sound.sound_file) - 1] = '\0';
        }
    } else {
        StopNotificationSound();
        SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(NULL, L"Test"));
        *isPlaying = FALSE;
    }
    
    return TRUE;
}

void HandleSoundDirButton(HWND hwndDlg, HWND hwndCombo) {
    if (!hwndDlg || !hwndCombo) return;

    char audio_path[MAX_PATH];
    GetAudioFolderPath(audio_path, MAX_PATH);
    
    wchar_t wAudioPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);
    
    ShellExecuteW(hwndDlg, L"open", wAudioPath, NULL, NULL, SW_SHOWNORMAL);
    
    int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
    wchar_t selectedFile[MAX_PATH] = {0};
    if (selectedIndex > 0) {
        SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
    }
    
    const char* currentFile = (selectedIndex > 0) ? g_AppConfig.notification.sound.sound_file : NULL;
    PopulateNotificationSoundComboBox(hwndCombo, currentFile);
    
    if (selectedFile[0] != L'\0') {
        int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
        if (newIndex != CB_ERR) {
            SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
        } else {
            SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
        }
    }
}

void HandleSoundComboDropdown(HWND hwndCombo) {
    if (!hwndCombo) return;

    int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
    wchar_t selectedFile[MAX_PATH] = {0};
    if (selectedIndex > 0) {
        SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
    }
    
    const char* currentFile = (selectedIndex > 0) ? g_AppConfig.notification.sound.sound_file : NULL;
    PopulateNotificationSoundComboBox(hwndCombo, currentFile);
    
    if (selectedFile[0] != L'\0') {
        int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
        if (newIndex != CB_ERR) {
            SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
        }
    }
}

void SetupAudioPlaybackCallback(HWND hwndDlg) {
    SetAudioPlaybackCompleteCallback(hwndDlg, OnAudioPlaybackComplete);
}

void CleanupAudioPlayback(BOOL isPlaying) {
    if (isPlaying) {
        StopNotificationSound();
    }
    SetAudioPlaybackCompleteCallback(NULL, NULL);
}

