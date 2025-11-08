/**
 * @file dialog_notification_audio.h
 * @brief Audio file management for notification settings dialog
 */

#ifndef DIALOG_NOTIFICATION_AUDIO_H
#define DIALOG_NOTIFICATION_AUDIO_H

#include <windows.h>

/* ============================================================================
 * Audio File Management
 * ============================================================================ */

/**
 * @brief Populate combo box with available audio files
 * @param hwndCombo Combo box handle
 * @param currentFile Current selected file path (UTF-8, can be NULL or empty)
 * 
 * @details
 * Scans audio folder for .mp3/.wav/.flac files and populates combo box.
 * First two items are always "None" and "System Beep".
 */
void PopulateNotificationSoundComboBox(HWND hwndCombo, const char* currentFile);

/**
 * @brief Handle sound test button click
 * @param hwndDlg Parent dialog handle
 * @param hwndCombo Sound combo box handle
 * @param hwndSlider Volume slider handle
 * @param isPlaying Pointer to playing state (modified by function)
 * @return TRUE if operation succeeded
 * 
 * @details
 * If not playing: starts audio playback with selected file
 * If playing: stops current playback
 * Button text is updated automatically via callback.
 */
BOOL HandleSoundTestButton(HWND hwndDlg, HWND hwndCombo, HWND hwndSlider, BOOL* isPlaying);

/**
 * @brief Open sound directory in Explorer and refresh combo box
 * @param hwndDlg Parent dialog handle
 * @param hwndCombo Sound combo box handle
 * 
 * @details
 * Opens %LOCALAPPDATA%\Catime\resources\audio in Windows Explorer
 * and refreshes the combo box to reflect any file changes.
 */
void HandleSoundDirButton(HWND hwndDlg, HWND hwndCombo);

/**
 * @brief Refresh sound list when combo box dropdown opens
 * @param hwndCombo Sound combo box handle
 * 
 * @details
 * Re-scans audio folder and updates combo box while preserving selection.
 */
void HandleSoundComboDropdown(HWND hwndCombo);

/**
 * @brief Setup audio playback complete callback for dialog
 * @param hwndDlg Dialog handle (receives WM_APP+100 on completion)
 */
void SetupAudioPlaybackCallback(HWND hwndDlg);

/**
 * @brief Cleanup audio resources before dialog closes
 * @param isPlaying Current playing state
 */
void CleanupAudioPlayback(BOOL isPlaying);

#endif /* DIALOG_NOTIFICATION_AUDIO_H */

