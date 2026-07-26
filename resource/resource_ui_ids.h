#ifndef CATIME_RESOURCE_UI_IDS_H
#define CATIME_RESOURCE_UI_IDS_H

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
#define IDC_NOTIFICATION_RADIUS_LABEL 2026   /**< Notification corner radius label */
#define IDC_NOTIFICATION_RADIUS_SLIDER 2027  /**< Notification corner radius slider */
#define IDC_NOTIFICATION_RADIUS_TEXT 2028    /**< Notification corner radius text */
#define IDC_NOTIFICATION_FONT_SIZE_LABEL 2029   /**< Notification text height ratio label */
#define IDC_NOTIFICATION_FONT_SIZE_SLIDER 2030  /**< Notification text height ratio slider */
#define IDC_NOTIFICATION_FONT_SIZE_TEXT 2031    /**< Notification text height ratio text */

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
#define NOTIFICATION_MAX_WIDTH 1600              /**< Prevents excessively wide notifications on large displays */
#define NOTIFICATION_MIN_HEIGHT 48               /**< Keeps preview resizing usable */
#define NOTIFICATION_HEIGHT 80                   /**< Fixed height accommodates single-line content */
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
#define NOTIFICATION_TEXT_PADDING 40             /**< Text width padding */
#define NOTIFICATION_BOTTOM_MARGIN 20            /**< Bottom margin from screen edge */
#define NOTIFICATION_RIGHT_MARGIN 20             /**< Right margin from screen edge */

/** @brief Notification font constants */
#define NOTIFICATION_FONT_NAME L"Microsoft YaHei"  /**< Notification font family */
#define NOTIFICATION_CONTENT_FONT_SIZE 20        /**< Content font size */

/** @brief Notification color constants */
#define NOTIFICATION_BG_COLOR RGB(255, 255, 255)     /**< Background color (white) */
#define NOTIFICATION_CONTENT_COLOR RGB(100, 100, 100) /**< Fallback content text color (gray) */

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
#define HOTKEY_ID_TOGGLE_MILLISECONDS 112        /**< Toggle milliseconds display hotkey ID */
#define HOTKEY_ID_TOPMOST         113            /**< Toggle topmost hotkey ID */

/** @brief Hotkey configuration dialog identifiers */
#define CLOCK_IDD_HOTKEY_DIALOG 2100     /**< Hotkey configuration dialog */
#define IDC_HOTKEY_LABEL1 2101           /**< Hotkey label 1 */
#define IDC_HOTKEY_EDIT1 2102            /**< Hotkey edit control 1 */
#define IDC_HOTKEY_LABEL2 2103           /**< Hotkey label 2 */
#define IDC_HOTKEY_EDIT2 2104            /**< Hotkey edit control 2 */
#define IDC_HOTKEY_LABEL3 2105           /**< Hotkey label 3 */
#define IDC_HOTKEY_EDIT3 2106            /**< Hotkey edit control 3 */
#define CLOCK_IDM_HOTKEY_SETTINGS 5000   /**< Hotkey settings menu item (outside animation range 3000-4000) */

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

/** @brief Additional hotkey control identifiers (14) */
#define IDC_HOTKEY_LABEL14 2129          /**< Hotkey label 14 */
#define IDC_HOTKEY_EDIT14 2130           /**< Hotkey edit control 14 */

/** @brief Animation menu identifiers */
#define CLOCK_IDM_ANIMATIONS_MENU 2200       /**< Animations submenu */
#define CLOCK_IDM_ANIMATIONS_OPEN_DIR 2201   /**< Open animations folder */
#define CLOCK_IDM_ANIMATIONS_USE_LOGO 2202   /**< Use logo animation */
#define CLOCK_IDM_ANIMATIONS_USE_CPU 2203    /**< Use CPU animation */
#define CLOCK_IDM_ANIMATIONS_USE_MEM 2204    /**< Use memory animation */
#define CLOCK_IDM_ANIMATIONS_USE_BATTERY 2205 /**< Use battery percent icon */
#define CLOCK_IDM_ANIMATIONS_USE_CAPSLOCK 2207 /**< Use Caps Lock indicator */
#define CLOCK_IDM_ANIMATIONS_USE_NONE 2206   /**< Use transparent/hidden icon */
#define CLOCK_IDM_ANIM_SPEED_ORIGINAL 2209   /**< Original speed (no scaling) */
#define CLOCK_IDM_ANIM_SPEED_MEMORY 2210     /**< Memory-based animation speed */
#define CLOCK_IDM_ANIM_SPEED_CPU 2211        /**< CPU-based animation speed */
#define CLOCK_IDM_ANIM_SPEED_TIMER 2212      /**< Timer-based animation speed */
#define CLOCK_IDM_ANIM_SPEED_FIXED 2213      /**< User-selected fixed animation speed */
#define CLOCK_IDM_TASKBAR_MONITOR_CPU_MEMORY 2214 /**< Toggle taskbar CPU/memory */
#define CLOCK_IDM_TASKBAR_MONITOR_NETWORK 2215 /**< Toggle taskbar network speed */

/** @brief Animation menu base identifier */
#define CLOCK_IDM_ANIMATIONS_BASE 3000       /**< Base ID for dynamic animation menus */
#define CLOCK_IDM_ANIMATIONS_END 4000        /**< End ID for dynamic animation menus */

#endif /* CATIME_RESOURCE_UI_IDS_H */
