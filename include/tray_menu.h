/**
 * @file tray_menu.h
 * @brief System tray menu functionality interface
 * 
 * This file defines constants and function interfaces related to the system tray menu,
 * including menu item ID definitions and menu display function declarations.
 */

#ifndef CLOCK_TRAY_MENU_H
#define CLOCK_TRAY_MENU_H

#include <windows.h>

/// @name Time display related menu items
/// @{
#define CLOCK_IDM_SHOW_CURRENT_TIME 150  ///< Show current time option
#define CLOCK_IDM_24HOUR_FORMAT 151      ///< 24-hour format display
#define CLOCK_IDM_SHOW_SECONDS 152       ///< Show seconds option
/// @}

/// @name Timer function menu items
/// @{
#define CLOCK_IDM_TIMER_MANAGEMENT 159   ///< Timer management main menu
#define CLOCK_IDM_TIMER_PAUSE_RESUME 158 ///< Pause/Resume timer
#define CLOCK_IDM_TIMER_RESTART 177      ///< Restart timer
#define CLOCK_IDM_COUNT_UP_START 171     ///< Start/Pause count-up timer
#define CLOCK_IDM_COUNT_UP_RESET 172     ///< Reset count-up timer
#define CLOCK_IDM_COUNTDOWN_START_PAUSE 154  ///< Start/Pause countdown
#define CLOCK_IDM_COUNTDOWN_RESET 155    ///< Reset countdown
/// @}

/// @name Edit and settings menu items
/// @{
#define CLOCK_IDC_EDIT_MODE 113          ///< Edit mode option
/// @}

/// @name Timeout action menu items
/// @{
#define CLOCK_IDM_SHOW_MESSAGE 121       ///< Show message action
#define CLOCK_IDM_LOCK_SCREEN 122        ///< Lock screen action
#define CLOCK_IDM_SHUTDOWN 123           ///< Shutdown action
#define CLOCK_IDM_RESTART 124            ///< Restart action
#define CLOCK_IDM_SLEEP 125              ///< Sleep action
#define CLOCK_IDM_BROWSE_FILE 131        ///< Browse file option
#define CLOCK_IDM_RECENT_FILE_1 126      ///< Recent file starting ID
#define CLOCK_IDM_TIMEOUT_SHOW_TIME 135  ///< Show current time action
#define CLOCK_IDM_TIMEOUT_COUNT_UP 136   ///< Count-up timer action
/// @}

/// @name Time options management menu items
/// @{
#define CLOCK_IDC_MODIFY_TIME_OPTIONS 156  ///< Modify time options
/// @}

/// @name Startup settings menu items
/// @{
#define CLOCK_IDC_SET_COUNTDOWN_TIME 173   ///< Set to countdown on startup
#define CLOCK_IDC_START_COUNT_UP 175       ///< Set to count-up on startup
#define CLOCK_IDC_START_SHOW_TIME 176      ///< Show current time on startup
#define CLOCK_IDC_START_NO_DISPLAY 174     ///< No display on startup
#define CLOCK_IDC_AUTO_START 160           ///< Auto-start on system boot
/// @}

/// @name Notification settings menu items
/// @{
#define CLOCK_IDM_NOTIFICATION_SETTINGS 193  ///< Notification settings main menu
#define CLOCK_IDM_NOTIFICATION_CONTENT 191   ///< Notification content settings
#define CLOCK_IDM_NOTIFICATION_DISPLAY 192   ///< Notification display settings
/// @}

/// @name Pomodoro menu items
/// @{
#define CLOCK_IDM_POMODORO_START 181   ///< Start Pomodoro
#define CLOCK_IDM_POMODORO_WORK 182    ///< Set work time
#define CLOCK_IDM_POMODORO_BREAK 183   ///< Set short break time
#define CLOCK_IDM_POMODORO_LBREAK 184  ///< Set long break time
#define CLOCK_IDM_POMODORO_LOOP_COUNT 185 ///< Set loop count
#define CLOCK_IDM_POMODORO_RESET 186  ///< Reset Pomodoro
/// @}

/// @name Color selection menu items
/// @{
#define CLOCK_IDC_COLOR_VALUE 1301       ///< Color value edit item
#define CLOCK_IDC_COLOR_PANEL 1302       ///< Color panel item
/// @}

/// @name Language selection menu items
/// @{
#define CLOCK_IDM_LANG_CHINESE 161       ///< Simplified Chinese option
#define CLOCK_IDM_LANG_CHINESE_TRAD 163  ///< Traditional Chinese option
#define CLOCK_IDM_LANG_ENGLISH 162       ///< English option
#define CLOCK_IDM_LANG_SPANISH 164       ///< Spanish option
#define CLOCK_IDM_LANG_FRENCH 165        ///< French option
#define CLOCK_IDM_LANG_GERMAN 166        ///< German option
#define CLOCK_IDM_LANG_RUSSIAN 167       ///< Russian option
#define CLOCK_IDM_LANG_KOREAN 170        ///< Korean option
/// @}

/**
 * @brief Display tray right-click menu
 * @param hwnd Window handle
 * 
 * Create and display the system tray right-click menu, including time settings, display mode switching, and quick time options.
 * Dynamically adjust menu items based on the current application state.
 */
void ShowContextMenu(HWND hwnd);

/**
 * @brief Display color and settings menu
 * @param hwnd Window handle
 * 
 * Create and display the application's main settings menu, including edit mode, timeout actions,
 * preset management, font selection, color settings, and about information options.
 */
void ShowColorMenu(HWND hwnd);

#endif // CLOCK_TRAY_MENU_H