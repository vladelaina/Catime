/**
 * @file tray_menu.h
 * @brief Tray context menu system with dynamic content
 * 
 * Modular helpers eliminate duplication (config reading, path conversion).
 * Dynamic menu items loaded from config (Pomodoro presets, recent files, fonts, animations).
 * Menu IDs organized by functional groups for maintainability.
 */

#ifndef CLOCK_TRAY_MENU_H
#define CLOCK_TRAY_MENU_H

#include <windows.h>

/* Time display */
#define CLOCK_IDM_SHOW_CURRENT_TIME 150
#define CLOCK_IDM_24HOUR_FORMAT 151
#define CLOCK_IDM_SHOW_SECONDS 152

/* Timer management */
#define CLOCK_IDM_TIMER_MANAGEMENT 159
#define CLOCK_IDM_TIMER_PAUSE_RESUME 158
#define CLOCK_IDM_TIMER_RESTART 177
#define CLOCK_IDM_COUNT_UP_START 171
#define CLOCK_IDM_COUNT_UP_RESET 172
#define CLOCK_IDM_COUNTDOWN_START_PAUSE 154
#define CLOCK_IDM_COUNTDOWN_RESET 155

/* Window interaction */
#define CLOCK_IDC_EDIT_MODE 113
/* CLOCK_IDM_TOPMOST in resource.h */

/* Timeout actions */
#define CLOCK_IDM_SHOW_MESSAGE 121
#define CLOCK_IDM_LOCK_SCREEN 122
#define CLOCK_IDM_SHUTDOWN 123
#define CLOCK_IDM_RESTART 124
#define CLOCK_IDM_SLEEP 125
#define CLOCK_IDM_BROWSE_FILE 131
#define CLOCK_IDM_RECENT_FILE_1 126
#define CLOCK_IDM_TIMEOUT_SHOW_TIME 135
#define CLOCK_IDM_TIMEOUT_COUNT_UP 136

/* Configuration */
#define CLOCK_IDC_MODIFY_TIME_OPTIONS 156
#define CLOCK_IDM_TIME_FORMAT_DEFAULT 194
#define CLOCK_IDM_TIME_FORMAT_ZERO_PADDED 196
#define CLOCK_IDM_TIME_FORMAT_FULL_PADDED 197
#define CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS 198

/* Startup */
#define CLOCK_IDC_SET_COUNTDOWN_TIME 173
#define CLOCK_IDC_START_COUNT_UP 175
#define CLOCK_IDC_START_SHOW_TIME 176
#define CLOCK_IDC_START_NO_DISPLAY 174
#define CLOCK_IDC_AUTO_START 160

/* Notifications */
#define CLOCK_IDM_NOTIFICATION_SETTINGS 193
#define CLOCK_IDM_NOTIFICATION_CONTENT 191
#define CLOCK_IDM_NOTIFICATION_DISPLAY 192

/* Pomodoro */
#define CLOCK_IDM_POMODORO_START 181
#define CLOCK_IDM_POMODORO_WORK 182
#define CLOCK_IDM_POMODORO_BREAK 183
#define CLOCK_IDM_POMODORO_LBREAK 184
#define CLOCK_IDM_POMODORO_LOOP_COUNT 185
#define CLOCK_IDM_POMODORO_RESET 186

/* Color */
#define CLOCK_IDC_COLOR_VALUE 1301
#define CLOCK_IDC_COLOR_PANEL 1302

/* Language */
#define CLOCK_IDM_LANG_CHINESE 161
#define CLOCK_IDM_LANG_CHINESE_TRAD 163
#define CLOCK_IDM_LANG_ENGLISH 162
#define CLOCK_IDM_LANG_SPANISH 164
#define CLOCK_IDM_LANG_FRENCH 165
#define CLOCK_IDM_LANG_GERMAN 166
#define CLOCK_IDM_LANG_RUSSIAN 167
#define CLOCK_IDM_LANG_KOREAN 170

/* Animations */
#define CLOCK_IDM_ANIMATIONS_MENU 2200
#define CLOCK_IDM_ANIMATIONS_OPEN_DIR 2201
#define CLOCK_IDM_ANIMATIONS_USE_LOGO 2202
#define CLOCK_IDM_ANIMATIONS_USE_CPU 2203
#define CLOCK_IDM_ANIMATIONS_USE_MEM 2204
#define CLOCK_IDM_ANIMATIONS_BASE 3000  /**< Dynamic items start here */

/* Animation speed metric */
#define CLOCK_IDM_ANIM_SPEED_MEMORY 2210
#define CLOCK_IDM_ANIM_SPEED_CPU 2211
#define CLOCK_IDM_ANIM_SPEED_TIMER 2212

/**
 * @brief Show context menu (left-click)
 * @param hwnd Window handle
 * 
 * @details
 * Timer controls, time display, Pomodoro presets (loaded from config).
 * Dynamic content reloaded each display.
 */
void ShowContextMenu(HWND hwnd);

/**
 * @brief Show config menu (right-click)
 * @param hwnd Window handle
 * 
 * @details
 * Comprehensive settings: edit mode, timeout actions, presets, time format,
 * fonts (recursive scan), colors, animations (GIF/WebP + speed metrics),
 * help, language.
 * 
 * Modular helpers for maintainability.
 */
void ShowColorMenu(HWND hwnd);

/**
 * @brief Handle animation menu command
 * @return TRUE if handled
 */
BOOL HandleAnimationMenuCommand(HWND hwnd, UINT id);

#endif