/**
 * @file audio_player.c
 * @brief Cross-platform audio playback with fallback mechanisms
 * @version 2.0 - Refactored for better maintainability
 */
#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include "../libs/miniaudio/miniaudio.h"
#include "config.h"

extern char NOTIFICATION_SOUND_FILE[MAX_PATH];
extern int NOTIFICATION_SOUND_VOLUME;

/** @brief Timer IDs for different playback types */
#define TIMER_ID_MINIAUDIO_CHECK  1001
#define TIMER_ID_PLAYSOUND_DONE   1002
#define TIMER_ID_SYSTEM_BEEP_DONE 1003

/** @brief Timer intervals in milliseconds */
#define TIMER_INTERVAL_AUDIO_CHECK 500
#define TIMER_INTERVAL_FALLBACK    3000
#define TIMER_INTERVAL_BEEP        500

/** @brief Audio playback completion callback type */
typedef void (*AudioPlaybackCompleteCallback)(HWND hwnd);

/** @brief Audio engine state */
static ma_engine g_audioEngine;
static ma_sound g_sound;
static ma_bool32 g_engineInitialized = MA_FALSE;
static ma_bool32 g_soundInitialized = MA_FALSE;

/** @brief Playback state tracking */
static ma_bool32 g_isPlaying = MA_FALSE;
static ma_bool32 g_isPaused = MA_FALSE;
static AudioPlaybackCompleteCallback g_audioCompleteCallback = NULL;
static HWND g_audioCallbackHwnd = NULL;
static UINT_PTR g_audioTimerId = 0;

/* ============================================================================
 * Forward declarations
 * ============================================================================ */
static void CALLBACK AudioTimerCallback(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime);
static BOOL FallbackToPlaySound(HWND hwnd, const wchar_t* wFilePath);
static BOOL FallbackToSystemBeep(HWND hwnd);
static void ResetPlaybackState(void);
static void StartPlaybackTimer(HWND hwnd, UINT timerId, UINT interval);

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * @brief Check if audio file exists on filesystem
 */
static BOOL FileExists(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;

    wchar_t wFilePath[MAX_PATH * 2] = {0};
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH * 2);

    DWORD dwAttrib = GetFileAttributesW(wFilePath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
            !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

/**
 * @brief Validate audio file path for security
 */
static BOOL IsValidFilePath(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    if (strchr(filePath, '=') != NULL) return FALSE;  // Reject potential injection
    if (strlen(filePath) >= MAX_PATH) return FALSE;
    return TRUE;
}

/**
 * @brief Reset all playback state flags
 */
static void ResetPlaybackState(void) {
    g_isPlaying = MA_FALSE;
    g_isPaused = MA_FALSE;
    g_audioTimerId = 0;
}

/**
 * @brief Start playback monitoring timer
 */
static void StartPlaybackTimer(HWND hwnd, UINT timerId, UINT interval) {
    if (g_audioTimerId != 0) {
        KillTimer(hwnd, g_audioTimerId);
    }
    g_audioTimerId = SetTimer(hwnd, timerId, interval, (TIMERPROC)AudioTimerCallback);
}

/* ============================================================================
 * Fallback mechanisms
 * ============================================================================ */

/**
 * @brief Fallback to Windows PlaySound API
 * @return TRUE if PlaySound succeeded, FALSE otherwise
 */
static BOOL FallbackToPlaySound(HWND hwnd, const wchar_t* wFilePath) {
    if (!PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
        return FALSE;
    }
    
    StartPlaybackTimer(hwnd, TIMER_ID_PLAYSOUND_DONE, TIMER_INTERVAL_FALLBACK);
    g_isPlaying = MA_TRUE;
    return TRUE;
}

/**
 * @brief Fallback to system beep with timer
 * @return Always TRUE (beep always succeeds)
 */
static BOOL FallbackToSystemBeep(HWND hwnd) {
    MessageBeep(MB_OK);
    StartPlaybackTimer(hwnd, TIMER_ID_SYSTEM_BEEP_DONE, TIMER_INTERVAL_BEEP);
    g_isPlaying = MA_TRUE;
    return TRUE;
}

/**
 * @brief Show error message dialog
 */
static void ShowErrorMessage(HWND hwnd, const wchar_t* errorMsg) {
    MessageBoxW(hwnd, errorMsg, L"Audio Playback Error", MB_ICONERROR | MB_OK);
}

/* ============================================================================
 * Audio engine management
 * ============================================================================ */

/**
 * @brief Initialize miniaudio engine
 */
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

/**
 * @brief Clean up audio engine and sound resources
 */
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

/**
 * @brief Convert UTF-8 path to best format for miniaudio
 * @param utf8Path Input UTF-8 path
 * @param outPath Output buffer for converted path
 * @param outPathSize Size of output buffer
 * @return TRUE if conversion succeeded
 */
static BOOL ConvertPathForMiniaudio(const char* utf8Path, char* outPath, size_t outPathSize) {
    wchar_t wFilePath[MAX_PATH * 2] = {0};
    
    // Step 1: UTF-8 to Unicode
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wFilePath, MAX_PATH * 2) == 0) {
        return FALSE;
    }
    
    // Step 2: Try short path for ASCII compatibility
    wchar_t shortPath[MAX_PATH] = {0};
    DWORD shortPathLen = GetShortPathNameW(wFilePath, shortPath, MAX_PATH);
    
    if (shortPathLen > 0 && shortPathLen < MAX_PATH) {
        // Convert short path to ANSI for miniaudio
        if (WideCharToMultiByte(CP_ACP, 0, shortPath, -1, outPath, (int)outPathSize, NULL, NULL) > 0) {
            return TRUE;
        }
    }
    
    // Step 3: Fallback to UTF-8 encoding
    WideCharToMultiByte(CP_UTF8, 0, wFilePath, -1, outPath, (int)outPathSize, NULL, NULL);
    return TRUE;
}

/**
 * @brief Get wide character path for fallback functions
 */
static BOOL GetWideCharPath(const char* utf8Path, wchar_t* wPath, size_t wPathSize) {
    return MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, (int)wPathSize) > 0;
}

/* ============================================================================
 * Audio playback core
 * ============================================================================ */

/**
 * @brief Load audio file into miniaudio sound object
 */
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

/**
 * @brief Start audio playback
 */
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

/**
 * @brief Play audio file using miniaudio with automatic fallback
 */
static BOOL PlayAudioWithMiniaudio(HWND hwnd, const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    
    // Initialize engine if needed
    if (!g_engineInitialized && !InitializeAudioEngine()) {
        return FALSE;
    }
    
    // Set volume
    float volume = (float)NOTIFICATION_SOUND_VOLUME / 100.0f;
    ma_engine_set_volume(&g_audioEngine, volume);
    
    // Get wide char path for fallback
    wchar_t wFilePath[MAX_PATH * 2] = {0};
    if (!GetWideCharPath(filePath, wFilePath, MAX_PATH * 2)) {
        return FALSE;
    }
    
    // Convert path for miniaudio
    char convertedPath[MAX_PATH * 4] = {0};
    if (!ConvertPathForMiniaudio(filePath, convertedPath, sizeof(convertedPath))) {
        return FallbackToPlaySound(hwnd, wFilePath);
    }
    
    // Try loading with converted path
    ma_result result = LoadAudioFile(convertedPath);
    if (result != MA_SUCCESS) {
        return FallbackToPlaySound(hwnd, wFilePath);
    }
    
    // Try starting playback
    if (StartAudioPlayback() != MA_SUCCESS) {
        return FallbackToPlaySound(hwnd, wFilePath);
    }
    
    // Success - start monitoring timer
    g_isPlaying = MA_TRUE;
    StartPlaybackTimer(hwnd, TIMER_ID_MINIAUDIO_CHECK, TIMER_INTERVAL_AUDIO_CHECK);
    return TRUE;
}

/* ============================================================================
 * Timer callbacks
 * ============================================================================ */

/**
 * @brief Unified timer callback for all playback types
 */
static void CALLBACK AudioTimerCallback(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime) {
    (void)message;
    (void)dwTime;
    
    BOOL shouldStop = FALSE;
    
    switch (idEvent) {
        case TIMER_ID_MINIAUDIO_CHECK:
            // Check if miniaudio sound finished
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
            // Simple timeout for fallback methods
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

/**
 * @brief Set audio playback volume
 */
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

/**
 * @brief Register callback for audio playback completion
 */
void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback) {
    g_audioCallbackHwnd = hwnd;
    g_audioCompleteCallback = callback;
}

/**
 * @brief Stop and clean up all audio playback resources
 */
void CleanupAudioResources(void) {
    // Stop all Windows PlaySound instances
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

/**
 * @brief Play notification sound with multiple fallback strategies
 */
BOOL PlayNotificationSound(HWND hwnd) {
    CleanupAudioResources();
    g_audioCallbackHwnd = hwnd;

    // No sound configured
    if (NOTIFICATION_SOUND_FILE[0] == '\0') {
        return TRUE;
    }

    // Special case: system beep mode
    if (strcmp(NOTIFICATION_SOUND_FILE, "SYSTEM_BEEP") == 0) {
        return FallbackToSystemBeep(hwnd);
    }

    // Validate file path for security
    if (!IsValidFilePath(NOTIFICATION_SOUND_FILE)) {
        wchar_t errorMsg[MAX_PATH + 64];
        StringCbPrintfW(errorMsg, sizeof(errorMsg), 
                       L"Invalid audio file path:\n%hs", NOTIFICATION_SOUND_FILE);
        ShowErrorMessage(hwnd, errorMsg);
        return FallbackToSystemBeep(hwnd);
    }

    // Check if file exists
    if (!FileExists(NOTIFICATION_SOUND_FILE)) {
        wchar_t errorMsg[MAX_PATH + 64];
        StringCbPrintfW(errorMsg, sizeof(errorMsg), 
                       L"Cannot find the configured audio file:\n%hs", NOTIFICATION_SOUND_FILE);
        ShowErrorMessage(hwnd, errorMsg);
        return FallbackToSystemBeep(hwnd);
    }

    // Try playing with miniaudio (has automatic fallback)
    if (PlayAudioWithMiniaudio(hwnd, NOTIFICATION_SOUND_FILE)) {
        return TRUE;
    }

    // Final fallback to system beep
    return FallbackToSystemBeep(hwnd);
}

/**
 * @brief Pause currently playing notification sound
 */
BOOL PauseNotificationSound(void) {
    if (g_isPlaying && !g_isPaused && g_engineInitialized && g_soundInitialized) {
        ma_sound_stop(&g_sound);
        g_isPaused = MA_TRUE;
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Resume paused notification sound
 */
BOOL ResumeNotificationSound(void) {
    if (g_isPlaying && g_isPaused && g_engineInitialized && g_soundInitialized) {
        ma_sound_start(&g_sound);
        g_isPaused = MA_FALSE;
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Stop notification sound playback immediately
 */
void StopNotificationSound(void) {
    CleanupAudioResources();
}
