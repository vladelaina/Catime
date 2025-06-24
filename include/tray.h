/**
 * @file tray.h
 * @brief System tray functionality interface
 * 
 * This file defines the application's system tray operation interface, including initialization, removal, and notification display functions.
 */

#ifndef TRAY_H
#define TRAY_H

#include <windows.h>

/// @name System tray message constants
/// @{
#define CLOCK_WM_TRAYICON (WM_USER + 2)  ///< Custom tray icon message ID
/// @}

/// @name System tray identifier constants
/// @{
#define CLOCK_ID_TRAY_APP_ICON 1001      ///< Tray icon identifier ID
/// @}

/// TaskbarCreated message ID
extern UINT WM_TASKBARCREATED;

/**
 * @brief Register TaskbarCreated message
 * 
 * Register the TaskbarCreated message sent by the system, used to recreate the tray icon after Explorer restarts
 */
void RegisterTaskbarCreatedMessage(void);

/**
 * @brief Initialize system tray icon
 * @param hwnd Window handle associated with the tray icon
 * @param hInstance Application instance handle
 * 
 * Create and display a system tray icon with default settings.
 * This icon will receive messages through CLOCK_WM_TRAYICON callback.
 */
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance);

/**
 * @brief Remove system tray icon
 * 
 * Remove the application's icon from the system tray.
 * Should be called when the application is closing.
 */
void RemoveTrayIcon(void);

/**
 * @brief Display notification in system tray
 * @param hwnd Window handle associated with the notification
 * @param message Text message to display in the notification
 * 
 * Display a balloon tip notification from the system tray icon.
 * The notification uses NIIF_NONE style (no icon) and times out after 3 seconds.
 */
void ShowTrayNotification(HWND hwnd, const char* message);

/**
 * @brief Recreate tray icon
 * @param hwnd Window handle
 * @param hInstance Instance handle
 * 
 * Recreate the tray icon after Windows Explorer restarts.
 * This function should be called when receiving the TaskbarCreated message.
 */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance);

/**
 * @brief Update tray icon and menu
 * @param hwnd Window handle
 * 
 * Update the tray icon and menu after application language or settings change.
 * Used to ensure the text displayed in the tray menu matches the current language settings.
 */
void UpdateTrayIcon(HWND hwnd);

#endif // TRAY_H