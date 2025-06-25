#ifndef CATIME_RESOURCE_H
#define CATIME_RESOURCE_H

/* Version and update information */
#define CATIME_VERSION "1.1.2"
#define CATIME_VERSION_MAJOR 1
#define CATIME_VERSION_MINOR 1
#define CATIME_VERSION_PATCH 2
#define CATIME_VERSION_BUILD 0

/* Credits link */
#define CREDIT_LINK_URL L"https://space.bilibili.com/26087398"

/*==================================
 * System constant definitions
 *==================================*/
#define CSIDL_STARTUP 0x0007
#define MAX_RECENT_FILES 5
#define MAX_TIME_OPTIONS 10
#define MIN_SCALE_FACTOR 0.5f
#define MAX_SCALE_FACTOR 100.0f
#define CLOCK_WM_TRAYICON (WM_USER + 2)
#define MAX_POMODORO_TIMES 10 // Maximum capacity of Pomodoro time array

/* Window size constants */
#define WINDOW_HORIZONTAL_PADDING 190 // Left and right
#define WINDOW_VERTICAL_PADDING -5    // Top and bottom

/* System key definitions */
#define VK_MEDIA_PLAY_PAUSE 0xB3
#define VK_MEDIA_STOP 0xB2
#define KEYEVENTF_KEYUP 0x0002

/* Visual effect constants */
#define BLUR_OPACITY 192
#define BLUR_TRANSITION_MS 200

/* External links */
#define URL_GITHUB_REPO L"https://github.com/vladelaina/Catime"
#define URL_FEEDBACK L"https://message.bilibili.com/#/whisper/mid1862395225"
#define URL_BILIBILI_SPACE L"https://space.bilibili.com/1862395225"

/*==================================
 * Main icons and application ID (100-199)
 *==================================*/
#define IDI_CATIME 101

/*==================================
 * Core dialogs and basic controls (1000-1099)
 *==================================*/
#define CLOCK_ID_TRAY_APP_ICON 1001
#define CLOCK_IDD_DIALOG1 1002
#define CLOCK_IDD_COLOR_DIALOG 1003
#define IDD_INPUTBOX 1004
#define IDD_STARTUP_TIME_DIALOG 1005
#define CLOCK_IDD_SHORTCUT_DIALOG 1006
#define CLOCK_IDD_STARTUP_DIALOG 1007
#define CLOCK_IDD_WEBSITE_DIALOG 1008

#define CLOCK_IDC_STATIC 1001
#define IDC_STATIC_PROMPT 1005
#define IDC_EDIT_INPUT 1006
#define CLOCK_IDC_EDIT 108
#define CLOCK_IDC_BUTTON_OK 109
#define CLOCK_IDC_CUSTOMIZE_LEFT 112
#define CLOCK_IDC_EDIT_MODE 113
#define CLOCK_IDC_MODIFY_OPTIONS 114

/*==================================
 * Main menu items (120-199)
 *==================================*/
/* File operations */
#define CLOCK_IDM_OPEN_FILE 125
#define CLOCK_IDM_RECENT_FILE_1 126
#define CLOCK_IDM_RECENT_FILE_2 127
#define CLOCK_IDM_RECENT_FILE_3 128
#define CLOCK_IDM_RECENT_FILE_4 129
#define CLOCK_IDM_RECENT_FILE_5 130
#define CLOCK_IDM_BROWSE_FILE 131
#define CLOCK_IDM_CURRENT_FILE 127 // Current file menu item ID

/* About and help */
#define CLOCK_IDM_ABOUT 132
#define CLOCK_IDM_CHECK_UPDATE 133 // Check update menu item ID
#define CLOCK_IDM_HELP 134         // Help menu item ID
#define CLOCK_IDM_SUPPORT 139      // Support option menu item ID
#define CLOCK_IDM_FEEDBACK 141     // Feedback option menu item ID

/* Timeout actions */
#define CLOCK_IDM_TIMEOUT_ACTION 120
#define CLOCK_IDM_SHOW_MESSAGE 121
#define CLOCK_IDM_LOCK_SCREEN 122
#define CLOCK_IDM_SHUTDOWN 123
#define CLOCK_IDM_RESTART 124
#define CLOCK_IDM_TIMEOUT_SHOW_TIME 135 // Timeout action: Show current time
#define CLOCK_IDM_TIMEOUT_COUNT_UP 136  // Timeout action: Count up
#define CLOCK_IDM_OPEN_WEBSITE 137      // Timeout action: Open website
#define CLOCK_IDM_CURRENT_WEBSITE 138   // Current website menu item ID

/* Basic settings */
#define CLOCK_IDM_SHOW_CURRENT_TIME 150
#define CLOCK_IDM_24HOUR_FORMAT 151
#define CLOCK_IDM_SHOW_SECONDS 152
#define CLOCK_IDM_TOPMOST 187 // Topmost option ID

/*==================================
 * Multilingual support (160-179)
 *==================================*/
#define CLOCK_IDM_LANGUAGE_MENU 160
#define CLOCK_IDM_LANG_CHINESE 161
#define CLOCK_IDM_LANG_ENGLISH 162
#define CLOCK_IDM_LANG_CHINESE_TRAD 163
#define CLOCK_IDM_LANG_SPANISH 164
#define CLOCK_IDM_LANG_FRENCH 165
#define CLOCK_IDM_LANG_GERMAN 166
#define CLOCK_IDM_LANG_RUSSIAN 167
#define CLOCK_IDM_LANG_PORTUGUESE 168
#define CLOCK_IDM_LANG_JAPANESE 169
#define CLOCK_IDM_LANG_KOREAN 170

/*==================================
 * Timer functionality (170-179)
 *==================================*/
#define CLOCK_IDM_COUNT_UP 153
#define CLOCK_IDM_COUNT_UP_START 171
#define CLOCK_IDM_COUNT_UP_RESET 172
#define CLOCK_IDM_COUNTDOWN_START_PAUSE 154
#define CLOCK_IDM_COUNTDOWN_RESET 155
#define CLOCK_IDC_SET_COUNTDOWN_TIME 173
#define CLOCK_IDC_START_NO_DISPLAY 174
#define CLOCK_IDC_START_COUNT_UP 175
#define CLOCK_IDC_START_SHOW_TIME 176

#define CLOCK_IDC_MODIFY_TIME_OPTIONS 156
#define CLOCK_IDC_MODIFY_DEFAULT_TIME 157
#define CLOCK_IDC_TIMEOUT_BROWSE 140
#define CLOCK_IDC_AUTO_START 160

/*==================================
 * Pomodoro functionality (180-199, 500-599)
 *==================================*/
#define CLOCK_IDM_POMODORO 500
#define CLOCK_IDM_POMODORO_START 181       // Start Pomodoro
#define CLOCK_IDM_POMODORO_WORK 182        // Set work time
#define CLOCK_IDM_POMODORO_BREAK 183       // Set short break time
#define CLOCK_IDM_POMODORO_LBREAK 184      // Set long break time
#define CLOCK_IDM_POMODORO_LOOP_COUNT 185  // Set loop count
#define CLOCK_IDM_POMODORO_RESET 186       // Reset Pomodoro
#define CLOCK_IDM_POMODORO_COMBINATION 188 // Pomodoro combination

/* Pomodoro dialogs */
#define CLOCK_IDD_POMODORO_TIME_DIALOG 510
#define CLOCK_IDD_POMODORO_LOOP_DIALOG 513
#define CLOCK_IDD_POMODORO_COMBO_DIALOG 514

/* Pomodoro dynamic menu base */
#define CLOCK_IDM_POMODORO_TIME_BASE 600 // Base ID for Pomodoro time menu items

/*==================================
 * Notification system (190-199, 1000-1099, 2000-2099)
 *==================================*/
#define CLOCK_IDM_NOTIFICATION_CONTENT 191
#define CLOCK_IDM_NOTIFICATION_DISPLAY 192
#define CLOCK_IDM_NOTIFICATION_SETTINGS 193 // Integrated notification settings menu item

/* Notification dialogs */
#define CLOCK_IDD_NOTIFICATION_MESSAGES_DIALOG 1010
#define CLOCK_IDD_NOTIFICATION_DISPLAY_DIALOG 1011
#define CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG 2000

/* Notification controls - Message editing */
#define IDC_NOTIFICATION_LABEL1 2001
#define IDC_NOTIFICATION_EDIT1 2002
#define IDC_NOTIFICATION_LABEL2 2003
#define IDC_NOTIFICATION_EDIT2 2004
#define IDC_NOTIFICATION_LABEL3 2005
#define IDC_NOTIFICATION_EDIT3 2006

/* Notification controls - Display settings */
#define IDC_NOTIFICATION_TIME_LABEL 2007
#define IDC_NOTIFICATION_TIME_EDIT 2008
#define IDC_DISABLE_NOTIFICATION_CHECK 2050
#define IDC_NOTIFICATION_OPACITY_LABEL 2009
#define IDC_NOTIFICATION_OPACITY_EDIT 2010
#define IDC_NOTIFICATION_OPACITY_TEXT 2021 // Opacity percentage text

/* Notification controls - Group boxes */
#define IDC_NOTIFICATION_CONTENT_GROUP 2022 // Notification content group box ID
#define IDC_NOTIFICATION_DISPLAY_GROUP 2023 // Notification display group box ID
#define IDC_NOTIFICATION_METHOD_GROUP 2024  // Notification method group box ID

/* Notification controls - Type selection */
#define IDC_NOTIFICATION_TYPE_CATIME 2011
#define IDC_NOTIFICATION_TYPE_OS 2012
#define IDC_NOTIFICATION_TYPE_SYSTEM_MODAL 2013
#define IDC_NOTIFICATION_SOUND_LABEL 2014
#define IDC_NOTIFICATION_SOUND_COMBO 2015
#define IDC_TEST_SOUND_BUTTON 2016     // Test audio button
#define IDC_OPEN_SOUND_DIR_BUTTON 2017 // Open audio directory button
#define IDC_VOLUME_LABEL 2018          // Volume label
#define IDC_VOLUME_SLIDER 2019         // Volume slider
#define IDC_VOLUME_TEXT 2020           // Volume percentage text

/* Notification window constants */
#define NOTIFICATION_MIN_WIDTH 350 // Notification window minimum width (pixels)
#define NOTIFICATION_MAX_WIDTH 800 // Notification window maximum width (pixels)
#define NOTIFICATION_HEIGHT 80     // Notification window height (pixels)
#define NOTIFICATION_TIMER_ID 1001 // Notification timeout timer ID
#define NOTIFICATION_CLASS_NAME L"CatimeNotificationClass" // Notification window class name
#define CLOSE_BTN_SIZE 16       // Close button size (pixels)
#define CLOSE_BTN_MARGIN 10     // Close button margin (pixels)
#define ANIMATION_TIMER_ID 1002 // Animation timer ID
#define ANIMATION_STEP 5        // Transparency change per step (0-255)
#define ANIMATION_INTERVAL 15   // Animation frame interval (milliseconds)

/* Hotkey settings dialog and controls (2100-2199) */
#define CLOCK_IDD_HOTKEY_DIALOG 2100   // Hotkey settings dialog ID
#define IDC_HOTKEY_LABEL1 2101         // "Show current time" label ID
#define IDC_HOTKEY_EDIT1 2102          // Show current time hotkey edit control ID
#define IDC_HOTKEY_LABEL2 2103         // "Count up" label ID
#define IDC_HOTKEY_EDIT2 2104          // Count up hotkey edit control ID
#define IDC_HOTKEY_LABEL3 2105         // "Default countdown" label ID
#define IDC_HOTKEY_EDIT3 2106          // Default countdown hotkey edit control ID
#define IDC_HOTKEY_NOTE 2107           // Hotkey note label ID
#define CLOCK_IDM_HOTKEY_SETTINGS 2108 // Hotkey settings menu item ID

// Additional hotkey controls
#define IDC_HOTKEY_LABEL4 2109 // "Pomodoro" label ID
#define IDC_HOTKEY_EDIT4 2110  // Pomodoro hotkey edit control ID
#define IDC_HOTKEY_LABEL5 2111 // "Hide/Show" label ID
#define IDC_HOTKEY_EDIT5 2112  // Hide/Show hotkey edit control ID
#define IDC_HOTKEY_LABEL6 2113 // "Edit mode" label ID
#define IDC_HOTKEY_EDIT6 2114  // Edit mode hotkey edit control ID
#define IDC_HOTKEY_LABEL7 2115 // "Pause/Resume" label ID
#define IDC_HOTKEY_EDIT7 2116  // Pause/Resume hotkey edit control ID
#define IDC_HOTKEY_LABEL8 2117 // "Restart" label ID
#define IDC_HOTKEY_EDIT8 2118  // Restart hotkey edit control ID

// Quick countdown hotkey controls
#define IDC_HOTKEY_LABEL9 2119  // "Quick countdown 1" label ID
#define IDC_HOTKEY_EDIT9 2120   // Quick countdown 1 hotkey edit control ID
#define IDC_HOTKEY_LABEL10 2121 // "Quick countdown 2" label ID
#define IDC_HOTKEY_EDIT10 2122  // Quick countdown 2 hotkey edit control ID
#define IDC_HOTKEY_LABEL11 2123 // "Quick countdown 3" label ID
#define IDC_HOTKEY_EDIT11 2124  // Quick countdown 3 hotkey edit control ID
#define IDC_HOTKEY_LABEL12 2125 // "Countdown" label ID
#define IDC_HOTKEY_EDIT12 2126  // Countdown hotkey edit control ID

/* Notification maximum opacity configuration */
extern int NOTIFICATION_MAX_OPACITY;
extern int NOTIFICATION_TIMEOUT_MS; // Notification display duration

/*==================================
 * About dialog (1050-1099)
 *==================================*/
#define IDD_ABOUT_DIALOG 1050
#define IDC_ABOUT_ICON 1005
#define IDC_VERSION_TEXT 1006
#define IDC_LIBS_TEXT 1007
#define IDC_AUTHOR_TEXT 1008
#define IDC_ABOUT_OK 1009
#define IDC_BUILD_DATE 1010
#define IDC_COPYRIGHT 1011
#define IDC_CREDITS_LABEL 1012 // "Credits:" label
#define IDC_CREDIT_LINK 1013   // Clickable link
#define IDS_CREDITS_TEXT 1014  // String resource ID

/* About dialog constants */
#define ABOUT_ICON_SIZE 200
#define IDC_ABOUT_TITLE 1022
#define IDC_ABOUT_TITLE_TEXT L"About Catime"
#define IDC_ABOUT_VERSION L"Version: %hs"
#define IDC_BUILD_DATE_TEXT L"Build date: %hs"
#define IDC_COPYRIGHT_TEXT L"Copyright (C) 2025 By vladelaina"

/* Bottom link controls */
#define IDC_CREDITS 1015        // Credits button
#define IDC_FEEDBACK 1016       // Feedback button
#define IDC_GITHUB 1017         // GitHub button
#define IDC_COPYRIGHT_LINK 1018 // Copyright statement button
#define IDC_SUPPORT 1019        // Support button
#define IDC_BILIBILI_LINK 1020  // Bilibili link
#define IDC_GITHUB_LINK 1021    // GitHub link

/*==================================
 * Color dialog (1300-1399)
 *==================================*/
#define IDD_COLOR_DIALOG 1003
#define IDC_COLOR_VALUE 1301
#define IDC_COLOR_PANEL 1302

/*==================================
 * Startup settings dialog (1400-1499)
 *==================================*/
#define IDC_STARTUP_TIME 1401

/*==================================
 * Error and update dialogs (700-799)
 *==================================*/
/* Error dialog */
#define IDD_ERROR_DIALOG 700
#define IDC_ERROR_TEXT 701

/* Update dialog */
#define IDD_UPDATE_DIALOG 710
#define IDC_UPDATE_TEXT 711
#define IDC_UPDATE_EXIT_TEXT 712

/* Update error dialog */
#define IDD_UPDATE_ERROR_DIALOG 720
#define IDC_UPDATE_ERROR_TEXT 721

/* No update needed dialog */
#define IDD_NO_UPDATE_DIALOG 730
#define IDC_NO_UPDATE_TEXT 731

/*==================================
 * Font menu items (300-399)
 *==================================*/
#define CLOCK_IDC_FONT_MENU 113

/* Basic fonts */
#define CLOCK_IDC_FONT_RECMONO 342
#define CLOCK_IDC_FONT_DEPARTURE 320
#define CLOCK_IDC_FONT_TERMINESS 343
#define CLOCK_IDC_FONT_YESTERYEAR 390
#define CLOCK_IDC_FONT_ZCOOL_KUAILE 391
#define CLOCK_IDC_FONT_PROFONT 392
#define CLOCK_IDC_FONT_DADDYTIME 393
#define CLOCK_IDC_FONT_PINYON_SCRIPT 394

/* Extended fonts */
#define CLOCK_IDC_FONT_ARBUTUS 347
#define CLOCK_IDC_FONT_BERKSHIRE 348
#define CLOCK_IDC_FONT_CAVEAT 349
#define CLOCK_IDC_FONT_CREEPSTER 350
#define CLOCK_IDC_FONT_DOTGOTHIC 351
#define CLOCK_IDC_FONT_DOTO 352
#define CLOCK_IDC_FONT_FOLDIT 354
#define CLOCK_IDC_FONT_FREDERICKA 355
#define CLOCK_IDC_FONT_FRIJOLE 356
#define CLOCK_IDC_FONT_GWENDOLYN 358
#define CLOCK_IDC_FONT_HANDJET 359
#define CLOCK_IDC_FONT_INKNUT 360
#define CLOCK_IDC_FONT_JACQUARD 361
#define CLOCK_IDC_FONT_JACQUARDA 362
#define CLOCK_IDC_FONT_KAVOON 363
#define CLOCK_IDC_FONT_KUMAR_ONE_OUTLINE 364
#define CLOCK_IDC_FONT_KUMAR_ONE 365
#define CLOCK_IDC_FONT_LAKKI_REDDY 366
#define CLOCK_IDC_FONT_LICORICE 367
#define CLOCK_IDC_FONT_MA_SHAN_ZHENG 368
#define CLOCK_IDC_FONT_MOIRAI_ONE 369
#define CLOCK_IDC_FONT_MYSTERY_QUEST 370
#define CLOCK_IDC_FONT_NOTO_NASTALIQ 371
#define CLOCK_IDC_FONT_PIEDRA 372
#define CLOCK_IDC_FONT_PIXELIFY 373
#define CLOCK_IDC_FONT_PRESS_START 374
#define CLOCK_IDC_FONT_RUBIK_BUBBLES 376
#define CLOCK_IDC_FONT_RUBIK_BURNED 377
#define CLOCK_IDC_FONT_RUBIK_GLITCH 379
#define CLOCK_IDC_FONT_RUBIK_MARKER_HATCH 380
#define CLOCK_IDC_FONT_RUBIK_PUDDLES 381
#define CLOCK_IDC_FONT_RUBIK_VINYL 382
#define CLOCK_IDC_FONT_RUBIK_WET_PAINT 383
#define CLOCK_IDC_FONT_RUGE_BOOGIE 384
#define CLOCK_IDC_FONT_SEVILLANA 385
#define CLOCK_IDC_FONT_SILKSCREEN 386
#define CLOCK_IDC_FONT_STICK 387
#define CLOCK_IDC_FONT_UNDERDOG 388
#define CLOCK_IDC_FONT_WALLPOET 389

/*==================================
 * Font resource IDs (400-499)
 *==================================*/
/* Basic font resources */
#define IDR_FONT_RECMONO 442
#define IDR_FONT_DEPARTURE 420
#define IDR_FONT_TERMINESS 443
#define IDR_FONT_YESTERYEAR 490
#define IDR_FONT_ZCOOL_KUAILE 491
#define IDR_FONT_PROFONT 492
#define IDR_FONT_DADDYTIME 493
#define IDR_FONT_PINYON_SCRIPT 494

/* Extended font resources */
#define IDR_FONT_ARBUTUS 447
#define IDR_FONT_BERKSHIRE 448
#define IDR_FONT_CAVEAT 449
#define IDR_FONT_CREEPSTER 450
#define IDR_FONT_DOTGOTHIC 451
#define IDR_FONT_DOTO 452
#define IDR_FONT_FOLDIT 454
#define IDR_FONT_FREDERICKA 455
#define IDR_FONT_FRIJOLE 456
#define IDR_FONT_GWENDOLYN 458
#define IDR_FONT_HANDJET 459
#define IDR_FONT_INKNUT 460
#define IDR_FONT_JACQUARD 461
#define IDR_FONT_JACQUARDA 462
#define IDR_FONT_KAVOON 463
#define IDR_FONT_KUMAR_ONE_OUTLINE 464
#define IDR_FONT_KUMAR_ONE 465
#define IDR_FONT_LAKKI_REDDY 466
#define IDR_FONT_LICORICE 467
#define IDR_FONT_MA_SHAN_ZHENG 468
#define IDR_FONT_MOIRAI_ONE 469
#define IDR_FONT_MYSTERY_QUEST 470
#define IDR_FONT_NOTO_NASTALIQ 471
#define IDR_FONT_PIEDRA 472
#define IDR_FONT_PIXELIFY 473
#define IDR_FONT_PRESS_START 474
#define IDR_FONT_RUBIK_BUBBLES 476
#define IDR_FONT_RUBIK_BURNED 477
#define IDR_FONT_RUBIK_GLITCH 479
#define IDR_FONT_RUBIK_MARKER_HATCH 480
#define IDR_FONT_RUBIK_PUDDLES 481
#define IDR_FONT_RUBIK_VINYL 482
#define IDR_FONT_RUBIK_WET_PAINT 483
#define IDR_FONT_RUGE_BOOGIE 484
#define IDR_FONT_SEVILLANA 485
#define IDR_FONT_SILKSCREEN 486
#define IDR_FONT_STICK 487
#define IDR_FONT_UNDERDOG 488
#define IDR_FONT_WALLPOET 489

#endif /* CATIME_RESOURCE_H */
