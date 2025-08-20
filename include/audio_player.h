/**
 * @file audio_player.h
 * @brief Audio notification system
 * 
 * Functions for playing, controlling, and managing notification sounds
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
 */
void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback);

/**
 * @brief Play notification sound
 * @param hwnd Window handle for audio context
 * @return TRUE on success, FALSE on failure
 */
BOOL PlayNotificationSound(HWND hwnd);

/**
 * @brief Pause currently playing notification sound
 * @return TRUE on success, FALSE on failure
 */
BOOL PauseNotificationSound(void);

/**
 * @brief Resume paused notification sound
 * @return TRUE on success, FALSE on failure
 */
BOOL ResumeNotificationSound(void);

/**
 * @brief Stop currently playing notification sound
 */
void StopNotificationSound(void);

/**
 * @brief Set audio playback volume
 * @param volume Volume level (0-100)
 */
void SetAudioVolume(int volume);

#endif