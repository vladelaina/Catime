/**
 * @file audio_player.h
 * @brief Audio notification system with three-tier fallback for reliability
 * 
 * 1. miniaudio (MP3/FLAC/WAV support)
 * 2. PlaySound (WAV fallback)
 * 3. System beep (guaranteed notification)
 * 
 * Ensures users always receive audio feedback even when preferred methods fail.
 */

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <windows.h>

/**
 * @brief Callback function type for audio playback completion events
 * @param hwnd Window handle of notification source
 */
typedef void (*AudioPlaybackCompleteCallback)(HWND hwnd);

/**
 * @brief Registers callback for playback completion
 * @param hwnd Window handle to receive completion notification
 * @param callback Function to invoke on completion
 * 
 * @details
 * Enables UI state transitions without polling or fixed delays.
 * Fires on both success and error to ensure consistent UI behavior.
 */
void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback);

/**
 * @brief Plays notification sound with automatic fallback
 * @param hwnd Window handle for error dialogs
 * @return TRUE on success (including beep fallback), FALSE on catastrophic failure
 * 
 * @details
 * Special cases:
 * - Empty NOTIFICATION_SOUND_FILE: Silent success (user disabled audio)
 * - "SYSTEM_BEEP": Direct beep (bypasses file playback)
 * - Invalid path: Shows error once, then falls back to beep
 * 
 * @note Always returns TRUE for beep fallback (notification delivered)
 */
BOOL PlayNotificationSound(HWND hwnd);

/**
 * @brief Pauses audio playback without losing position
 * @return TRUE if paused, FALSE if not playing or backend unsupported
 * 
 * @note Only miniaudio supports pause; PlaySound and beep cannot pause
 */
BOOL PauseNotificationSound(void);

/**
 * @brief Resumes paused audio from last position
 * @return TRUE if resumed, FALSE if not paused or backend unsupported
 * 
 * @note Only miniaudio supports resume; PlaySound and beep cannot resume
 */
BOOL ResumeNotificationSound(void);

/**
 * @brief Immediately stops all audio and releases resources
 * 
 * @details
 * Stops all backends (miniaudio, PlaySound, timers) to prevent leaks and artifacts.
 * Must be called before application exit.
 * 
 * @note Safe to call when nothing is playing
 */
void StopNotificationSound(void);

/**
 * @brief Sets audio volume with immediate effect (clamped to 0-100)
 * @param volume Volume level (0=mute, 100=max)
 * 
 * @details
 * Applies to active and future playback. Clamping prevents distortion.
 */
void SetAudioVolume(int volume);

#endif