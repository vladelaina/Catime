/**
 * @file window_config_handlers.h
 * @brief Configuration reload handlers for WM_APP_*_CHANGED messages
 */

#ifndef WINDOW_CONFIG_HANDLERS_H
#define WINDOW_CONFIG_HANDLERS_H

#include <windows.h>

/* ============================================================================
 * Configuration Reload Handlers
 * ============================================================================ */

/**
 * @brief Handle WM_APP_DISPLAY_CHANGED
 */
LRESULT HandleAppDisplayChanged(HWND hwnd);

/**
 * @brief Handle WM_APP_TIMER_CHANGED
 */
LRESULT HandleAppTimerChanged(HWND hwnd);

/**
 * @brief Handle WM_APP_POMODORO_CHANGED
 */
LRESULT HandleAppPomodoroChanged(HWND hwnd);

/**
 * @brief Handle WM_APP_NOTIFICATION_CHANGED
 */
LRESULT HandleAppNotificationChanged(HWND hwnd);

/**
 * @brief Handle WM_APP_HOTKEYS_CHANGED
 */
LRESULT HandleAppHotkeysChanged(HWND hwnd);

/**
 * @brief Handle WM_APP_RECENTFILES_CHANGED
 */
LRESULT HandleAppRecentFilesChanged(HWND hwnd);

/**
 * @brief Handle WM_APP_COLORS_CHANGED
 */
LRESULT HandleAppColorsChanged(HWND hwnd);

/**
 * @brief Handle WM_APP_ANIM_SPEED_CHANGED
 */
LRESULT HandleAppAnimSpeedChanged(HWND hwnd);

/**
 * @brief Handle WM_APP_ANIM_PATH_CHANGED
 */
LRESULT HandleAppAnimPathChanged(HWND hwnd);

#endif /* WINDOW_CONFIG_HANDLERS_H */

