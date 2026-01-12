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
#include "../../resource/resource.h"

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

/**
 * @brief Tray tooltip timer callback
 * @param hwnd Window handle
 * @param msg Timer message
 * @param id Timer ID
 * @param time System time
 *
 * @details Updates tooltip with system metrics (CPU, memory, network)
 */
void CALLBACK TrayTipTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);

/**
 * @brief Update tray icon tooltip
 * @param tip Tooltip text to display
 */
void UpdateTrayTooltip(const wchar_t* tip);

/**
 * @brief Install mouse hook for tray wheel events
 * @note Called when mouse enters tray icon area (NIN_POPUPOPEN)
 */
void InstallTrayMouseHook(void);

/**
 * @brief Uninstall mouse hook for tray wheel events
 * @note Called when mouse leaves tray icon area (NIN_POPUPCLOSE)
 */
void UninstallTrayMouseHook(void);

/**
 * @brief Check if mouse hook is currently installed
 * @return TRUE if hook is installed
 */
BOOL IsTrayMouseHookInstalled(void);

/**
 * @brief Check if mouse is over tray icon area
 * @param pt Mouse position in screen coordinates
 * @return TRUE if mouse is over tray icon
 */
BOOL IsMouseOverTrayIconArea(POINT pt);

#endif