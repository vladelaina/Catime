/**
 * @file notification.h
 * @brief Multi-modal notification with automatic fallback
 * 
 * Three modes with fallback chain (Toast → Modal → Tray):
 * 1. Toast - Custom animated overlay with transparency
 * 2. Modal - Modern acknowledgment dialog on a background UI thread
 * 3. Tray - System tray balloon (guaranteed delivery)
 * 
 * Dynamic width calculation prevents text clipping for long messages.
 */

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <windows.h>
#include "config.h"

/* ============================================================================
 * Animation States
 * ============================================================================ */

/**
 * @brief Toast notification fade animation states
 */
typedef enum {
    ANIM_FADE_IN,
    ANIM_VISIBLE,
    ANIM_FADE_OUT,
} AnimationState;

/* ============================================================================
 * External Configuration Variables
 * ============================================================================ */

/* NOTIFICATION_TIMEOUT_MS now in g_AppConfig.notification.display.timeout_ms */

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief Show notification via configured method
 * @param hwnd Parent handle
 * @param message Message text (wide-char)
 * 
 * @details
 * Selects type from config (Toast/Modal/Tray), respects NOTIFICATION_DISABLED.
 * Falls back to tray on failure.
 * 
 * @note Primary entry point for notifications
 */
void ShowNotification(HWND hwnd, const wchar_t* message);

/**
 * @brief Show toast notification with animation
 * @param hwnd Parent handle
 * @param message Message text
 * 
 * @details
 * Dynamic width (based on message length), smooth fade animations,
 * click-to-dismiss, bottom-right positioning. Falls back to tray on failure.
 * 
 * @note Uses layered windows for transparency. ~3-5s execution time.
 */
void ShowToastNotification(HWND hwnd, const wchar_t* message);

/**
 * @brief Show toast notification with preview/normal mode control
 * @param hwnd Parent handle
 * @param message Message text
 * @param isPreview TRUE for interactive preview, FALSE for normal notification
 * 
 * @details
 * - Normal mode (isPreview=FALSE): Auto-dismiss, click-to-close, non-draggable
 * - Preview mode (isPreview=TRUE): Draggable, resizable, saves position, scroll to adjust opacity
 */
void ShowToastNotificationEx(HWND hwnd, const wchar_t* message, BOOL isPreview);

/**
 * @brief Show a toast notification with an explicit timeout
 * @param hwnd Parent handle
 * @param message Message text
 * @param timeoutMs Auto-dismiss timeout in milliseconds
 *
 * @details
 * Used for one-shot plugin notifications without temporarily mutating global
 * notification config.
 */
void ShowToastNotificationWithTimeout(HWND hwnd, const wchar_t* message, int timeoutMs);

/**
 * @brief Show a persistent toast preview with explicit opacity
 * @param hwnd Parent handle
 * @param message Message text
 * @param opacityPercent Initial opacity percentage (10-100)
 *
 * @details
 * Used by settings UI to preview appearance without temporarily mutating
 * global notification config.
 */
void ShowToastNotificationPreview(HWND hwnd, const wchar_t* message, int opacityPercent);

/**
 * @brief Update a toast window opacity and its internal animation target
 * @param hwnd Toast window handle
 * @param opacityPercent Opacity percentage (10-100)
 */
void SetToastNotificationOpacity(HWND hwnd, int opacityPercent);

/**
 * @brief Update a toast window corner radius
 * @param hwnd Toast window handle
 * @param cornerRadius Corner radius in pixels
 */
void SetToastNotificationCornerRadius(HWND hwnd, int cornerRadius);

/**
 * @brief Update a toast window notification text height ratio
 * @param hwnd Toast window handle
 * @param fontPercent Text height as a percentage of the window height
 */
void SetToastNotificationFontPercent(HWND hwnd, int fontPercent);

/**
 * @brief Update a toast window message text
 * @param hwnd Toast window handle
 * @param message Replacement message text
 * @return TRUE if the message was updated, FALSE if the window/data was invalid or allocation failed
 */
BOOL SetToastNotificationMessage(HWND hwnd, const wchar_t* message);

/**
 * @brief Repaint toast notifications after the active text color changes
 */
void RefreshToastNotificationColors(void);

/**
 * @brief Check whether a toast window is an interactive preview window
 * @param hwnd Toast window handle
 * @return TRUE only for current Catime preview toasts
 */
BOOL IsToastNotificationPreviewWindow(HWND hwnd);

/**
 * @brief Show modal in background thread (non-blocking)
 * @param hwnd Parent handle
 * @param message Message text
 * 
 * @details
 * Background thread avoids blocking main UI. Thread detached and self-cleans.
 * Falls back to tray on failure.
 */
void ShowModalNotification(HWND hwnd, const wchar_t* message);

/**
 * @brief Force close all toasts (graceful fade-out)
 * 
 * @details
 * Enumerates and triggers fade-out. Used on shutdown, explicit close,
 * config changes.
 * 
 * @note Only affects toasts (not modal/tray). Already fading ignored.
 */
void CloseAllNotifications(void);

/**
 * @brief Release cached notification drawing resources.
 */
void CleanupNotificationResources(void);

#endif /* NOTIFICATION_H */
