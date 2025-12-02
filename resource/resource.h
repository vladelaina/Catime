/**
 * @file resource.h
 * @brief Resource identifiers and constants for Windows resources
 * 
 * Central header defining all dialog IDs, control IDs, menu IDs, icon resources,
 * font resources, and application constants used throughout Catime
 */

#ifndef CATIME_RESOURCE_H
#define CATIME_RESOURCE_H

/** @brief Application version information */
#define CATIME_VERSION "1.4.0-alpha5"    /**< Version string */
#define CATIME_VERSION_MAJOR 1           /**< Major version number */
#define CATIME_VERSION_MINOR 4           /**< Minor version number */
#define CATIME_VERSION_PATCH 0           /**< Patch version number */
#define CATIME_VERSION_BUILD 0           /**< Build number */

/** @brief Font license version information */
#define FONT_LICENSE_VERSION "1.0"       /**< Font license version string */

/** @brief External links and URLs */
#define CREDIT_LINK_URL L"https://space.bilibili.com/26087398"  /**< Credit link URL */

/** @brief Windows shell constants */
#define CSIDL_STARTUP 0x0007             /**< Startup folder CSIDL */

/** @brief Application limits and constraints */
#define MAX_RECENT_FILES 5               /**< Maximum recent files to remember */
#define MAX_TIME_OPTIONS 50              /**< Maximum configurable time options */
#define MIN_SCALE_FACTOR 0.5f            /**< Minimum window scale factor */
#define MAX_SCALE_FACTOR 100.0f          /**< Maximum window scale factor */
#define MAX_POMODORO_TIMES 10            /**< Maximum Pomodoro time configurations */

/** @brief Window message and layout constants */
#define CLOCK_WM_TRAYICON (WM_USER + 2)  /**< Custom tray icon message */
#define CLOCK_WM_ANIMATION_PREVIEW_LOADED (WM_USER + 3)  /**< Animation preview loaded message */
#define CLOCK_WM_PLUGIN_EXIT (WM_USER + 4)  /**< Plugin requested exit via <exit> tag */
#define WINDOW_HORIZONTAL_PADDING 190    /**< Accounts for window borders, shadow, and visual breathing room */
#define WINDOW_VERTICAL_PADDING 5        /**< Minimal vertical spacing for compact display */

/** @brief Timer ID constants */
#define TIMER_ID_MAIN 1                      /**< Main application timer */
#define IDT_MENU_DEBOUNCE 500                /**< Menu debounce timer */
#define IDT_ANIMATION_PREVIEW_DELAY 501      /**< Animation preview delay timer */
#define TIMER_ID_TOPMOST_RETRY 999           /**< Topmost retry timer (3 attempts) */
#define TIMER_ID_VISIBILITY_RETRY 1000       /**< Visibility retry timer (3 attempts) */
#define TIMER_ID_MINIAUDIO_CHECK 1001        /**< Miniaudio playback check timer */
#define TIMER_ID_PLAYSOUND_DONE 1002         /**< PlaySound completion check timer */
#define TIMER_ID_SYSTEM_BEEP_DONE 1003       /**< System beep completion timer */
#define TIMER_ID_FORCE_REDRAW 1004           /**< Force redraw timer */
#define TIMER_ID_CONFIG_SAVE 1005            /**< Config save debounce timer */
#define TIMER_ID_FONT_VALIDATION 1006        /**< Font validation timer (every 2s) */
#define TIMER_ID_VOLUME_PREVIEW 1007         /**< Volume preview auto-stop timer (3s limit) */
#define TIMER_ID_EDIT_MODE_REFRESH 2001      /**< Edit mode refresh timer */
#define TRAY_TIP_TIMER_ID 42421              /**< Tray tooltip update timer */

/** @brief Timer interval constants */
#define TIMER_REFRESH_INTERVAL_MS 150        /**< Edit mode: ~7 FPS provides responsive feedback without excessive redraws */
#define CONFIG_SAVE_DELAY_MS 500             /**< Debounce: Batches rapid setting changes to reduce disk writes */

/** @brief Media control virtual key codes */
#define VK_MEDIA_PLAY_PAUSE 0xB3         /**< Media play/pause key */
#define VK_MEDIA_STOP 0xB2               /**< Media stop key */
#define KEYEVENTF_KEYUP 0x0002           /**< Key up event flag */

/** @brief Visual effects constants */
#define BLUR_OPACITY 192                 /**< 75% opacity (192/255) balances blur effect visibility with transparency */
#define BLUR_TRANSITION_MS 200           /**< Smooth transition without feeling sluggish */

/** @brief Application URLs */
#define URL_GITHUB_REPO L"https://github.com/vladelaina/Catime"                      /**< GitHub repository URL */
#define URL_FEEDBACK L"https://message.bilibili.com/#/whisper/mid1862395225"         /**< Feedback URL */
#define URL_BILIBILI_SPACE L"https://space.bilibili.com/1862395225"                 /**< Bilibili space URL */

/** @brief Application icon resource */
#define IDI_CATIME 101                   /**< Main application icon */

/** @brief Language resource identifiers */
#define LANG_EN_INI       1001           /**< English language pack */
#define LANG_ZH_CN_INI    1002           /**< Simplified Chinese language pack */
#define LANG_ZH_TW_INI    1003           /**< Traditional Chinese language pack */
#define LANG_ES_INI       1004           /**< Spanish language pack */
#define LANG_FR_INI       1005           /**< French language pack */
#define LANG_DE_INI       1006           /**< German language pack */
#define LANG_RU_INI       1007           /**< Russian language pack */
#define LANG_PT_INI       1008           /**< Portuguese language pack */
#define LANG_JA_INI       1009           /**< Japanese language pack */
#define LANG_KO_INI       1010           /**< Korean language pack */

/** @brief Dialog resource identifiers */
#define CLOCK_ID_TRAY_APP_ICON 1001      /**< Tray icon identifier */
#define CLOCK_IDD_DIALOG1 1002           /**< Main input dialog */
#define CLOCK_IDD_COLOR_DIALOG 1003      /**< Color selection dialog */
#define IDD_INPUTBOX 1004                /**< Generic input box dialog */
#define IDD_STARTUP_TIME_DIALOG 1005     /**< Startup time configuration dialog */
#define CLOCK_IDD_SHORTCUT_DIALOG 1006   /**< Shortcut creation dialog */
#define CLOCK_IDD_STARTUP_DIALOG 1007    /**< Startup mode configuration dialog */
#define CLOCK_IDD_WEBSITE_DIALOG 1008    /**< Website configuration dialog */

/** @brief Common dialog control identifiers */
#define CLOCK_IDC_STATIC 1001            /**< Static text control */
#define IDC_STATIC_PROMPT 1005           /**< Input dialog prompt text */
#define IDC_EDIT_INPUT 1006              /**< Input dialog edit control */
#define CLOCK_IDC_EDIT 108               /**< Generic edit control */
#define CLOCK_IDC_BUTTON_OK 109          /**< OK button */
#define CLOCK_IDC_CUSTOMIZE_LEFT 112     /**< Left customization button */
#define CLOCK_IDC_EDIT_MODE 113          /**< Edit mode toggle */
#define CLOCK_IDC_TOGGLE_VISIBILITY 114  /**< Toggle window visibility */
#define CLOCK_IDC_MODIFY_OPTIONS 115     /**< Modify options button */

/** @brief File menu identifiers */
#define CLOCK_IDM_OPEN_FILE 125          /**< Open file menu item */
#define CLOCK_IDM_RECENT_FILE_1 126      /**< Recent file 1 menu item */
#define CLOCK_IDM_RECENT_FILE_2 127      /**< Recent file 2 menu item */
#define CLOCK_IDM_RECENT_FILE_3 128      /**< Recent file 3 menu item */
#define CLOCK_IDM_RECENT_FILE_4 129      /**< Recent file 4 menu item */
#define CLOCK_IDM_RECENT_FILE_5 130      /**< Recent file 5 menu item */
#define CLOCK_IDM_BROWSE_FILE 131        /**< Browse for file menu item */
#define CLOCK_IDM_CURRENT_FILE 127       /**< Current file menu item */

/** @brief Plugin menu identifiers */
#define CLOCK_IDM_PLUGINS 140                /**< Plugins submenu */
#define CLOCK_IDM_PLUGINS_BASE 4000          /**< Base ID for plugin menu items (4000-4499) */
#define CLOCK_IDM_PLUGINS_SETTINGS_BASE 4500 /**< Base ID for plugin settings items (4500-4998) */
#define CLOCK_IDM_PLUGINS_SHOW_FILE 4998     /**< Show plugin file content directly */
#define CLOCK_IDM_PLUGINS_OPEN_DIR 4999      /**< Open plugins folder menu item */

/** @brief Help and application menu identifiers */
#define CLOCK_IDM_ABOUT 132              /**< About dialog menu item */
#define CLOCK_IDM_CHECK_UPDATE 133       /**< Check for updates menu item */
#define CLOCK_IDM_HELP 134               /**< Help menu item */
#define CLOCK_IDM_SUPPORT 139            /**< Support menu item */
#define CLOCK_IDM_FEEDBACK 141           /**< Feedback menu item */

/** @brief Timeout action menu identifiers */
#define CLOCK_IDM_TIMEOUT_ACTION 120     /**< Timeout action submenu */
#define CLOCK_IDM_SHOW_MESSAGE 121       /**< Show message timeout action */
#define CLOCK_IDM_LOCK_SCREEN 122        /**< Lock screen timeout action */
#define CLOCK_IDM_SHUTDOWN 123           /**< Shutdown timeout action */
#define CLOCK_IDM_RESTART 124            /**< Restart timeout action */
#define CLOCK_IDM_TIMEOUT_SHOW_TIME 135  /**< Show time timeout action */
#define CLOCK_IDM_TIMEOUT_COUNT_UP 136   /**< Count-up timeout action */
#define CLOCK_IDM_OPEN_WEBSITE 137       /**< Open website timeout action */
#define CLOCK_IDM_CURRENT_WEBSITE 138    /**< Current website timeout action */

/** @brief Display and window menu identifiers */
#define CLOCK_IDM_SHOW_CURRENT_TIME 150  /**< Show current time menu item */
#define CLOCK_IDM_24HOUR_FORMAT 151      /**< 24-hour format menu item */
#define CLOCK_IDM_SHOW_SECONDS 152       /**< Show seconds menu item */
#define CLOCK_IDM_TOPMOST 187            /**< Window topmost menu item */

/** @brief Language selection menu identifiers */
#define CLOCK_IDM_LANGUAGE_MENU 160      /**< Language submenu */
#define CLOCK_IDM_LANG_CHINESE 161       /**< Simplified Chinese language */
#define CLOCK_IDM_LANG_ENGLISH 162       /**< English language */
#define CLOCK_IDM_LANG_CHINESE_TRAD 163  /**< Traditional Chinese language */
#define CLOCK_IDM_LANG_SPANISH 164       /**< Spanish language */
#define CLOCK_IDM_LANG_FRENCH 165        /**< French language */
#define CLOCK_IDM_LANG_GERMAN 166        /**< German language */
#define CLOCK_IDM_LANG_RUSSIAN 167       /**< Russian language */
#define CLOCK_IDM_LANG_PORTUGUESE 168    /**< Portuguese language */
#define CLOCK_IDM_LANG_JAPANESE 169      /**< Japanese language */
#define CLOCK_IDM_LANG_KOREAN 170        /**< Korean language */

/** @brief Timer control menu identifiers */
#define CLOCK_IDM_COUNT_UP 153           /**< Count-up timer menu item */
#define CLOCK_IDM_COUNT_UP_START 171     /**< Start count-up timer */
#define CLOCK_IDM_COUNT_UP_RESET 172     /**< Reset count-up timer */
#define CLOCK_IDM_COUNTDOWN_START_PAUSE 154  /**< Start/pause countdown timer */
#define CLOCK_IDM_COUNTDOWN_RESET 155    /**< Reset countdown timer */

/** @brief Startup configuration control identifiers */
#define CLOCK_IDC_SET_COUNTDOWN_TIME 173 /**< Set countdown time control */
#define CLOCK_IDC_START_NO_DISPLAY 174   /**< Start with no display */
#define CLOCK_IDC_START_COUNT_UP 175     /**< Start in count-up mode */
#define CLOCK_IDC_START_SHOW_TIME 176    /**< Start showing current time */

/** @brief Quick time menu base identifier */
#define CLOCK_IDM_QUICK_TIME_BASE 800    /**< Base ID for dynamic quick time menus */

/** @brief Basic menu identifiers */
#define CLOCK_IDM_CUSTOM_COUNTDOWN 101       /**< Custom countdown input */
#define CLOCK_IDM_EXIT 109                   /**< Exit application */

/** @brief Reset menu identifiers */
#define CLOCK_IDM_RESET_POSITION 199         /**< Reset window position and size */
#define CLOCK_IDM_RESET_ALL 200              /**< Reset all settings */

/** @brief Command range base identifiers */
#define CMD_QUICK_COUNTDOWN_BASE 102         /**< Quick countdown command range start */
#define CMD_QUICK_COUNTDOWN_END 108          /**< Quick countdown command range end */
#define CMD_COLOR_OPTIONS_BASE 201           /**< Color options command range start */
#define CMD_POMODORO_TIME_BASE 600           /**< Pomodoro time command range start */
#define CMD_POMODORO_TIME_END 609            /**< Pomodoro time command range end */
#define CMD_FONT_SELECTION_BASE 2000         /**< Font selection command range start */
#define CMD_FONT_SELECTION_END 3000          /**< Font selection command range end */

/** @brief Timer management menu identifiers */
#define CLOCK_IDM_TIMER_MANAGEMENT 159       /**< Timer management submenu */
#define CLOCK_IDM_TIMER_PAUSE_RESUME 158     /**< Pause/resume timer menu item */
#define CLOCK_IDM_TIMER_RESTART 177          /**< Restart timer menu item */
#define CLOCK_IDM_SLEEP 125                  /**< Sleep timeout action */

/** @brief Time format menu identifiers */
#define CLOCK_IDM_TIME_FORMAT_DEFAULT 194             /**< Default time format */
#define CLOCK_IDM_TIME_FORMAT_ZERO_PADDED 196         /**< Zero-padded time format */
#define CLOCK_IDM_TIME_FORMAT_FULL_PADDED 197         /**< Full-padded time format */
#define CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS 198   /**< Show milliseconds toggle */

/** @brief Time configuration control identifiers */
#define CLOCK_IDC_MODIFY_TIME_OPTIONS 156  /**< Modify time options control */
#define CLOCK_IDC_MODIFY_DEFAULT_TIME 157  /**< Modify default time control */
#define CLOCK_IDC_TIMEOUT_BROWSE 140       /**< Browse for timeout file */
#define CLOCK_IDC_AUTO_START 160           /**< Auto-start with Windows */

/** @brief Pomodoro timer menu identifiers */
#define CLOCK_IDM_POMODORO 500           /**< Pomodoro submenu */
#define CLOCK_IDM_POMODORO_START 181     /**< Start Pomodoro timer */
#define CLOCK_IDM_POMODORO_WORK 182      /**< Configure work time */
#define CLOCK_IDM_POMODORO_BREAK 183     /**< Configure short break time */
#define CLOCK_IDM_POMODORO_LBREAK 184    /**< Configure long break time */
#define CLOCK_IDM_POMODORO_LOOP_COUNT 185 /**< Configure loop count */
#define CLOCK_IDM_POMODORO_RESET 186     /**< Reset Pomodoro timer */
#define CLOCK_IDM_POMODORO_COMBINATION 188 /**< Pomodoro combination settings */

/** @brief Pomodoro dialog identifiers */
#define CLOCK_IDD_POMODORO_TIME_DIALOG 510  /**< Pomodoro time configuration dialog */
#define CLOCK_IDD_POMODORO_LOOP_DIALOG 513  /**< Pomodoro loop configuration dialog */
#define CLOCK_IDD_POMODORO_COMBO_DIALOG 514 /**< Pomodoro combination dialog */

/** @brief Pomodoro time menu base identifier */
#define CLOCK_IDM_POMODORO_TIME_BASE 600    /**< Base ID for dynamic Pomodoro time menus */

/** @brief Notification menu identifiers */
#define CLOCK_IDM_NOTIFICATION_CONTENT 191   /**< Notification content configuration */
#define CLOCK_IDM_NOTIFICATION_DISPLAY 192   /**< Notification display configuration */
#define CLOCK_IDM_NOTIFICATION_SETTINGS 193  /**< Notification settings */

/** @brief Notification dialog identifiers */
#define CLOCK_IDD_NOTIFICATION_MESSAGES_DIALOG 1010  /**< Notification messages dialog */
#define CLOCK_IDD_NOTIFICATION_DISPLAY_DIALOG 1011   /**< Notification display dialog */
#define CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG 2000  /**< Notification settings dialog */

/** @brief Notification dialog control identifiers */
#define IDC_NOTIFICATION_LABEL1 2001     /**< Notification message label 1 */
#define IDC_NOTIFICATION_EDIT1 2002      /**< Notification message edit 1 */

/** @brief Notification display control identifiers */
#define IDC_NOTIFICATION_TIME_LABEL 2007     /**< Notification timeout label */
#define IDC_NOTIFICATION_TIME_EDIT 2008      /**< Notification timeout edit */
#define IDC_DISABLE_NOTIFICATION_CHECK 2050  /**< Disable notifications checkbox */
#define IDC_NOTIFICATION_OPACITY_LABEL 2009  /**< Notification opacity label */
#define IDC_NOTIFICATION_OPACITY_EDIT 2010   /**< Notification opacity edit */
#define IDC_NOTIFICATION_OPACITY_TEXT 2021   /**< Notification opacity text */

/** @brief Notification group box identifiers */
#define IDC_NOTIFICATION_CONTENT_GROUP 2022  /**< Notification content group */
#define IDC_NOTIFICATION_DISPLAY_GROUP 2023  /**< Notification display group */
#define IDC_NOTIFICATION_METHOD_GROUP 2024   /**< Notification method group */
#define IDC_NOTIFICATION_AUDIO_GROUP 2025    /**< Notification audio settings group */

/** @brief Notification type and audio control identifiers */
#define IDC_NOTIFICATION_TYPE_CATIME 2011      /**< Catime notification type radio */
#define IDC_NOTIFICATION_TYPE_OS 2012          /**< OS notification type radio */
#define IDC_NOTIFICATION_TYPE_SYSTEM_MODAL 2013 /**< System modal notification type radio */
#define IDC_NOTIFICATION_SOUND_LABEL 2014      /**< Notification sound label */
#define IDC_NOTIFICATION_SOUND_COMBO 2015      /**< Notification sound combo box */
#define IDC_TEST_SOUND_BUTTON 2016             /**< Test sound button */
#define IDC_OPEN_SOUND_DIR_BUTTON 2017         /**< Open sound directory button */
#define IDC_VOLUME_LABEL 2018                  /**< Volume label */
#define IDC_VOLUME_SLIDER 2019                 /**< Volume slider control */
#define IDC_VOLUME_TEXT 2020                   /**< Volume text display */

/** @brief Notification window constants */
#define NOTIFICATION_MIN_WIDTH 350               /**< Ensures readability of typical notification messages */
#define NOTIFICATION_MAX_WIDTH 800               /**< Prevents excessively wide notifications on large displays */
#define NOTIFICATION_HEIGHT 80                   /**< Fixed height accommodates title + single-line content */
#define NOTIFICATION_TIMER_ID 1001               /**< Notification timer identifier */
#define NOTIFICATION_CLASS_NAME L"CatimeNotificationClass"  /**< Notification window class */
#define CLOSE_BTN_SIZE 16                        /**< Standard close button size for easy clicking */
#define CLOSE_BTN_MARGIN 10                      /**< Close button margin */
#define ANIMATION_TIMER_ID 1002                  /**< Animation timer identifier */
#define ANIMATION_STEP 5                         /**< Step size: 51 steps for 0-255 opacity range (~765ms total at 15ms interval) */
#define ANIMATION_INTERVAL 15                    /**< ~67 FPS provides smooth fade without excessive CPU usage */

/** @brief Notification UI layout constants */
#define NOTIFICATION_PADDING_H 15                /**< Horizontal padding */
#define NOTIFICATION_PADDING_V 10                /**< Vertical padding */
#define NOTIFICATION_TITLE_HEIGHT 25             /**< Title section height */
#define NOTIFICATION_CONTENT_SPACING 35          /**< Content vertical spacing */
#define NOTIFICATION_TEXT_PADDING 40             /**< Text width padding */
#define NOTIFICATION_BOTTOM_MARGIN 20            /**< Bottom margin from screen edge */
#define NOTIFICATION_RIGHT_MARGIN 20             /**< Right margin from screen edge */

/** @brief Notification font constants */
#define NOTIFICATION_FONT_NAME L"Microsoft YaHei"  /**< Notification font family */
#define NOTIFICATION_TITLE_FONT_SIZE 22          /**< Title font size */
#define NOTIFICATION_CONTENT_FONT_SIZE 20        /**< Content font size */

/** @brief Notification color constants */
#define NOTIFICATION_BG_COLOR RGB(255, 255, 255)     /**< Background color (white) */
#define NOTIFICATION_TITLE_COLOR RGB(0, 0, 0)        /**< Title text color (black) */
#define NOTIFICATION_CONTENT_COLOR RGB(100, 100, 100) /**< Content text color (gray) */
#define NOTIFICATION_BORDER_COLOR RGB(200, 200, 200)  /**< Border color (light gray) */

/** @brief Global hotkey identifiers for RegisterHotKey */
#define HOTKEY_ID_SHOW_TIME       100            /**< Show time hotkey ID */
#define HOTKEY_ID_COUNT_UP        101            /**< Count-up hotkey ID */
#define HOTKEY_ID_COUNTDOWN       102            /**< Countdown hotkey ID */
#define HOTKEY_ID_QUICK_COUNTDOWN1 103           /**< Quick countdown 1 hotkey ID */
#define HOTKEY_ID_QUICK_COUNTDOWN2 104           /**< Quick countdown 2 hotkey ID */
#define HOTKEY_ID_QUICK_COUNTDOWN3 105           /**< Quick countdown 3 hotkey ID */
#define HOTKEY_ID_POMODORO        106            /**< Pomodoro hotkey ID */
#define HOTKEY_ID_TOGGLE_VISIBILITY 107          /**< Toggle visibility hotkey ID */
#define HOTKEY_ID_EDIT_MODE       108            /**< Edit mode hotkey ID */
#define HOTKEY_ID_PAUSE_RESUME    109            /**< Pause/resume hotkey ID */
#define HOTKEY_ID_RESTART_TIMER   110            /**< Restart timer hotkey ID */
#define HOTKEY_ID_CUSTOM_COUNTDOWN 111           /**< Custom countdown hotkey ID */
#define HOTKEY_ID_TOGGLE_MILLISECONDS 116        /**< Toggle milliseconds display hotkey ID */

/** @brief Hotkey configuration dialog identifiers */
#define CLOCK_IDD_HOTKEY_DIALOG 2100     /**< Hotkey configuration dialog */
#define IDC_HOTKEY_LABEL1 2101           /**< Hotkey label 1 */
#define IDC_HOTKEY_EDIT1 2102            /**< Hotkey edit control 1 */
#define IDC_HOTKEY_LABEL2 2103           /**< Hotkey label 2 */
#define IDC_HOTKEY_EDIT2 2104            /**< Hotkey edit control 2 */
#define IDC_HOTKEY_LABEL3 2105           /**< Hotkey label 3 */
#define IDC_HOTKEY_EDIT3 2106            /**< Hotkey edit control 3 */
#define IDC_HOTKEY_NOTE 2107             /**< Hotkey note text */
#define CLOCK_IDM_HOTKEY_SETTINGS 3100   /**< Hotkey settings menu item*/

/** @brief Additional hotkey control identifiers (4-8) */
#define IDC_HOTKEY_LABEL4 2109           /**< Hotkey label 4 */
#define IDC_HOTKEY_EDIT4 2110            /**< Hotkey edit control 4 */
#define IDC_HOTKEY_LABEL5 2111           /**< Hotkey label 5 */
#define IDC_HOTKEY_EDIT5 2112            /**< Hotkey edit control 5 */
#define IDC_HOTKEY_LABEL6 2113           /**< Hotkey label 6 */
#define IDC_HOTKEY_EDIT6 2114            /**< Hotkey edit control 6 */
#define IDC_HOTKEY_LABEL7 2115           /**< Hotkey label 7 */
#define IDC_HOTKEY_EDIT7 2116            /**< Hotkey edit control 7 */
#define IDC_HOTKEY_LABEL8 2117           /**< Hotkey label 8 */
#define IDC_HOTKEY_EDIT8 2118            /**< Hotkey edit control 8 */

/** @brief Additional hotkey control identifiers (9-12) */
#define IDC_HOTKEY_LABEL9 2119           /**< Hotkey label 9 */
#define IDC_HOTKEY_EDIT9 2120            /**< Hotkey edit control 9 */
#define IDC_HOTKEY_LABEL10 2121          /**< Hotkey label 10 */
#define IDC_HOTKEY_EDIT10 2122           /**< Hotkey edit control 10 */
#define IDC_HOTKEY_LABEL11 2123          /**< Hotkey label 11 */
#define IDC_HOTKEY_EDIT11 2124           /**< Hotkey edit control 11 */
#define IDC_HOTKEY_LABEL12 2125          /**< Hotkey label 12 */
#define IDC_HOTKEY_EDIT12 2126           /**< Hotkey edit control 12 */

/** @brief Additional hotkey control identifiers (13) */
#define IDC_HOTKEY_LABEL13 2127          /**< Hotkey label 13 */
#define IDC_HOTKEY_EDIT13 2128           /**< Hotkey edit control 13 */

/** @brief Animation menu identifiers */
#define CLOCK_IDM_ANIMATIONS_MENU 2200       /**< Animations submenu */
#define CLOCK_IDM_ANIMATIONS_OPEN_DIR 2201   /**< Open animations folder */
#define CLOCK_IDM_ANIMATIONS_USE_LOGO 2202   /**< Use logo animation */
#define CLOCK_IDM_ANIMATIONS_USE_CPU 2203    /**< Use CPU animation */
#define CLOCK_IDM_ANIMATIONS_USE_MEM 2204    /**< Use memory animation */
#define CLOCK_IDM_ANIMATIONS_USE_NONE 2205   /**< Use transparent/hidden icon */
#define CLOCK_IDM_ANIM_SPEED_MEMORY 2210     /**< Memory-based animation speed */
#define CLOCK_IDM_ANIM_SPEED_CPU 2211        /**< CPU-based animation speed */
#define CLOCK_IDM_ANIM_SPEED_TIMER 2212      /**< Timer-based animation speed */

/** @brief Animation menu base identifier */
#define CLOCK_IDM_ANIMATIONS_BASE 3000       /**< Base ID for dynamic animation menus */
#define CLOCK_IDM_ANIMATIONS_END 4000        /**< End ID for dynamic animation menus */

/** @brief External notification configuration variables
 * @note NOTIFICATION_MAX_OPACITY and NOTIFICATION_TIMEOUT_MS now in g_AppConfig.notification.display
 */

/** @brief About dialog identifiers */
#define IDD_ABOUT_DIALOG 1050            /**< About dialog */
#define IDC_ABOUT_ICON 1005              /**< About dialog icon */
#define IDC_VERSION_TEXT 1006            /**< Version text control */
#define IDC_LIBS_TEXT 1007               /**< Libraries text control */
#define IDC_AUTHOR_TEXT 1008             /**< Author text control */
#define IDC_ABOUT_OK 1009                /**< About dialog OK button */
#define IDC_BUILD_DATE 1010              /**< Build date control */
#define IDC_COPYRIGHT 1011               /**< Copyright control */
#define IDC_CREDITS_LABEL 1012           /**< Credits label */
#define IDC_CREDIT_LINK 1013             /**< Credit link control */
#define IDS_CREDITS_TEXT 1014            /**< Credits text string */

/** @brief About dialog constants and text */
#define ABOUT_ICON_SIZE 200                              /**< About dialog icon size */
#define IDC_ABOUT_TITLE 1022                             /**< About dialog title control */
#define IDC_ABOUT_TITLE_TEXT L"About Catime"             /**< About dialog title text */
#define IDC_ABOUT_VERSION L"Version: %hs"                /**< Version format string */
#define IDC_BUILD_DATE_TEXT L"Build date: %hs"           /**< Build date format string */
#define IDC_COPYRIGHT_TEXT L"Copyright (C) 2025 By vladelaina"  /**< Copyright text */

/** @brief About dialog link control identifiers */
#define IDC_CREDITS 1015                 /**< Credits control */
#define IDC_FEEDBACK 1016                /**< Feedback link control */
#define IDC_GITHUB 1017                  /**< GitHub link control */
#define IDC_COPYRIGHT_LINK 1018          /**< Copyright link control */
#define IDC_SUPPORT 1019                 /**< Support link control */
#define IDC_BILIBILI_LINK 1020           /**< Bilibili link control */
#define IDC_GITHUB_LINK 1021             /**< GitHub link control */

/** @brief CLI help dialog identifiers */
#define IDD_CLI_HELP_DIALOG 1100         /**< CLI help dialog */
#define IDC_CLI_HELP_EDIT 1101           /**< CLI help edit control */

/** @brief Color dialog identifiers */
#define IDD_COLOR_DIALOG 1003            /**< Color selection dialog */
#define IDC_COLOR_VALUE 1301             /**< Color value control */
#define IDC_COLOR_PANEL 1302             /**< Color panel control */
#define CLOCK_IDC_COLOR_VALUE 1301       /**< Color value control (alias) */
#define CLOCK_IDC_COLOR_PANEL 1302       /**< Color panel control (alias) */

/** @brief Startup configuration control */
#define IDC_STARTUP_TIME 1401            /**< Startup time control */

/** @brief Error and update dialog identifiers */
#define IDD_ERROR_DIALOG 700            /**< Error message dialog */
#define IDC_ERROR_TEXT 701              /**< Error message text control */

/** @brief Update dialog identifiers */
#define IDD_UPDATE_DIALOG 710           /**< Update available dialog */
#define IDC_UPDATE_TEXT 711             /**< Update message text control */
#define IDC_UPDATE_EXIT_TEXT 712        /**< Update exit text control */
#define IDC_UPDATE_NOTES 713            /**< Update release notes text control with modern scrollbar */

/** @brief Update error dialog identifiers */
#define IDD_UPDATE_ERROR_DIALOG 720     /**< Update error dialog */
#define IDC_UPDATE_ERROR_TEXT 721       /**< Update error text control */

/** @brief No update dialog identifiers */
#define IDD_NO_UPDATE_DIALOG 730        /**< No update available dialog */
#define IDC_NO_UPDATE_TEXT 731          /**< No update text control */

/** @brief Exit dialog identifiers */
#define IDD_EXIT_DIALOG 750             /**< Application exit dialog */
#define IDC_EXIT_TEXT 751               /**< Exit message text control */

/** @brief Font menu identifier */
#define CLOCK_IDC_FONT_MENU 113         /**< Font selection submenu */

/** @brief Font menu item identifiers - Special fonts */
#define CLOCK_IDC_FONT_RECMONO 342      /**< RecMono font menu item */
#define CLOCK_IDC_FONT_DEPARTURE 320    /**< Departure font menu item */
#define CLOCK_IDC_FONT_TERMINESS 343    /**< Terminess font menu item */
#define CLOCK_IDC_FONT_YESTERYEAR 390   /**< Yesteryear font menu item */
#define CLOCK_IDC_FONT_ZCOOL_KUAILE 391 /**< ZCOOL KuaiLe font menu item */
#define CLOCK_IDC_FONT_PROFONT 392      /**< ProFont font menu item */
#define CLOCK_IDC_FONT_DADDYTIME 393    /**< DaddyTime font menu item */

/** @brief Font menu item identifiers - Google Fonts collection */
#define CLOCK_IDC_FONT_JACQUARD 361             /**< Jacquard 12 font menu item */
#define CLOCK_IDC_FONT_JACQUARDA 362            /**< Jacquarda Bastarda 9 font menu item */
#define CLOCK_IDC_FONT_PIXELIFY 373             /**< Pixelify Sans Medium font menu item */
#define CLOCK_IDC_FONT_RUBIK_BURNED 377         /**< Rubik Burned font menu item */
#define CLOCK_IDC_FONT_RUBIK_GLITCH 379         /**< Rubik Glitch font menu item */
#define CLOCK_IDC_FONT_RUBIK_MARKER_HATCH 380   /**< Rubik Marker Hatch font menu item */
#define CLOCK_IDC_FONT_RUBIK_PUDDLES 381        /**< Rubik Puddles font menu item */
#define CLOCK_IDC_FONT_WALLPOET 389             /**< Wallpoet font menu item */
#define CLOCK_IDC_FONT_ADVANCED 395             /**< Font advanced options menu item */
#define CLOCK_IDC_FONT_LICENSE_AGREE 396        /**< Font license agreement menu item */

/** @brief Font license agreement dialog identifiers */
#define IDD_FONT_LICENSE_DIALOG 740             /**< Font license agreement dialog */
#define IDC_FONT_LICENSE_TEXT 741               /**< Font license agreement text control */
#define IDC_FONT_LICENSE_AGREE_BTN 742          /**< Font license agreement button */
#define IDC_FONT_LICENSE_CANCEL_BTN 743         /**< Font license cancel button */

/** @brief System font picker dialog identifiers */
#define IDD_FONT_PICKER_SIMPLE 758              /**< Simple system font picker dialog */
#define IDC_FONT_LIST_SIMPLE 759                /**< System font list control */
#define IDC_FONT_PICKER_LABEL 757               /**< Font picker label */
#define CLOCK_IDM_SYSTEM_FONT_PICKER 3200       /**< System font picker menu item */

/** @brief Plugin security dialog identifiers */
#define IDD_PLUGIN_SECURITY_DIALOG 760          /**< Plugin security confirmation dialog */
#define IDC_PLUGIN_SECURITY_ICON 761            /**< Plugin security icon control */
#define IDC_PLUGIN_SECURITY_TEXT 762            /**< Plugin security text control */
#define IDC_PLUGIN_SECURITY_CANCEL_BTN 764      /**< Plugin security cancel button */
#define IDC_PLUGIN_SECURITY_RUN_ONCE_BTN 765    /**< Plugin security run once button */
#define IDC_PLUGIN_SECURITY_TRUST_BTN 766       /**< Plugin security trust button */

/** @brief Font resource identifiers - Special fonts */
#define IDR_FONT_RECMONO 442            /**< RecMono font resource */
#define IDR_FONT_DEPARTURE 420          /**< Departure font resource */
#define IDR_FONT_TERMINESS 443          /**< Terminess font resource */
#define IDR_FONT_PROFONT 492            /**< ProFont font resource */
#define IDR_FONT_DADDYTIME 493          /**< DaddyTime font resource */

/** @brief Font resource identifiers - Google Fonts collection */
#define IDR_FONT_JACQUARD 461               /**< Jacquard 12 font resource */
#define IDR_FONT_JACQUARDA 462              /**< Jacquarda Bastarda 9 font resource */
#define IDR_FONT_PIXELIFY 473               /**< Pixelify Sans Medium font resource */
#define IDR_FONT_RUBIK_BURNED 477           /**< Rubik Burned font resource */
#define IDR_FONT_RUBIK_GLITCH 479           /**< Rubik Glitch font resource */
#define IDR_FONT_RUBIK_MARKER_HATCH 480     /**< Rubik Marker Hatch font resource */
#define IDR_FONT_RUBIK_PUDDLES 481          /**< Rubik Puddles font resource */
#define IDR_FONT_WALLPOET 489               /**< Wallpoet font resource */


#endif
