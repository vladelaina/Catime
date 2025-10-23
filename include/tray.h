/**
 * @file tray.h
 * @brief Refactored system tray icon management
 * @version 2.0 - Enhanced with modular helper functions
 * 
 * Provides functions for creating, updating, and managing the system tray icon
 * with intelligent tooltip generation and automatic icon refresh.
 * 
 * Internal implementation features:
 * - Modular tooltip building with CPU, memory, and network metrics
 * - Smart animation type detection (logo, CPU%, memory%, custom)
 * - Automatic byte formatting with appropriate units (B/s, KB/s, MB/s, GB/s)
 * - Simplified static image detection for various formats
 * - Centralized external declarations for better organization
 */

#ifndef TRAY_H
#define TRAY_H

#include <windows.h>

/** @brief Custom tray icon message identifier */
#define CLOCK_WM_TRAYICON (WM_USER + 2)

/** @brief Tray icon resource identifier */
#define CLOCK_ID_TRAY_APP_ICON 1001

/** @brief Taskbar recreation message identifier */
extern UINT WM_TASKBARCREATED;

/**
 * @brief Register taskbar created message for Explorer restart detection
 * 
 * @details Registers a Windows message that is broadcast when the taskbar
 * is recreated (typically after Explorer.exe crashes or restarts).
 */
void RegisterTaskbarCreatedMessage(void);

/**
 * @brief Initialize system tray icon with smart type detection
 * @param hwnd Main window handle for callbacks
 * @param hInstance Application instance handle for resources
 * 
 * @details Initializes the tray icon with appropriate initial state:
 * - For CPU/memory percent icons: warm-up sampling for accurate initial value
 * - For custom animations: first frame of animation
 * - For logo: application icon resource
 * 
 * Starts periodic tooltip updates (1Hz) with system metrics.
 */
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance);

/**
 * @brief Remove tray icon from system notification area
 * 
 * @details Cleanly removes the icon and stops all timers when application
 * exits or hides. Also shuts down system monitoring.
 */
void RemoveTrayIcon(void);

/**
 * @brief Display balloon notification from system tray
 * @param hwnd Window handle for notification context
 * @param message Notification message text (UTF-8 encoded)
 * 
 * @details Shows a 3-second notification balloon without title or icons.
 * Message is automatically converted from UTF-8 to wide characters.
 */
void ShowTrayNotification(HWND hwnd, const char* message);

/**
 * @brief Recreate tray icon after taskbar restart
 * @param hwnd Main window handle
 * @param hInstance Application instance handle
 * 
 * @details Performs clean removal and re-initialization to restore the
 * tray icon when Windows Explorer restarts or recovers from crashes.
 */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance);

/**
 * @brief Update tray icon by full recreation
 * @param hwnd Main window handle
 * 
 * @details Convenience function that extracts the instance handle from
 * the window and performs a full icon recreation cycle.
 */
void UpdateTrayIcon(HWND hwnd);

#endif