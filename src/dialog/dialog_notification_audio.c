/**
 * @file dialog_notification_audio.c
 * @brief Notification sound selection and preview actions
 */

#include "dialog_notification_audio_internal.h"

static void OnAudioPlaybackComplete(HWND hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(
            hwnd, WM_NOTIFICATION_SOUND_PLAYBACK_COMPLETE, 0, 0);
    }
}

BOOL GetSelectedNotificationSoundFile(
    HWND combo, char* outSoundFile, size_t outSize) {
    if (!combo || !outSoundFile || outSize == 0) {
        return FALSE;
    }
    outSoundFile[0] = '\0';

    LRESULT index = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (index == CB_ERR || index <= 0) {
        return TRUE;
    }

    LRESULT textLength = SendMessageW(
        combo, CB_GETLBTEXTLEN, (WPARAM)index, 0);
    if (textLength == CB_ERR || textLength < 0 || textLength >= MAX_PATH) {
        return FALSE;
    }

    wchar_t wideFileName[MAX_PATH] = {0};
    if (SendMessageW(combo, CB_GETLBTEXT, (WPARAM)index,
                     (LPARAM)wideFileName) == CB_ERR) {
        return FALSE;
    }
    wideFileName[MAX_PATH - 1] = L'\0';

    const wchar_t* systemBeepText = GetLocalizedString(NULL, L"System Beep");
    if (systemBeepText && wcscmp(wideFileName, systemBeepText) == 0) {
        int written = snprintf(
            outSoundFile, outSize, "%s", "SYSTEM_BEEP");
        return written >= 0 && (size_t)written < outSize;
    }

    char audioPath[MAX_PATH] = {0};
    GetAudioFolderPath(audioPath, MAX_PATH);
    if (audioPath[0] == '\0') {
        return FALSE;
    }

    char fileName[MAX_PATH] = {0};
    if (WideCharToMultiByte(CP_UTF8, 0, wideFileName, -1,
                            fileName, MAX_PATH, NULL, NULL) <= 0) {
        return FALSE;
    }

    int pathLength = snprintf(
        outSoundFile, outSize, "%s\\%s", audioPath, fileName);
    if (pathLength < 0 || (size_t)pathLength >= outSize) {
        outSoundFile[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

BOOL HandleSoundTestButton(
    HWND dialog, HWND combo, HWND slider, BOOL* isPlaying) {
    if (!dialog || !combo || !slider || !isPlaying) {
        return FALSE;
    }

    if (!*isPlaying) {
        char soundFile[MAX_PATH] = {0};
        if (!GetSelectedNotificationSoundFile(
                combo, soundFile, sizeof(soundFile))) {
            return FALSE;
        }
        if (soundFile[0] != '\0') {
            int volume = (int)SendMessageW(slider, TBM_GETPOS, 0, 0);
            SetAudioVolume(volume);
            if (PreviewNotificationSoundFile(dialog, soundFile)) {
                SetDlgItemTextW(
                    dialog, IDC_TEST_SOUND_BUTTON,
                    GetLocalizedString(NULL, L"Stop"));
                *isPlaying = TRUE;
            }
        }
    } else {
        StopNotificationSound();
        SetDlgItemTextW(
            dialog, IDC_TEST_SOUND_BUTTON,
            GetLocalizedString(NULL, L"Test"));
        *isPlaying = FALSE;
    }
    return TRUE;
}

void HandleSoundDirButton(HWND dialog, HWND combo) {
    if (!dialog || !combo) {
        return;
    }

    char audioPath[MAX_PATH] = {0};
    GetAudioFolderPath(audioPath, MAX_PATH);
    if (audioPath[0] == '\0') {
        return;
    }

    wchar_t wideAudioPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, audioPath, -1,
                            wideAudioPath, MAX_PATH) <= 0) {
        return;
    }
    ShellExecuteW(dialog, L"open", wideAudioPath, NULL, NULL, SW_SHOWNORMAL);
    NotificationAudio_RequestCacheScanAsync();
}

void HandleSoundComboDropdown(HWND combo) {
    if (combo) {
        NotificationAudio_RequestCacheScanAsync();
    }
}

void SetupAudioPlaybackCallback(HWND dialog) {
    SetAudioPlaybackCompleteCallback(dialog, OnAudioPlaybackComplete);
}

void CleanupAudioPlayback(BOOL isPlaying) {
    if (isPlaying) {
        StopNotificationSound();
    }
    SetAudioPlaybackCompleteCallback(NULL, NULL);
}
