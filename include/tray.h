/**
 * @file tray.h
 * @brief Tray icon management with smart tooltip generation
 * 
 * Modular tooltip builder includes CPU/memory/network metrics with auto-formatted units.
 * Animation type detection enables warm-up sampling for CPU/Memory percent modes.
 * Periodic tooltip updates (1Hz) keep system metrics fresh.
 */

#ifndef TRAY_H
#define TRAY_H

#include <windows.h>

#define CLOCK_WM_TRAYICON (WM_USER + 2)
#define CLOCK_ID_TRAY_APP_ICON 1001

extern UINT WM_TASKBARCREATED;

/**
 * @brief Register taskbar created message
 * 
 * @details For Explorer restart detection (crash recovery)
 */
void RegisterTaskbarCreatedMessage(void);

/**
 * @brief Initialize tray icon with smart type detection
 * @param hwnd Window handle
 * @param hInstance App instance
 * 
 * @details
 * - CPU/Memory percent: Warm-up sampling for accurate initial value
 * - Custom animations: First frame
 * - Logo: App icon resource
 * 
 * Starts 1Hz tooltip updates with system metrics.
 */
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance);

/**
 * @brief Remove tray icon and stop timers
 * 
 * @details Also shuts down system monitoring
 */
void RemoveTrayIcon(void);

/**
 * @brief Show balloon notification (3 seconds, no title/icons)
 * @param hwnd Window handle
 * @param message Message text (UTF-8)
 */
void ShowTrayNotification(HWND hwnd, const char* message);

/**
 * @brief Recreate icon after Explorer restart
 * @param hwnd Window handle
 * @param hInstance App instance
 * 
 * @details Clean remove + re-init for crash recovery
 */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance);

/**
 * @brief Update icon via full recreation
 * @param hwnd Window handle
 */
void UpdateTrayIcon(HWND hwnd);

#endif