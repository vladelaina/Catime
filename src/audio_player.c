#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include "../libs/miniaudio/miniaudio.h"

#include "config.h"

extern char NOTIFICATION_SOUND_FILE[MAX_PATH];
extern int NOTIFICATION_SOUND_VOLUME;

typedef void (*AudioPlaybackCompleteCallback)(HWND hwnd);

static ma_engine g_audioEngine;
static ma_sound g_sound;
static ma_bool32 g_engineInitialized = MA_FALSE;
static ma_bool32 g_soundInitialized = MA_FALSE;

static AudioPlaybackCompleteCallback g_audioCompleteCallback = NULL;
static HWND g_audioCallbackHwnd = NULL;
static UINT_PTR g_audioTimerId = 0;

static ma_bool32 g_isPlaying = MA_FALSE;
static ma_bool32 g_isPaused = MA_FALSE;

static void CheckAudioPlaybackComplete(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime);

static BOOL InitializeAudioEngine() {
    if (g_engineInitialized) {
        return TRUE;
    }

    ma_result result = ma_engine_init(NULL, &g_audioEngine);
    if (result != MA_SUCCESS) {
        return FALSE;
    }

    g_engineInitialized = MA_TRUE;
    return TRUE;
}

static void UninitializeAudioEngine() {
    if (g_engineInitialized) {
        if (g_soundInitialized) {
            ma_sound_uninit(&g_sound);
            g_soundInitialized = MA_FALSE;
        }

        ma_engine_uninit(&g_audioEngine);
        g_engineInitialized = MA_FALSE;
    }
}

static BOOL FileExists(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;

    wchar_t wFilePath[MAX_PATH * 2] = {0};
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH * 2);

    DWORD dwAttrib = GetFileAttributesW(wFilePath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
            !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static void ShowErrorMessage(HWND hwnd, const wchar_t* errorMsg) {
    MessageBoxW(hwnd, errorMsg, L"Audio Playback Error", MB_ICONERROR | MB_OK);
}

static void CALLBACK CheckAudioPlaybackComplete(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime) {
    if (g_engineInitialized && g_soundInitialized) {
        if (!ma_sound_is_playing(&g_sound) && !g_isPaused) {
            if (g_soundInitialized) {
                ma_sound_uninit(&g_sound);
                g_soundInitialized = MA_FALSE;
            }

            KillTimer(hwnd, idEvent);
            g_audioTimerId = 0;
            g_isPlaying = MA_FALSE;
            g_isPaused = MA_FALSE;

            if (g_audioCompleteCallback) {
                g_audioCompleteCallback(g_audioCallbackHwnd);
            }
        }
    } else {
        KillTimer(hwnd, idEvent);
        g_audioTimerId = 0;
        g_isPlaying = MA_FALSE;
        g_isPaused = MA_FALSE;

        if (g_audioCompleteCallback) {
            g_audioCompleteCallback(g_audioCallbackHwnd);
        }
    }
}

static void CALLBACK SystemBeepDoneCallback(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime) {
    KillTimer(hwnd, idEvent);
    g_audioTimerId = 0;
    g_isPlaying = MA_FALSE;
    g_isPaused = MA_FALSE;

    if (g_audioCompleteCallback) {
        g_audioCompleteCallback(g_audioCallbackHwnd);
    }
}

void SetAudioVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    if (g_engineInitialized) {
        float volFloat = (float)volume / 100.0f;
        ma_engine_set_volume(&g_audioEngine, volFloat);

        if (g_soundInitialized && g_isPlaying) {
            ma_sound_set_volume(&g_sound, volFloat);
        }
    }
}

static BOOL PlayAudioWithMiniaudio(HWND hwnd, const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;

    if (!g_engineInitialized) {
        if (!InitializeAudioEngine()) {
            return FALSE;
        }
    }

    float volume = (float)NOTIFICATION_SOUND_VOLUME / 100.0f;
    ma_engine_set_volume(&g_audioEngine, volume);

    if (g_soundInitialized) {
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
    }

    wchar_t wFilePath[MAX_PATH * 2] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH * 2) == 0) {
        DWORD error = GetLastError();
        wchar_t errorMsg[256];
        StringCbPrintfW(errorMsg, sizeof(errorMsg), L"Path conversion error (UTF-8->Unicode): %lu", error);
        ShowErrorMessage(hwnd, errorMsg);
        return FALSE;
    }

    wchar_t shortPath[MAX_PATH] = {0};
    DWORD shortPathLen = GetShortPathNameW(wFilePath, shortPath, MAX_PATH);
    if (shortPathLen == 0 || shortPathLen >= MAX_PATH) {
        DWORD error = GetLastError();

        if (PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1002, 3000, (TIMERPROC)SystemBeepDoneCallback);
            g_isPlaying = MA_TRUE;
            return TRUE;
        }

        wchar_t errorMsg[512];
        StringCbPrintfW(errorMsg, sizeof(errorMsg), L"Failed to get short path: %ls\nError code: %lu", wFilePath, error);
        ShowErrorMessage(hwnd, errorMsg);
        return FALSE;
    }

    char asciiPath[MAX_PATH] = {0};
    if (WideCharToMultiByte(CP_ACP, 0, shortPath, -1, asciiPath, MAX_PATH, NULL, NULL) == 0) {
        DWORD error = GetLastError();
        wchar_t errorMsg[256];
        StringCbPrintfW(errorMsg, sizeof(errorMsg), L"Path conversion error (Short Path->ASCII): %lu", error);
        ShowErrorMessage(hwnd, errorMsg);

        if (PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1002, 3000, (TIMERPROC)SystemBeepDoneCallback);
            g_isPlaying = MA_TRUE;
            return TRUE;
        }

        return FALSE;
    }

    ma_result result = ma_sound_init_from_file(&g_audioEngine, asciiPath, 0, NULL, NULL, &g_sound);

    if (result != MA_SUCCESS) {
        char utf8Path[MAX_PATH * 4] = {0};
        WideCharToMultiByte(CP_UTF8, 0, wFilePath, -1, utf8Path, sizeof(utf8Path), NULL, NULL);

        result = ma_sound_init_from_file(&g_audioEngine, utf8Path, 0, NULL, NULL, &g_sound);

        if (result != MA_SUCCESS) {
            if (PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
                if (g_audioTimerId != 0) {
                    KillTimer(hwnd, g_audioTimerId);
                }
                g_audioTimerId = SetTimer(hwnd, 1002, 3000, (TIMERPROC)SystemBeepDoneCallback);
                g_isPlaying = MA_TRUE;
                return TRUE;
            }

            wchar_t errorMsg[512];
            StringCbPrintfW(errorMsg, sizeof(errorMsg), L"Unable to load audio file: %ls\nError code: %d", wFilePath, result);
            ShowErrorMessage(hwnd, errorMsg);
            return FALSE;
        }
    }

    g_soundInitialized = MA_TRUE;

    if (ma_sound_start(&g_sound) != MA_SUCCESS) {
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;

        if (PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1002, 3000, (TIMERPROC)SystemBeepDoneCallback);
            g_isPlaying = MA_TRUE;
            return TRUE;
        }

        ShowErrorMessage(hwnd, L"Cannot start audio playback");
        return FALSE;
    }

    g_isPlaying = MA_TRUE;

    if (g_audioTimerId != 0) {
        KillTimer(hwnd, g_audioTimerId);
    }
    g_audioTimerId = SetTimer(hwnd, 1001, 500, (TIMERPROC)CheckAudioPlaybackComplete);

    return TRUE;
}

static BOOL IsValidFilePath(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;

    if (strchr(filePath, '=') != NULL) return FALSE;

    if (strlen(filePath) >= MAX_PATH) return FALSE;

    return TRUE;
}

void CleanupAudioResources(void) {
    PlaySoundW(NULL, NULL, SND_PURGE);

    if (g_engineInitialized && g_soundInitialized) {
        ma_sound_stop(&g_sound);
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
    }

    if (g_audioTimerId != 0 && g_audioCallbackHwnd != NULL) {
        KillTimer(g_audioCallbackHwnd, g_audioTimerId);
        g_audioTimerId = 0;
    }

    g_isPlaying = MA_FALSE;
    g_isPaused = MA_FALSE;
}

void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback) {
    g_audioCallbackHwnd = hwnd;
    g_audioCompleteCallback = callback;
}

BOOL PlayNotificationSound(HWND hwnd) {
    CleanupAudioResources();

    g_audioCallbackHwnd = hwnd;

    if (NOTIFICATION_SOUND_FILE[0] != '\0') {
        if (strcmp(NOTIFICATION_SOUND_FILE, "SYSTEM_BEEP") == 0) {
            MessageBeep(MB_OK);
            g_isPlaying = MA_TRUE;

            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1003, 500, (TIMERPROC)SystemBeepDoneCallback);

            return TRUE;
        }

        if (!IsValidFilePath(NOTIFICATION_SOUND_FILE)) {
            wchar_t errorMsg[MAX_PATH + 64];
            StringCbPrintfW(errorMsg, sizeof(errorMsg), L"Invalid audio file path:\n%hs", NOTIFICATION_SOUND_FILE);
            ShowErrorMessage(hwnd, errorMsg);

            MessageBeep(MB_OK);
            g_isPlaying = MA_TRUE;

            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1003, 500, (TIMERPROC)SystemBeepDoneCallback);

            return TRUE;
        }

        if (FileExists(NOTIFICATION_SOUND_FILE)) {
            if (PlayAudioWithMiniaudio(hwnd, NOTIFICATION_SOUND_FILE)) {
                return TRUE;
            }

            MessageBeep(MB_OK);
            g_isPlaying = MA_TRUE;

            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1003, 500, (TIMERPROC)SystemBeepDoneCallback);

            return TRUE;
        } else {
            wchar_t errorMsg[MAX_PATH + 64];
            StringCbPrintfW(errorMsg, sizeof(errorMsg), L"Cannot find the configured audio file:\n%hs", NOTIFICATION_SOUND_FILE);
            ShowErrorMessage(hwnd, errorMsg);

            MessageBeep(MB_OK);
            g_isPlaying = MA_TRUE;

            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1003, 500, (TIMERPROC)SystemBeepDoneCallback);

            return TRUE;
        }
    }

    return TRUE;
}

BOOL PauseNotificationSound(void) {
    if (g_isPlaying && !g_isPaused && g_engineInitialized && g_soundInitialized) {
        ma_sound_stop(&g_sound);
        g_isPaused = MA_TRUE;
        return TRUE;
    }
    return FALSE;
}

BOOL ResumeNotificationSound(void) {
    if (g_isPlaying && g_isPaused && g_engineInitialized && g_soundInitialized) {
        ma_sound_start(&g_sound);
        g_isPaused = MA_FALSE;
        return TRUE;
    }
    return FALSE;
}

void StopNotificationSound(void) {
    CleanupAudioResources();
}