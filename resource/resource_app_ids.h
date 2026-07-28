#ifndef CATIME_RESOURCE_APP_IDS_H
#define CATIME_RESOURCE_APP_IDS_H

/** @brief Application version information */
#include "catime_version_numeric.h"

/**
 * @brief Configuration Reset Switch
 * Set to 1 to force a clean configuration reset on version mismatch.
 * Useful for major updates with breaking changes in config structure.
 * WARNING: This will delete the user's config.ini without backup!
 */
#define FORCE_CONFIG_RESET_ON_UPDATE 0

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
#define CLOCK_WM_MAIN_TIMER_TICK (WM_USER + 5)  /**< High-precision main timer tick for smooth milliseconds */
#define CLOCK_WM_TRAY_OPACITY_WHEEL (WM_USER + 6)  /**< Tray icon wheel opacity change */
#define CLOCK_WM_PLUGIN_DATA_REDRAW (WM_USER + 7)  /**< Coalesced plugin data redraw request */

/** @brief Modeless dialog result notification messages */
#define WM_DIALOG_COUNTDOWN     (WM_USER + 10)  /**< Countdown dialog result: wParam=seconds, lParam=0 */
#define WM_DIALOG_SHORTCUT      (WM_USER + 11)  /**< Shortcut time dialog result: config saved */
#define WM_DIALOG_COLOR         (WM_USER + 12)  /**< Color dialog result: wParam=0(cancel)/1(ok) */
#define WM_DIALOG_CLOSED        (WM_USER + 13)  /**< Generic dialog closed notification */
#define WM_DIALOG_UPDATE        (WM_USER + 14)  /**< Update dialog result: wParam=IDYES(update)/IDNO(later) */
#define WM_DIALOG_FONT_LICENSE  (WM_USER + 15)  /**< Font license dialog result: wParam=IDOK(agree)/IDCANCEL */
#define WM_DIALOG_PLUGIN_SECURITY (WM_USER + 16) /**< Plugin security dialog result: wParam=IDYES(trust)/IDOK(once)/IDCANCEL */
#define WM_UPDATE_CHECK_RESULT (WM_USER + 17)   /**< Update check result: wParam=1(update available)/0(no update), lParam=0 */
#define WM_PLUGIN_HOT_RELOAD   (WM_USER + 18)   /**< Plugin hot-reload request: wParam=plugin index */
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
#define TIMER_ID_EDIT_MODE_REFRESH 2001      /**< Edit mode refresh timer */
#define TIMER_ID_RENDER_ANIMATION 2002       /**< Dedicated animation render timer (30-60 FPS) */
#define TIMER_ID_TOPMOST_ENFORCE 2003        /**< Fast topmost enforcement when near taskbar (50ms) */
#define TIMER_ID_TRAY_ANIMATION 2004         /**< Tray animation fallback timer */
#define TIMER_ID_CI_EXIT 2005                /**< CI smoke-mode auto-exit timer */
#define TIMER_ID_TOPMOST_APPLY_RETRY 2006    /**< Retry failed topmost/non-topmost application */
#define TIMER_ID_DISPLAY_RESTORE 2007        /**< Restore window position after display/DPI changes */
#define TIMER_ID_TOPMOST_VISIBILITY_RESTORE 42425 /**< Restore topmost window after external hide */
#define TRAY_TIP_TIMER_ID 42421              /**< Tray tooltip update timer */

/** @brief Timer interval constants */
#define TIMER_REFRESH_INTERVAL_MS 150        /**< Edit mode: ~7 FPS provides responsive feedback without excessive redraws */
#define CONFIG_SAVE_DELAY_MS 500             /**< Debounce: Batches rapid setting changes to reduce disk writes */

/** @brief Visual effects constants */
#define BLUR_OPACITY 192                 /**< 75% opacity (192/255) balances blur effect visibility with transparency */
#define BLUR_TRANSITION_MS 200           /**< Smooth transition without feeling sluggish */

/** @brief Application URLs */
#define URL_GITHUB_REPO L"https://github.com/vladelaina/Catime"                      /**< GitHub repository URL */
#define URL_MICROSOFT_STORE L"ms-windows-store://pdp/?productid=9N3MZDF1Z34V"       /**< Microsoft Store product page */
#define URL_MICROSOFT_STORE_WEB L"https://apps.microsoft.com/detail/9N3MZDF1Z34V"   /**< Microsoft Store web page */
#define URL_FEEDBACK L"https://message.bilibili.com/#/whisper/mid1862395225"         /**< Feedback URL */
#define URL_BILIBILI_SPACE L"https://space.bilibili.com/1862395225"                 /**< Bilibili space URL */
#define URL_VLAINA L"https://vlaina.com/r/catime_win"                               /**< Vlaina project URL */
#define URL_TRAY_ANIMATIONS L"https://cati.me/tray"                                  /**< Online tray animation library */

/** @brief Application icon resource */
#define IDI_CATIME 101                   /**< Main application icon */

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
#define CLOCK_IDM_PLUGINS_SETTINGS_BASE 4500 /**< Base ID for plugin settings items (4500-4997) */
#define CLOCK_IDM_CUSTOM_TEXT_DISPLAY 4998   /**< Display custom text via custom_display.txt */
#define CLOCK_IDM_PLUGINS_OPEN_DIR 4999      /**< Open plugins folder menu item */

/** @brief Help and application menu identifiers */
#define CLOCK_IDM_ABOUT 132              /**< About dialog menu item */
#define CLOCK_IDM_CHECK_UPDATE 133       /**< Check for updates menu item */
#define CLOCK_IDM_HELP 134               /**< Help menu item */
#define CLOCK_IDM_SUPPORT 139            /**< Support menu item */
#define CLOCK_IDM_FEEDBACK 141           /**< Feedback menu item */
#define CLOCK_IDM_VLAINA 142             /**< Vlaina project menu item */

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
#define CLOCK_IDM_TEXT_EFFECT_BASE 5300  /**< Text effect menu command range start */
#define CLOCK_IDM_TEXT_EFFECT_END (CLOCK_IDM_TEXT_EFFECT_BASE + 99) /**< Text effect menu command range end */

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
#define CLOCK_IDM_LANG_JAPANESE    40110
#define CLOCK_IDM_LANG_KOREAN      40111

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
#define CLOCK_IDC_START_POMODORO 177     /**< Start in pomodoro mode */

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
#define CLOCK_IDM_TIMER_RESTART 178          /**< Restart timer menu item */
#define CLOCK_IDM_SLEEP 125                  /**< Sleep timeout action */

#endif /* CATIME_RESOURCE_APP_IDS_H */
