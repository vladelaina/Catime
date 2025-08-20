/**
 * @file tray.h
 * @brief System tray icon management
 * 
 * Functions for creating, updating, and managing the system tray icon
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
 */
void RegisterTaskbarCreatedMessage(void);

/**
 * @brief Initialize system tray icon
 * @param hwnd Main window handle
 * @param hInstance Application instance handle
 */
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance);

/**
 * @brief Remove tray icon from system tray
 */
void RemoveTrayIcon(void);

/**
 * @brief Show balloon notification in system tray
 * @param hwnd Window handle for context
 * @param message Notification message text
 */
void ShowTrayNotification(HWND hwnd, const char* message);

/**
 * @brief Recreate tray icon after taskbar restart
 * @param hwnd Main window handle
 * @param hInstance Application instance handle
 */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance);

/**
 * @brief Update tray icon appearance or tooltip
 * @param hwnd Main window handle
 */
void UpdateTrayIcon(HWND hwnd);

#endif