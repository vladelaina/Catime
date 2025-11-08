/**
 * @file dialog_notification.h
 * @brief Notification configuration dialogs
 */

#ifndef DIALOG_NOTIFICATION_H
#define DIALOG_NOTIFICATION_H

#include <windows.h>

/* ============================================================================
 * Notification Messages Dialog
 * ============================================================================ */

/**
 * @brief Show notification messages configuration dialog
 * @param hwndParent Parent window
 * 
 * @details
 * Configures: Work/short break/long break notification messages
 * Supports: Emoji, multiline text, per-phase customization
 * Persists to: [NOTIFICATIONS]work_msg, short_break_msg, long_break_msg
 */
void ShowNotificationMessagesDialog(HWND hwndParent);

/**
 * @brief Notification messages dialog procedure
 */
INT_PTR CALLBACK NotificationMessagesDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/* ============================================================================
 * Notification Display Dialog
 * ============================================================================ */

/**
 * @brief Show notification display settings dialog
 * @param hwndParent Parent window
 * 
 * @details
 * Configures: Display duration, opacity, fade effects
 * Controls: Slider for timeout (1-30s), opacity (0-100%)
 * Persists to: [NOTIFICATIONS]timeout, opacity
 */
void ShowNotificationDisplayDialog(HWND hwndParent);

/**
 * @brief Notification display dialog procedure
 */
INT_PTR CALLBACK NotificationDisplayDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/* ============================================================================
 * Full Notification Settings Dialog
 * ============================================================================ */

/**
 * @brief Show comprehensive notification settings dialog
 * @param hwndParent Parent window
 * 
 * @details
 * All-in-one dialog combining:
 * - Message customization (multiline edit)
 * - Display settings (timeout, opacity)
 * - Notification type (toast/modal/disabled)
 * - Sound configuration (file selection, volume, test playback)
 */
void ShowNotificationSettingsDialog(HWND hwndParent);

/**
 * @brief Update opacity slider and text in settings dialog
 * @param opacity New opacity value (1-100)
 */
void UpdateNotificationOpacityControls(int opacity);

/**
 * @brief Full notification settings dialog procedure
 */
INT_PTR CALLBACK NotificationSettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/* ============================================================================
 * Configuration Read/Write Functions
 * ============================================================================ */

/**
 * @note All notification configuration functions are now declared in config.h
 *       and implemented in config_notification.c:
 *       
 *       Read functions:
 *       - ReadNotificationMessagesConfig()
 *       - ReadNotificationTimeoutConfig()
 *       - ReadNotificationOpacityConfig()
 *       - ReadNotificationTypeConfig()
 *       - ReadNotificationSoundConfig()
 *       - ReadNotificationVolumeConfig()
 *       
 *       Write functions:
 *       - WriteConfigNotificationMessages(timeout_msg)
 *       - WriteConfigNotificationTimeout(timeout_ms)
 *       - WriteConfigNotificationOpacity(opacity)
 *       - WriteConfigNotificationType(NotificationType type)
 *       - WriteConfigNotificationSound(sound_file)
 *       - WriteConfigNotificationVolume(volume)
 */

#endif /* DIALOG_NOTIFICATION_H */

