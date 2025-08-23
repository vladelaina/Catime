/**
 * @file tray_menu.h
 * @brief System tray context menu definitions
 * 
 * Menu item identifiers and functions for tray menu management
 */

#ifndef CLOCK_TRAY_MENU_H
#define CLOCK_TRAY_MENU_H

#include <windows.h>

/** @brief Time display options */
#define CLOCK_IDM_SHOW_CURRENT_TIME 150  /**< Toggle current time display */
#define CLOCK_IDM_24HOUR_FORMAT 151      /**< Toggle 24-hour format */
#define CLOCK_IDM_SHOW_SECONDS 152       /**< Toggle seconds display */

/** @brief Timer management commands */
#define CLOCK_IDM_TIMER_MANAGEMENT 159      /**< Timer management submenu */
#define CLOCK_IDM_TIMER_PAUSE_RESUME 158    /**< Pause/resume current timer */
#define CLOCK_IDM_TIMER_RESTART 177         /**< Restart current timer */
#define CLOCK_IDM_COUNT_UP_START 171        /**< Start count-up timer */
#define CLOCK_IDM_COUNT_UP_RESET 172        /**< Reset count-up timer */
#define CLOCK_IDM_COUNTDOWN_START_PAUSE 154  /**< Start/pause countdown */
#define CLOCK_IDM_COUNTDOWN_RESET 155       /**< Reset countdown timer */

/** @brief Window interaction */
#define CLOCK_IDC_EDIT_MODE 113             /**< Toggle edit mode */
#define CLOCK_IDM_TOPMOST 195               /**< Toggle always on top */

/** @brief Timeout action menu items */
#define CLOCK_IDM_SHOW_MESSAGE 121       /**< Show message action */
#define CLOCK_IDM_LOCK_SCREEN 122        /**< Lock screen action */
#define CLOCK_IDM_SHUTDOWN 123           /**< Shutdown system action */
#define CLOCK_IDM_RESTART 124            /**< Restart system action */
#define CLOCK_IDM_SLEEP 125              /**< Sleep system action */
#define CLOCK_IDM_BROWSE_FILE 131        /**< Browse for file action */
#define CLOCK_IDM_RECENT_FILE_1 126      /**< Recent file 1 action */
#define CLOCK_IDM_TIMEOUT_SHOW_TIME 135  /**< Show time timeout action */
#define CLOCK_IDM_TIMEOUT_COUNT_UP 136   /**< Count-up timeout action */

/** @brief Configuration options */
#define CLOCK_IDC_MODIFY_TIME_OPTIONS 156  /**< Modify quick time options */
#define CLOCK_IDM_TIME_FORMAT_DEFAULT 194   /**< Default time format 9:59 */
#define CLOCK_IDM_TIME_FORMAT_ZERO_PADDED 196   /**< Zero-padded format 09:59 */
#define CLOCK_IDM_TIME_FORMAT_FULL_PADDED 197   /**< Full zero-padded format 00:09:59 */

/** @brief Startup configuration */
#define CLOCK_IDC_SET_COUNTDOWN_TIME 173   /**< Set default countdown time */
#define CLOCK_IDC_START_COUNT_UP 175       /**< Start in count-up mode */
#define CLOCK_IDC_START_SHOW_TIME 176      /**< Start showing current time */
#define CLOCK_IDC_START_NO_DISPLAY 174     /**< Start with no display */
#define CLOCK_IDC_AUTO_START 160           /**< Windows auto-start option */

/** @brief Notification settings */
#define CLOCK_IDM_NOTIFICATION_SETTINGS 193  /**< Notification settings dialog */
#define CLOCK_IDM_NOTIFICATION_CONTENT 191   /**< Notification content config */
#define CLOCK_IDM_NOTIFICATION_DISPLAY 192   /**< Notification display config */

/** @brief Pomodoro timer options */
#define CLOCK_IDM_POMODORO_START 181      /**< Start Pomodoro session */
#define CLOCK_IDM_POMODORO_WORK 182       /**< Configure work time */
#define CLOCK_IDM_POMODORO_BREAK 183      /**< Configure short break */
#define CLOCK_IDM_POMODORO_LBREAK 184     /**< Configure long break */
#define CLOCK_IDM_POMODORO_LOOP_COUNT 185 /**< Configure cycle count */
#define CLOCK_IDM_POMODORO_RESET 186      /**< Reset Pomodoro timer */

/** @brief Color customization */
#define CLOCK_IDC_COLOR_VALUE 1301        /**< Color value input */
#define CLOCK_IDC_COLOR_PANEL 1302        /**< Color picker panel */

/** @brief Language selection */
#define CLOCK_IDM_LANG_CHINESE 161        /**< Simplified Chinese */
#define CLOCK_IDM_LANG_CHINESE_TRAD 163   /**< Traditional Chinese */
#define CLOCK_IDM_LANG_ENGLISH 162        /**< English */
#define CLOCK_IDM_LANG_SPANISH 164        /**< Spanish */
#define CLOCK_IDM_LANG_FRENCH 165         /**< French */
#define CLOCK_IDM_LANG_GERMAN 166         /**< German */
#define CLOCK_IDM_LANG_RUSSIAN 167        /**< Russian */
#define CLOCK_IDM_LANG_KOREAN 170         /**< Korean */

/**
 * @brief Display tray context menu
 * @param hwnd Parent window handle
 */
void ShowContextMenu(HWND hwnd);

/**
 * @brief Display color selection submenu
 * @param hwnd Parent window handle
 */
void ShowColorMenu(HWND hwnd);

#endif