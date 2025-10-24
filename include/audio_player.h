/**
 * @file audio_player.h
 * @brief Cross-platform audio notification system with fallback mechanisms
 * @version 2.0 - Refactored for better maintainability
 * 
 * Provides audio playback for notifications with multiple fallback strategies:
 * 1. Primary: miniaudio library (high quality, all formats)
 * 2. Fallback: Windows PlaySound API
 * 3. Final fallback: System beep
 * 
 * Features:
 * - Automatic format detection and path conversion
 * - Volume control with real-time adjustment
 * - Playback completion callbacks
 * - Thread-safe operation
 */

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <windows.h>

/**
 * @brief Callback function type for audio playback completion
 * @param hwnd Window handle to notify when playback completes
 */
typedef void (*AudioPlaybackCompleteCallback)(HWND hwnd);

/**
 * @brief Set callback for audio playback completion
 * @param hwnd Window handle to receive completion notification
 * @param callback Function to call when audio playback completes
 * 
 * @note Callback is invoked when audio finishes naturally or on error
 */
void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback);

/**
 * @brief Play notification sound with automatic fallback
 * @param hwnd Window handle for audio context
 * @return TRUE on success, FALSE on failure
 * 
 * @details Playback strategy:
 * 1. Tries miniaudio for configured sound file
 * 2. Falls back to PlaySound if miniaudio fails
 * 3. Falls back to system beep if all else fails
 * 4. Returns TRUE even for system beep (always succeeds)
 * 
 * Special behaviors:
 * - Empty NOTIFICATION_SOUND_FILE: returns TRUE silently
 * - "SYSTEM_BEEP": plays system beep directly
 * - Invalid paths: shows error dialog and falls back to beep
 * 
 * @note Supports .mp3, .wav, .flac, and other formats via miniaudio
 * @note Respects NOTIFICATION_SOUND_VOLUME global setting
 */
BOOL PlayNotificationSound(HWND hwnd);

/**
 * @brief Pause currently playing notification sound
 * @return TRUE if sound was paused, FALSE if not playing or already paused
 * 
 * @note Only works with miniaudio playback (not PlaySound or system beep)
 */
BOOL PauseNotificationSound(void);

/**
 * @brief Resume paused notification sound
 * @return TRUE if sound was resumed, FALSE if not paused
 * 
 * @note Only works with miniaudio playback (not PlaySound or system beep)
 */
BOOL ResumeNotificationSound(void);

/**
 * @brief Stop currently playing notification sound immediately
 * 
 * Stops all audio playback and releases resources:
 * - miniaudio: stops and uninitializes sound
 * - PlaySound: purges all instances
 * - Timers: kills completion timers
 * 
 * @note Safe to call even if nothing is playing
 */
void StopNotificationSound(void);

/**
 * @brief Set audio playback volume
 * @param volume Volume level (0-100), clamped automatically
 * 
 * @details
 * - Applies immediately to currently playing audio
 * - Persists for future playback
 * - 0 = mute, 100 = maximum volume
 * 
 * @note Volume setting is global and affects all subsequent playback
 */
void SetAudioVolume(int volume);

#endif