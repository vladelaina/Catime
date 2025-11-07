/**
 * @file audio_player.c
 * @brief Three-tier audio fallback (miniaudio → PlaySound → beep)
 * 
 * Why three tiers: Unicode paths fail miniaudio, WAV-only fails PlaySound, beep never fails.
 * Path encoding: UTF-8 → short path (8.3) → ANSI for miniaudio compatibility.
 * Completion: miniaudio polls 500ms, PlaySound 3s timeout (no API), beep 500ms fixed.
 */
#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include "../libs/miniaudio/miniaudio.h"
#include "config.h"

#define TIMER_INTERVAL_AUDIO_CHECK 500
#define TIMER_INTERVAL_FALLBACK    3000
#define TIMER_INTERVAL_BEEP        500

typedef void (*AudioPlaybackCompleteCallback)(HWND hwnd);

static ma_engine g_audioEngine;
static ma_sound g_sound;
static ma_bool32 g_engineInitialized = MA_FALSE;
static ma_bool32 g_soundInitialized = MA_FALSE;

static ma_bool32 g_isPlaying = MA_FALSE;
static ma_bool32 g_isPaused = MA_FALSE;
static AudioPlaybackCompleteCallback g_audioCompleteCallback = NULL;
static HWND g_audioCallbackHwnd = NULL;
static UINT_PTR g_audioTimerId = 0;

static void CALLBACK AudioTimerCallback(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime);
static BOOL FallbackToPlaySound(HWND hwnd, const wchar_t* wFilePath);
static BOOL FallbackToSystemBeep(HWND hwnd);
static void ResetPlaybackState(void);
static void StartPlaybackTimer(HWND hwnd, UINT timerId, UINT interval);

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/** GetFileAttributes cheaper than CreateFile, rejects directories */
static BOOL AudioFileExists(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;

    wchar_t wFilePath[MAX_PATH * 2] = {0};
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH * 2);

    DWORD dwAttrib = GetFileAttributesW(wFilePath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
            !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

/** Basic validation (prevents misconfig crashes, not full security) */
static BOOL IsValidFilePath(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    if (strchr(filePath, '=') != NULL) return FALSE;
    if (strlen(filePath) >= MAX_PATH) return FALSE;
    return TRUE;
}

static void ResetPlaybackState(void) {
    g_isPlaying = MA_FALSE;
    g_isPaused = MA_FALSE;
    g_audioTimerId = 0;
}

/** One timer at a time (kills previous) */
static void StartPlaybackTimer(HWND hwnd, UINT timerId, UINT interval) {
    if (g_audioTimerId != 0) {
        KillTimer(hwnd, g_audioTimerId);
    }
    g_audioTimerId = SetTimer(hwnd, timerId, interval, (TIMERPROC)AudioTimerCallback);
}

/* ============================================================================
 * Fallback mechanisms
 * ============================================================================ */

/** Tier 2: PlaySound (WAV only) */
static BOOL FallbackToPlaySound(HWND hwnd, const wchar_t* wFilePath) {
    if (!PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
        return FALSE;
    }
    
    StartPlaybackTimer(hwnd, TIMER_ID_PLAYSOUND_DONE, TIMER_INTERVAL_FALLBACK);
    g_isPlaying = MA_TRUE;
    return TRUE;
}

/** Tier 3: System beep (never fails) */
static BOOL FallbackToSystemBeep(HWND hwnd) {
    MessageBeep(MB_OK);
    StartPlaybackTimer(hwnd, TIMER_ID_SYSTEM_BEEP_DONE, TIMER_INTERVAL_BEEP);
    g_isPlaying = MA_TRUE;
    return TRUE;
}

static void ShowErrorMessage(HWND hwnd, const wchar_t* errorMsg) {
    MessageBoxW(hwnd, errorMsg, L"Audio Playback Error", MB_ICONERROR | MB_OK);
}

/* ============================================================================
 * Audio engine management
 * ============================================================================ */

static BOOL InitializeAudioEngine(void) {
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

static void UninitializeAudioEngine(void) {
    if (g_engineInitialized) {
        if (g_soundInitialized) {
            ma_sound_uninit(&g_sound);
            g_soundInitialized = MA_FALSE;
        }
        ma_engine_uninit(&g_audioEngine);
        g_engineInitialized = MA_FALSE;
    }
}

/* ============================================================================
 * Path conversion utilities
 * ============================================================================ */

/** UTF-8 → short path (8.3) → ANSI for miniaudio */
static BOOL ConvertPathForMiniaudio(const char* utf8Path, char* outPath, size_t outPathSize) {
    wchar_t wFilePath[MAX_PATH * 2] = {0};
    
    /* UTF-8 to Unicode */
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wFilePath, MAX_PATH * 2) == 0) {
        return FALSE;
    }
    
    /* Try short path for ASCII compatibility */
    wchar_t shortPath[MAX_PATH] = {0};
    DWORD shortPathLen = GetShortPathNameW(wFilePath, shortPath, MAX_PATH);
    
    if (shortPathLen > 0 && shortPathLen < MAX_PATH) {
        if (WideCharToMultiByte(CP_ACP, 0, shortPath, -1, outPath, (int)outPathSize, NULL, NULL) > 0) {
            return TRUE;
        }
    }
    
    /* Fallback to UTF-8 encoding */
    WideCharToMultiByte(CP_UTF8, 0, wFilePath, -1, outPath, (int)outPathSize, NULL, NULL);
    return TRUE;
}

static BOOL GetWideCharPath(const char* utf8Path, wchar_t* wPath, size_t wPathSize) {
    return MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, (int)wPathSize) > 0;
}

/* ============================================================================
 * Audio playback core
 * ============================================================================ */

static ma_result LoadAudioFile(const char* convertedPath) {
    if (g_soundInitialized) {
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
    }
    
    ma_result result = ma_sound_init_from_file(&g_audioEngine, convertedPath, 0, NULL, NULL, &g_sound);
    if (result == MA_SUCCESS) {
        g_soundInitialized = MA_TRUE;
    }
    return result;
}

static ma_result StartAudioPlayback(void) {
    if (!g_soundInitialized) {
        return MA_ERROR;
    }
    
    ma_result result = ma_sound_start(&g_sound);
    if (result != MA_SUCCESS) {
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
    }
    return result;
}

/** Tier 1: miniaudio with automatic fallback */
static BOOL PlayAudioWithMiniaudio(HWND hwnd, const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    
    if (!g_engineInitialized && !InitializeAudioEngine()) {
        return FALSE;
    }
    
    float volume = (float)g_AppConfig.notification.sound.volume / 100.0f;
    ma_engine_set_volume(&g_audioEngine, volume);
    
    wchar_t wFilePath[MAX_PATH * 2] = {0};
    if (!GetWideCharPath(filePath, wFilePath, MAX_PATH * 2)) {
        return FALSE;
    }
    
    char convertedPath[MAX_PATH * 4] = {0};
    if (!ConvertPathForMiniaudio(filePath, convertedPath, sizeof(convertedPath))) {
        return FallbackToPlaySound(hwnd, wFilePath);
    }
    
    ma_result result = LoadAudioFile(convertedPath);
    if (result != MA_SUCCESS) {
        return FallbackToPlaySound(hwnd, wFilePath);
    }
    
    if (StartAudioPlayback() != MA_SUCCESS) {
        return FallbackToPlaySound(hwnd, wFilePath);
    }
    
    g_isPlaying = MA_TRUE;
    StartPlaybackTimer(hwnd, TIMER_ID_MINIAUDIO_CHECK, TIMER_INTERVAL_AUDIO_CHECK);
    return TRUE;
}

/* ============================================================================
 * Timer callbacks
 * ============================================================================ */

/** Unified completion detector (polls miniaudio, timeouts for others) */
static void CALLBACK AudioTimerCallback(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime) {
    (void)message;
    (void)dwTime;
    
    BOOL shouldStop = FALSE;
    
    switch (idEvent) {
        case TIMER_ID_MINIAUDIO_CHECK:
            if (g_engineInitialized && g_soundInitialized) {
                if (!ma_sound_is_playing(&g_sound) && !g_isPaused) {
                    if (g_soundInitialized) {
                        ma_sound_uninit(&g_sound);
                        g_soundInitialized = MA_FALSE;
                    }
                    shouldStop = TRUE;
                }
            } else {
                shouldStop = TRUE;
            }
            break;
            
        case TIMER_ID_PLAYSOUND_DONE:
        case TIMER_ID_SYSTEM_BEEP_DONE:
            shouldStop = TRUE;
            break;
    }
    
    if (shouldStop) {
        KillTimer(hwnd, idEvent);
        ResetPlaybackState();
        
        if (g_audioCompleteCallback) {
            g_audioCompleteCallback(g_audioCallbackHwnd);
        }
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

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

void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback) {
    g_audioCallbackHwnd = hwnd;
    g_audioCompleteCallback = callback;
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
    }

    ResetPlaybackState();
}

BOOL PlayNotificationSound(HWND hwnd) {
    CleanupAudioResources();
    g_audioCallbackHwnd = hwnd;

    if (g_AppConfig.notification.sound.sound_file[0] == '\0') {
        return TRUE;
    }

    if (strcmp(g_AppConfig.notification.sound.sound_file, "SYSTEM_BEEP") == 0) {
        return FallbackToSystemBeep(hwnd);
    }

    if (!IsValidFilePath(g_AppConfig.notification.sound.sound_file)) {
        wchar_t errorMsg[MAX_PATH + 64];
        StringCbPrintfW(errorMsg, sizeof(errorMsg), 
                       L"Invalid audio file path:\n%hs", g_AppConfig.notification.sound.sound_file);
        ShowErrorMessage(hwnd, errorMsg);
        return FallbackToSystemBeep(hwnd);
    }

    if (!AudioFileExists(g_AppConfig.notification.sound.sound_file)) {
        wchar_t errorMsg[MAX_PATH + 64];
        StringCbPrintfW(errorMsg, sizeof(errorMsg), 
                       L"Cannot find the configured audio file:\n%hs", g_AppConfig.notification.sound.sound_file);
        ShowErrorMessage(hwnd, errorMsg);
        return FallbackToSystemBeep(hwnd);
    }

    if (PlayAudioWithMiniaudio(hwnd, g_AppConfig.notification.sound.sound_file)) {
        return TRUE;
    }

    return FallbackToSystemBeep(hwnd);
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
