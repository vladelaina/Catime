/**
 * @file audio_player.h
 * @brief Audio playback functionality interface
 * 
 * Provides interfaces for notification audio playback functionality, supporting background playback of various audio formats.
 */

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <windows.h>

/**
 * @brief Audio playback completion callback function type
 * @param hwnd Window handle
 * 
 * When audio playback completes, the system will call this callback function to notify the application.
 */
typedef void (*AudioPlaybackCompleteCallback)(HWND hwnd);

/**
 * @brief Set audio playback completion callback function
 * @param hwnd Callback window handle
 * @param callback Callback function
 * 
 * Sets the callback function for audio playback completion. When audio playback ends,
 * the system will call this callback function to notify the application.
 */
void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback);

/**
 * @brief Play notification audio
 * @param hwnd Parent window handle
 * @return BOOL Returns TRUE on success, FALSE on failure
 * 
 * When a valid NOTIFICATION_SOUND_FILE is configured and the file exists, the system will play that audio file.
 * If the file doesn't exist or playback fails, the system default notification sound will be played.
 * An error dialog will be displayed if playback fails.
 */
BOOL PlayNotificationSound(HWND hwnd);

/**
 * @brief Pause currently playing notification audio
 * @return BOOL Returns TRUE on success, FALSE on failure
 * 
 * Pauses the currently playing notification audio, used to synchronize audio pause when the user pauses the timer.
 * Only effective when audio is in playing state and being played with miniaudio.
 */
BOOL PauseNotificationSound(void);

/**
 * @brief Resume previously paused notification audio
 * @return BOOL Returns TRUE on success, FALSE on failure
 * 
 * Resumes playing previously paused notification audio, used to synchronize audio playback when the user resumes the timer.
 * Only effective when audio is in paused state.
 */
BOOL ResumeNotificationSound(void);

/**
 * @brief Stop playing notification audio
 * 
 * Stops any currently playing notification audio
 */
void StopNotificationSound(void);

/**
 * @brief Set audio playback volume
 * @param volume Volume percentage (0-100)
 * 
 * Sets the volume level for audio playback, affecting all notification audio playback
 */
void SetAudioVolume(int volume);

#endif // AUDIO_PLAYER_H 