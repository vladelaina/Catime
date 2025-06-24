/**
 * @file audio_player.c
 * @brief Audio playback functionality handler
 * 
 * Implements notification audio playback functionality using the miniaudio library to support
 * background playback of various audio formats. Prioritizes playing configured audio files,
 * and falls back to system default notification sounds if the file doesn't exist or playback fails.
 */

#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include "../libs/miniaudio/miniaudio.h"

#include "config.h"  // Include configuration related header

// Declare global variables
extern char NOTIFICATION_SOUND_FILE[MAX_PATH];  // Referenced from config.c
extern int NOTIFICATION_SOUND_VOLUME;          // Volume configuration referenced from config.c

// Define callback function type for audio playback completion notification
typedef void (*AudioPlaybackCompleteCallback)(HWND hwnd);

// Declare global miniaudio variables
static ma_engine g_audioEngine;
static ma_sound g_sound;
static ma_bool32 g_engineInitialized = MA_FALSE;
static ma_bool32 g_soundInitialized = MA_FALSE;

// Callback related global variables
static AudioPlaybackCompleteCallback g_audioCompleteCallback = NULL;
static HWND g_audioCallbackHwnd = NULL;
static UINT_PTR g_audioTimerId = 0;

// Playback status tracking
static ma_bool32 g_isPlaying = MA_FALSE;
static ma_bool32 g_isPaused = MA_FALSE;  // New pause state variable

// Forward declarations
static void CheckAudioPlaybackComplete(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime);

/**
 * @brief Initialize audio engine
 * @return BOOL Returns TRUE if initialization is successful, otherwise FALSE
 */
static BOOL InitializeAudioEngine() {
    if (g_engineInitialized) {
        return TRUE; // Already initialized
    }
    
    // Initialize audio engine
    ma_result result = ma_engine_init(NULL, &g_audioEngine);
    if (result != MA_SUCCESS) {
        return FALSE;
    }
    
    g_engineInitialized = MA_TRUE;
    return TRUE;
}

/**
 * @brief Clean up audio engine resources
 */
static void UninitializeAudioEngine() {
    if (g_engineInitialized) {
        // Stop all sounds first
        if (g_soundInitialized) {
            ma_sound_uninit(&g_sound);
            g_soundInitialized = MA_FALSE;
        }
        
        // Then clean up the engine
        ma_engine_uninit(&g_audioEngine);
        g_engineInitialized = MA_FALSE;
    }
}

/**
 * @brief Check if a file exists
 * @param filePath File path
 * @return BOOL Returns TRUE if file exists, otherwise FALSE
 */
static BOOL FileExists(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    
    // Convert to wide characters to support Unicode paths
    wchar_t wFilePath[MAX_PATH * 2] = {0};
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH * 2);
    
    // Get file attributes
    DWORD dwAttrib = GetFileAttributesW(wFilePath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
            !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

/**
 * @brief Show error message dialog
 * @param hwnd Parent window handle
 * @param errorMsg Error message
 */
static void ShowErrorMessage(HWND hwnd, const wchar_t* errorMsg) {
    MessageBoxW(hwnd, errorMsg, L"音频播放错误", MB_ICONERROR | MB_OK);
}

/**
 * @brief Timer callback to check if audio playback is complete
 * @param hwnd Window handle
 * @param message Message
 * @param idEvent Timer ID
 * @param dwTime Time
 */
static void CALLBACK CheckAudioPlaybackComplete(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime) {
    // If audio engine and sound are initialized
    if (g_engineInitialized && g_soundInitialized) {
        // Check playback status - if paused, don't consider it as completed
        if (!ma_sound_is_playing(&g_sound) && !g_isPaused) {
            // Stop and clean up resources
            if (g_soundInitialized) {
                ma_sound_uninit(&g_sound);
                g_soundInitialized = MA_FALSE;
            }
            
            // Clean up timer
            KillTimer(hwnd, idEvent);
            g_audioTimerId = 0;
            g_isPlaying = MA_FALSE;
            g_isPaused = MA_FALSE;  // Make sure pause state is also reset
            
            // Call callback function
            if (g_audioCompleteCallback) {
                g_audioCompleteCallback(g_audioCallbackHwnd);
            }
        }
    } else {
        // If engine or sound is not initialized, consider playback as completed
        KillTimer(hwnd, idEvent);
        g_audioTimerId = 0;
        g_isPlaying = MA_FALSE;
        g_isPaused = MA_FALSE;  // Make sure pause state is also reset
        
        if (g_audioCompleteCallback) {
            g_audioCompleteCallback(g_audioCallbackHwnd);
        }
    }
}

/**
 * @brief System beep playback completion callback timer function
 * @param hwnd Window handle
 * @param message Message
 * @param idEvent Timer ID
 * @param dwTime Time
 */
static void CALLBACK SystemBeepDoneCallback(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime) {
    // Clean up timer
    KillTimer(hwnd, idEvent);
    g_audioTimerId = 0;
    g_isPlaying = MA_FALSE;
    g_isPaused = MA_FALSE;  // Make sure pause state is also reset
    
    // Execute callback
    if (g_audioCompleteCallback) {
        g_audioCompleteCallback(g_audioCallbackHwnd);
    }
}

/**
 * @brief Set audio playback volume
 * @param volume Volume percentage (0-100)
 */
void SetAudioVolume(int volume) {
    // Ensure volume is within valid range
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    // If engine is initialized, set volume
    if (g_engineInitialized) {
        // miniaudio volume range is 0.0-1.0
        float volFloat = (float)volume / 100.0f;
        ma_engine_set_volume(&g_audioEngine, volFloat);
        
        // If audio is currently playing, also update its volume
        if (g_soundInitialized && g_isPlaying) {
            ma_sound_set_volume(&g_sound, volFloat);
        }
    }
}

/**
 * @brief Play audio file using miniaudio
 * @param hwnd Parent window handle
 * @param filePath Audio file path
 * @return BOOL Returns TRUE on success, FALSE on failure
 */
static BOOL PlayAudioWithMiniaudio(HWND hwnd, const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    
    // If audio engine is not initialized, initialize it first
    if (!g_engineInitialized) {
        if (!InitializeAudioEngine()) {
            return FALSE;
        }
    }
    
    // Set volume
    float volume = (float)NOTIFICATION_SOUND_VOLUME / 100.0f;
    ma_engine_set_volume(&g_audioEngine, volume);
    
    // Load and play audio file
    if (g_soundInitialized) {
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
    }
    
    // Step 1: First convert UTF-8 path to Windows wide character format
    wchar_t wFilePath[MAX_PATH * 2] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH * 2) == 0) {
        DWORD error = GetLastError();
        wchar_t errorMsg[256];
        StringCbPrintfW(errorMsg, sizeof(errorMsg), L"路径转换错误 (UTF-8->Unicode): %lu", error);
        ShowErrorMessage(hwnd, errorMsg);
        return FALSE;
    }
    
    // Step 2: Get short path name (8.3 format) to avoid Chinese path issues
    wchar_t shortPath[MAX_PATH] = {0};
    DWORD shortPathLen = GetShortPathNameW(wFilePath, shortPath, MAX_PATH);
    if (shortPathLen == 0 || shortPathLen >= MAX_PATH) {
        DWORD error = GetLastError();
        
        // If getting short path fails, try using Windows native PlaySound API
        if (PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
            // Set a timer to simulate playback completion callback
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1002, 3000, (TIMERPROC)SystemBeepDoneCallback);
            g_isPlaying = MA_TRUE;
            return TRUE;
        }
        
        // If fallback method also fails, show error message
        wchar_t errorMsg[512];
        StringCbPrintfW(errorMsg, sizeof(errorMsg), L"获取短路径失败: %ls\n错误代码: %lu", wFilePath, error);
        ShowErrorMessage(hwnd, errorMsg);
        return FALSE;
    }
    
    // Step 3: Convert short path name to ASCII encoding (works for short paths)
    char asciiPath[MAX_PATH] = {0};
    if (WideCharToMultiByte(CP_ACP, 0, shortPath, -1, asciiPath, MAX_PATH, NULL, NULL) == 0) {
        DWORD error = GetLastError();
        wchar_t errorMsg[256];
        StringCbPrintfW(errorMsg, sizeof(errorMsg), L"路径转换错误 (Short Path->ASCII): %lu", error);
        ShowErrorMessage(hwnd, errorMsg);
        
        // Try using Windows native API for playback
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
    
    // Step 4: Initialize miniaudio using ASCII short path
    ma_result result = ma_sound_init_from_file(&g_audioEngine, asciiPath, 0, NULL, NULL, &g_sound);
    
    if (result != MA_SUCCESS) {
        // Try falling back to wide character path (likely to fail, but worth trying)
        char utf8Path[MAX_PATH * 4] = {0};
        WideCharToMultiByte(CP_UTF8, 0, wFilePath, -1, utf8Path, sizeof(utf8Path), NULL, NULL);
        
        result = ma_sound_init_from_file(&g_audioEngine, utf8Path, 0, NULL, NULL, &g_sound);
        
        if (result != MA_SUCCESS) {
            // If still failing, try using Windows native API
            if (PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
                // Set a timer to simulate playback completion callback
                if (g_audioTimerId != 0) {
                    KillTimer(hwnd, g_audioTimerId);
                }
                g_audioTimerId = SetTimer(hwnd, 1002, 3000, (TIMERPROC)SystemBeepDoneCallback);
                g_isPlaying = MA_TRUE;
                return TRUE;
            }
            
            // If all methods fail, show error message
            wchar_t errorMsg[512];
            StringCbPrintfW(errorMsg, sizeof(errorMsg), L"无法加载音频文件: %ls\n错误代码: %d", wFilePath, result);
            ShowErrorMessage(hwnd, errorMsg);
            return FALSE;
        }
    }
    
    g_soundInitialized = MA_TRUE;
    
    // Start playback
    if (ma_sound_start(&g_sound) != MA_SUCCESS) {
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
        
        // Try using Windows native API again
        if (PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1002, 3000, (TIMERPROC)SystemBeepDoneCallback);
            g_isPlaying = MA_TRUE;
            return TRUE;
        }
        
        ShowErrorMessage(hwnd, L"无法开始播放音频");
        return FALSE;
    }
    
    g_isPlaying = MA_TRUE;
    
    // Set timer to check playback status, every 500 milliseconds
    if (g_audioTimerId != 0) {
        KillTimer(hwnd, g_audioTimerId);
    }
    g_audioTimerId = SetTimer(hwnd, 1001, 500, (TIMERPROC)CheckAudioPlaybackComplete);
    
    return TRUE;
}

/**
 * @brief Validate if file path is legal
 * @param filePath File path to validate
 * @return BOOL Returns TRUE if path is valid, otherwise FALSE
 */
static BOOL IsValidFilePath(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    
    // Check if path contains equals sign or other illegal characters
    if (strchr(filePath, '=') != NULL) return FALSE;
    
    // Check path length
    if (strlen(filePath) >= MAX_PATH) return FALSE;
    
    return TRUE;
}

/**
 * @brief Clean up audio resources
 * 
 * Stops any currently playing audio and releases related resources,
 * ensuring no resource conflicts when playing new audio.
 */
void CleanupAudioResources(void) {
    // Stop any possibly playing WAV audio
    PlaySound(NULL, NULL, SND_PURGE);
    
    // Stop miniaudio playback
    if (g_engineInitialized && g_soundInitialized) {
        ma_sound_stop(&g_sound);
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
    }
    
    // Cancel timer
    if (g_audioTimerId != 0 && g_audioCallbackHwnd != NULL) {
        KillTimer(g_audioCallbackHwnd, g_audioTimerId);
        g_audioTimerId = 0;
    }
    
    g_isPlaying = MA_FALSE;
    g_isPaused = MA_FALSE;  // Reset pause state
}

/**
 * @brief Set audio playback completion callback function
 * @param hwnd Callback window handle
 * @param callback Callback function
 */
void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback) {
    g_audioCallbackHwnd = hwnd;
    g_audioCompleteCallback = callback;
}

/**
 * @brief Play notification audio
 * @param hwnd Parent window handle
 * @return BOOL Returns TRUE on success, FALSE on failure
 * 
 * If a valid NOTIFICATION_SOUND_FILE is configured and the file exists,
 * the system will prioritize using the configured audio file. Only when the file
 * doesn't exist or playback fails will it play the system default notification sound.
 * If no audio file is configured, no sound will be played.
 */
BOOL PlayNotificationSound(HWND hwnd) {
    // First clean up previous audio resources to ensure playback quality
    CleanupAudioResources();
    
    // Record callback window handle
    g_audioCallbackHwnd = hwnd;
    
    // Check if audio file is configured
    if (NOTIFICATION_SOUND_FILE[0] != '\0') {
        // Check if it's the system beep special marker
        if (strcmp(NOTIFICATION_SOUND_FILE, "SYSTEM_BEEP") == 0) {
            // Directly play system notification sound
            MessageBeep(MB_OK);
            g_isPlaying = MA_TRUE;
            
            // For system notification sound, set a shorter timer (500ms) to simulate completion callback
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1003, 500, (TIMERPROC)SystemBeepDoneCallback);
            
            return TRUE;
        }
        
        // Validate if file path is legal
        if (!IsValidFilePath(NOTIFICATION_SOUND_FILE)) {
            wchar_t errorMsg[MAX_PATH + 64];
            StringCbPrintfW(errorMsg, sizeof(errorMsg), L"音频文件路径无效:\n%hs", NOTIFICATION_SOUND_FILE);
            ShowErrorMessage(hwnd, errorMsg);
            
            // Play system default notification sound as fallback
            MessageBeep(MB_OK);
            g_isPlaying = MA_TRUE;
            
            // Also set a short timer
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1003, 500, (TIMERPROC)SystemBeepDoneCallback);
            
            return TRUE;
        }
        
        // Check if file exists
        if (FileExists(NOTIFICATION_SOUND_FILE)) {
            // Use miniaudio to play all types of audio files
            if (PlayAudioWithMiniaudio(hwnd, NOTIFICATION_SOUND_FILE)) {
                return TRUE;
            }
            
            // If playback fails, fall back to system notification sound
            MessageBeep(MB_OK);
            g_isPlaying = MA_TRUE;
            
            // Set short timer
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1003, 500, (TIMERPROC)SystemBeepDoneCallback);
            
            return TRUE;
        } else {
            // File doesn't exist
            wchar_t errorMsg[MAX_PATH + 64];
            StringCbPrintfW(errorMsg, sizeof(errorMsg), L"找不到配置的音频文件:\n%hs", NOTIFICATION_SOUND_FILE);
            ShowErrorMessage(hwnd, errorMsg);
            
            // Play system default notification sound as fallback
            MessageBeep(MB_OK);
            g_isPlaying = MA_TRUE;
            
            // Set short timer
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1003, 500, (TIMERPROC)SystemBeepDoneCallback);
            
            return TRUE;
        }
    }
    
    // If no audio file is configured, don't play any sound
    return TRUE;
}

/**
 * @brief Pause currently playing notification audio
 * @return BOOL Returns TRUE on success, FALSE on failure
 * 
 * Pauses the currently playing notification audio, used to synchronize audio
 * when the user pauses the timer. Only effective when audio is in playing state
 * and using miniaudio for playback.
 */
BOOL PauseNotificationSound(void) {
    // Check if audio is playing and not in paused state
    if (g_isPlaying && !g_isPaused && g_engineInitialized && g_soundInitialized) {
        // Pause playback - use stop instead of set_paused because the library might not support the latter
        ma_sound_stop(&g_sound);
        g_isPaused = MA_TRUE;
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Resume previously paused notification audio
 * @return BOOL Returns TRUE on success, FALSE on failure
 * 
 * Resumes playing previously paused notification audio, used to synchronize audio
 * when the user resumes the timer. Only effective when audio is in paused state.
 */
BOOL ResumeNotificationSound(void) {
    // Check if audio is in paused state
    if (g_isPlaying && g_isPaused && g_engineInitialized && g_soundInitialized) {
        // Resume playback - use start instead of set_paused
        ma_sound_start(&g_sound);
        g_isPaused = MA_FALSE;
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Stop playing notification audio
 * 
 * Stops any currently playing notification audio, including paused audio
 */
void StopNotificationSound(void) {
    // Directly clean up all audio resources, no need to handle pause state separately
    // Because CleanupAudioResources will reset all states
    CleanupAudioResources();
} 