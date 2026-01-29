/**
 * @file config_defaults.c
 * @brief Configuration defaults implementation
 *
 * Centralized storage of default configuration values.
 *
 * Opacity Adjustment Configuration:
 * - OPACITY_STEP_NORMAL: Step size for normal mouse wheel scroll (default: 1)
 * - OPACITY_STEP_FAST: Step size when Ctrl key is held (default: 5)
 * - Both values are in percentage units (1-100)
 * - Supports hot-reload: changes take effect immediately without restart
 */

#include "config/config_defaults.h"
#include "config/config_loader.h"
#include "language.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <windows.h>
#include <winnls.h>

/* Helper macros for metadata table */
#define CFG_OFFSET(field) offsetof(ConfigSnapshot, field)
#define CFG_SIZE(field)   sizeof(((ConfigSnapshot*)0)->field)
#define CFG_NO_OFFSET     SIZE_MAX
#define CFG_NO_SIZE       0

/* ============================================================================
 * Configuration metadata table
 * ============================================================================ */

static const ConfigItemMeta CONFIG_METADATA[] = {
    /* General settings */
    /* CONFIG_VERSION and FIRST_RUN are not mapped to ConfigSnapshot - handled separately */
    {INI_SECTION_GENERAL, "CONFIG_VERSION", CATIME_VERSION, CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Configuration version"},
    {INI_SECTION_GENERAL, "LANGUAGE", "English", CONFIG_TYPE_ENUM, SIZE_MAX, 0, "Language"},
    {INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", "FALSE", CONFIG_TYPE_BOOL, CFG_NO_OFFSET, CFG_NO_SIZE, "Desktop shortcut check completed"},
    {INI_SECTION_GENERAL, "FIRST_RUN", "TRUE", CONFIG_TYPE_BOOL, CFG_NO_OFFSET, CFG_NO_SIZE, "First run flag"},
    {INI_SECTION_GENERAL, "FONT_LICENSE_ACCEPTED", "FALSE", CONFIG_TYPE_BOOL, CFG_OFFSET(fontLicenseAccepted), CFG_NO_SIZE, "Font license accepted"},
    {INI_SECTION_GENERAL, "FONT_LICENSE_VERSION_ACCEPTED", "", CONFIG_TYPE_STRING, CFG_OFFSET(fontLicenseVersion), CFG_SIZE(fontLicenseVersion), "Accepted license version"},
    
    /* Display settings */
    {INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", DEFAULT_TEXT_COLOR, CONFIG_TYPE_STRING, CFG_OFFSET(textColor), CFG_SIZE(textColor), "Text color (hex)"},
    {INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", "20", CONFIG_TYPE_INT, CFG_OFFSET(baseFontSize), CFG_NO_SIZE, "Base font size"},
    {INI_SECTION_DISPLAY, "FONT_FILE_NAME", FONTS_PATH_PREFIX DEFAULT_FONT_NAME, CONFIG_TYPE_CUSTOM, CFG_OFFSET(fontFileName), CFG_SIZE(fontFileName), "Font file path"},
    {INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", "-2", CONFIG_TYPE_INT, CFG_OFFSET(windowPosX), CFG_NO_SIZE, "Window X position (-2 = Auto/Golden Ratio, -1 = Center)"},
    {INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", "-1", CONFIG_TYPE_INT, CFG_OFFSET(windowPosY), CFG_NO_SIZE, "Window Y position"},
    {INI_SECTION_DISPLAY, "WINDOW_SCALE", DEFAULT_WINDOW_SCALE, CONFIG_TYPE_FLOAT, CFG_OFFSET(windowScale), CFG_NO_SIZE, "Window scale factor"},
    {INI_SECTION_DISPLAY, "PLUGIN_SCALE", DEFAULT_PLUGIN_SCALE, CONFIG_TYPE_FLOAT, CFG_OFFSET(pluginScale), CFG_NO_SIZE, "Plugin mode scale factor"},
    {INI_SECTION_DISPLAY, "WINDOW_TOPMOST", "TRUE", CONFIG_TYPE_BOOL, CFG_OFFSET(windowTopmost), CFG_NO_SIZE, "Always on top"},
    {INI_SECTION_DISPLAY, "WINDOW_OPACITY", "100", CONFIG_TYPE_INT, CFG_OFFSET(windowOpacity), CFG_NO_SIZE, "Window opacity (0-100)"},
    {INI_SECTION_DISPLAY, "MOVE_STEP_SMALL", "10", CONFIG_TYPE_INT, CFG_OFFSET(moveStepSmall), CFG_NO_SIZE, "Arrow key move step (1-500 pixels)"},
    {INI_SECTION_DISPLAY, "MOVE_STEP_LARGE", "50", CONFIG_TYPE_INT, CFG_OFFSET(moveStepLarge), CFG_NO_SIZE, "Ctrl+arrow key move step (1-500 pixels)"},
    {INI_SECTION_DISPLAY, "OPACITY_STEP_NORMAL", "1", CONFIG_TYPE_INT, CFG_OFFSET(opacityStepNormal), CFG_NO_SIZE, "Opacity scroll step (1-100)"},
    {INI_SECTION_DISPLAY, "OPACITY_STEP_FAST", "5", CONFIG_TYPE_INT, CFG_OFFSET(opacityStepFast), CFG_NO_SIZE, "Opacity Ctrl+scroll step (1-100)"},
    {INI_SECTION_DISPLAY, "SCALE_STEP_NORMAL", "10", CONFIG_TYPE_INT, CFG_OFFSET(scaleStepNormal), CFG_NO_SIZE, "Scale scroll step (1-100)"},
    {INI_SECTION_DISPLAY, "SCALE_STEP_FAST", "15", CONFIG_TYPE_INT, CFG_OFFSET(scaleStepFast), CFG_NO_SIZE, "Scale Ctrl+scroll step (1-100)"},
    {INI_SECTION_DISPLAY, "TEXT_EFFECT", "NONE", CONFIG_TYPE_ENUM, CFG_OFFSET(textEffect), CFG_NO_SIZE, "Text effect style (NONE/GLOW/GLASS/NEON/HOLOGRAPHIC/LIQUID)"},

    /* Timer settings */
    {INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", "1500", CONFIG_TYPE_INT, CFG_OFFSET(defaultStartTime), CFG_NO_SIZE, "Default timer duration (seconds)"},
    {INI_SECTION_TIMER, "CLOCK_USE_24HOUR", "TRUE", CONFIG_TYPE_BOOL, CFG_OFFSET(use24Hour), CFG_NO_SIZE, "Use 24-hour format"},
    {INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", "FALSE", CONFIG_TYPE_BOOL, CFG_OFFSET(showSeconds), CFG_NO_SIZE, "Show seconds in clock mode"},
    {INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", "DEFAULT", CONFIG_TYPE_ENUM, CFG_OFFSET(timeFormat), CFG_NO_SIZE, "Time format style"},
    {INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", "FALSE", CONFIG_TYPE_BOOL, CFG_OFFSET(showMilliseconds), CFG_NO_SIZE, "Show centiseconds"},
    {INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", "1500,600,300", CONFIG_TYPE_CUSTOM, CFG_NO_OFFSET, CFG_NO_SIZE, "Quick countdown presets"},
    {INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", "0", CONFIG_TYPE_STRING, CFG_OFFSET(timeoutText), CFG_SIZE(timeoutText), "Timeout text"},
    {INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE", CONFIG_TYPE_ENUM, CFG_OFFSET(timeoutAction), CFG_NO_SIZE, "Timeout action type"},
    {INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", "", CONFIG_TYPE_STRING, CFG_OFFSET(timeoutFilePath), CFG_SIZE(timeoutFilePath), "File to open on timeout"},
    {INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "", CONFIG_TYPE_CUSTOM, CFG_NO_OFFSET, CFG_NO_SIZE, "Website to open on timeout"},
    {INI_SECTION_TIMER, "STARTUP_MODE", "SHOW_TIME", CONFIG_TYPE_STRING, CFG_OFFSET(startupMode), CFG_SIZE(startupMode), "Startup mode"},
    
    /* Pomodoro settings */
    {INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "1500,300,1500,600", CONFIG_TYPE_CUSTOM, CFG_NO_OFFSET, CFG_NO_SIZE, "Pomodoro time intervals"},
    {INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", "1", CONFIG_TYPE_INT, CFG_OFFSET(pomodoroLoopCount), CFG_NO_SIZE, "Cycles before long break"},
    
    /* Notification settings */
    {INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", DEFAULT_TIMEOUT_MESSAGE, CONFIG_TYPE_STRING, CFG_OFFSET(timeoutMessage), CFG_SIZE(timeoutMessage), "Timeout message"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", "3000", CONFIG_TYPE_INT, CFG_OFFSET(notificationTimeoutMs), CFG_NO_SIZE, "Notification display duration"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", "95", CONFIG_TYPE_INT, CFG_OFFSET(notificationMaxOpacity), CFG_NO_SIZE, "Notification opacity (1-100)"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", "CATIME", CONFIG_TYPE_ENUM, CFG_OFFSET(notificationType), CFG_NO_SIZE, "Notification display type"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "", CONFIG_TYPE_STRING, CFG_OFFSET(notificationSoundFile), CFG_SIZE(notificationSoundFile), "Notification sound file"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", "100", CONFIG_TYPE_INT, CFG_OFFSET(notificationSoundVolume), CFG_NO_SIZE, "Sound volume (0-100)"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", "FALSE", CONFIG_TYPE_BOOL, CFG_OFFSET(notificationDisabled), CFG_NO_SIZE, "Disable all notifications"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_X", "-1", CONFIG_TYPE_INT, CFG_OFFSET(notificationWindowX), CFG_NO_SIZE, "Notification window X position"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_Y", "-1", CONFIG_TYPE_INT, CFG_OFFSET(notificationWindowY), CFG_NO_SIZE, "Notification window Y position"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_WIDTH", "0", CONFIG_TYPE_INT, CFG_OFFSET(notificationWindowWidth), CFG_NO_SIZE, "Notification window width"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_HEIGHT", "0", CONFIG_TYPE_INT, CFG_OFFSET(notificationWindowHeight), CFG_NO_SIZE, "Notification window height"},
    
    /* Animation settings - not mapped to ConfigSnapshot, handled by tray_animation_core */
    {"Animation", "ANIMATION_PATH", "__logo__", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Tray icon animation path"},
    {"Animation", "ANIMATION_SPEED_METRIC", "MEMORY", CONFIG_TYPE_ENUM, CFG_NO_OFFSET, CFG_NO_SIZE, "Animation speed metric (MEMORY/CPU/TIMER)"},
    {"Animation", "ANIMATION_SPEED_DEFAULT", "100", CONFIG_TYPE_INT, CFG_NO_OFFSET, CFG_NO_SIZE, "Default animation speed percentage"},
    {"Animation", "ANIMATION_SPEED_MAP_10", "140", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Speed at 10% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_20", "180", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Speed at 20% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_30", "220", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Speed at 30% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_40", "260", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Speed at 40% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_50", "300", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Speed at 50% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_60", "340", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Speed at 60% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_70", "380", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Speed at 70% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_80", "420", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Speed at 80% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_90", "460", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Speed at 90% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_100", "500", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Speed at 100% metric"},
    {"Animation", "PERCENT_ICON_TEXT_COLOR", "auto", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Percent icon text color (auto = theme-based, or hex color like #000000)"},
    {"Animation", "PERCENT_ICON_BG_COLOR", "transparent", CONFIG_TYPE_STRING, CFG_NO_OFFSET, CFG_NO_SIZE, "Percent icon background color (transparent = no background, or hex color like #FFFFFF)"},
    {"Animation", "ANIMATION_FOLDER_INTERVAL_MS", "150", CONFIG_TYPE_INT, CFG_NO_OFFSET, CFG_NO_SIZE, "Folder animation interval"},
    {"Animation", "ANIMATION_MIN_INTERVAL_MS", "0", CONFIG_TYPE_INT, CFG_NO_OFFSET, CFG_NO_SIZE, "Minimum animation interval"},
    
    /* Hotkeys */
    {INI_SECTION_HOTKEYS, "HOTKEY_SHOW_TIME", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyShowTime), CFG_NO_SIZE, "Show current time hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_COUNT_UP", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyCountUp), CFG_NO_SIZE, "Count up mode hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_COUNTDOWN", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyCountdown), CFG_NO_SIZE, "Countdown mode hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN1", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyQuickCountdown1), CFG_NO_SIZE, "Quick countdown 1 hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN2", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyQuickCountdown2), CFG_NO_SIZE, "Quick countdown 2 hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN3", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyQuickCountdown3), CFG_NO_SIZE, "Quick countdown 3 hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_POMODORO", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyPomodoro), CFG_NO_SIZE, "Pomodoro mode hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_TOGGLE_VISIBILITY", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyToggleVisibility), CFG_NO_SIZE, "Toggle visibility hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_EDIT_MODE", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyEditMode), CFG_NO_SIZE, "Edit mode hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_PAUSE_RESUME", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyPauseResume), CFG_NO_SIZE, "Pause/resume hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_RESTART_TIMER", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyRestartTimer), CFG_NO_SIZE, "Restart timer hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_CUSTOM_COUNTDOWN", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyCustomCountdown), CFG_NO_SIZE, "Custom countdown hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_TOGGLE_MILLISECONDS", "None", CONFIG_TYPE_HOTKEY, CFG_OFFSET(hotkeyToggleMilliseconds), CFG_NO_SIZE, "Toggle milliseconds display hotkey"},
    
    /* Colors */
    {INI_SECTION_COLORS, "COLOR_OPTIONS", DEFAULT_COLOR_OPTIONS_INI, CONFIG_TYPE_STRING, CFG_OFFSET(colorOptions), CFG_SIZE(colorOptions), "Color palette"},
    
    /* Recent files - handled separately via custom logic */
};

static const int CONFIG_METADATA_COUNT = sizeof(CONFIG_METADATA) / sizeof(CONFIG_METADATA[0]);

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

const ConfigItemMeta* GetConfigMetadata(int* count) {
    if (count) {
        *count = CONFIG_METADATA_COUNT;
    }
    return CONFIG_METADATA;
}

const char* GetDefaultValue(const char* section, const char* key) {
    if (!section || !key) return NULL;
    
    for (int i = 0; i < CONFIG_METADATA_COUNT; i++) {
        if (strcmp(CONFIG_METADATA[i].section, section) == 0 &&
            strcmp(CONFIG_METADATA[i].key, key) == 0) {
            return CONFIG_METADATA[i].defaultValue;
        }
    }
    
    return NULL;
}

int DetectSystemLanguage(void) {
    return (int)GetSystemDefaultLanguage();
}

void WriteDefaultsToConfig(const char* config_path) {
    if (!config_path) return;

    /* Convert path to wide char for _wfopen */
    wchar_t wconfig_path[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);

    /* Open file for writing in UTF-8 mode (no BOM needed) */
    FILE* f = _wfopen(wconfig_path, L"wb");
    if (!f) return;

    /* Track current section to insert help docs */
    const char* lastSection = "";

    /* Write all metadata-defined defaults */
    for (int i = 0; i < CONFIG_METADATA_COUNT; i++) {
        const ConfigItemMeta* item = &CONFIG_METADATA[i];

        /* Write section header if section changed */
        if (strcmp(item->section, lastSection) != 0) {
            fprintf(f, "[%s]\n", item->section);
            lastSection = item->section;
        }

        /* Write key=value */
        switch (item->type) {
            case CONFIG_TYPE_INT:
                fprintf(f, "%s=%s\n", item->key, item->defaultValue);
                break;

            case CONFIG_TYPE_BOOL:
            case CONFIG_TYPE_STRING:
            case CONFIG_TYPE_ENUM:
            default:
                fprintf(f, "%s=%s\n", item->key, item->defaultValue);
                break;
        }

        /* Check if we just finished writing Display section */
        BOOL isLastDisplayItem = (i + 1 >= CONFIG_METADATA_COUNT ||
                                   strcmp(CONFIG_METADATA[i + 1].section, INI_SECTION_DISPLAY) != 0);
        if (strcmp(item->section, INI_SECTION_DISPLAY) == 0 && isLastDisplayItem) {
            fputs(";========================================================\n", f);
            fputs("; Display section help (hot reload supported)\n", f);
            fputs(";========================================================\n", f);
            fputs("; MOVE_STEP_SMALL: arrow key move step in edit mode (unit: pixels).\n", f);
            fputs(";   Controls how far the window moves with each arrow key press.\n", f);
            fputs(";   Range: 1-500 pixels\n", f);
            fputs(";   Default: 10 pixels\n", f);
            fputs(";\n", f);
            fputs("; MOVE_STEP_LARGE: Ctrl+arrow key move step in edit mode (unit: pixels).\n", f);
            fputs(";   Controls how far the window moves with Ctrl+arrow key.\n", f);
            fputs(";   Range: 1-500 pixels\n", f);
            fputs(";   Default: 50 pixels\n", f);
            fputs(";\n", f);
            fputs("; OPACITY_STEP_NORMAL: mouse wheel opacity adjustment step (unit: percent).\n", f);
            fputs(";   Controls opacity change when scrolling over tray icon.\n", f);
            fputs(";   Range: 1-100%\n", f);
            fputs(";   Default: 1%\n", f);
            fputs(";\n", f);
            fputs("; OPACITY_STEP_FAST: Ctrl+mouse wheel opacity adjustment step (unit: percent).\n", f);
            fputs(";   Controls opacity change when scrolling with Ctrl held.\n", f);
            fputs(";   Range: 1-100%\n", f);
            fputs(";   Default: 5%\n", f);
            fputs(";\n", f);
            fputs("; SCALE_STEP_NORMAL: mouse wheel scale adjustment step (unit: percent).\n", f);
            fputs(";   Controls window scale change when scrolling over window.\n", f);
            fputs(";   Range: 1-100%\n", f);
            fputs(";   Default: 10%\n", f);
            fputs(";\n", f);
            fputs("; SCALE_STEP_FAST: Ctrl+mouse wheel scale adjustment step (unit: percent).\n", f);
            fputs(";   Controls window scale change when scrolling with Ctrl held.\n", f);
            fputs(";   Range: 1-100%\n", f);
            fputs(";   Default: 15%\n", f);
            fputs(";========================================================\n", f);
        }

        /* Check if we just finished writing Animation section */
        BOOL isLastAnimationItem = (i + 1 >= CONFIG_METADATA_COUNT ||
                                     strcmp(CONFIG_METADATA[i + 1].section, "Animation") != 0);
        if (strcmp(item->section, "Animation") == 0 && isLastAnimationItem) {
            fputs(";========================================================\n", f);
            fputs("; Animation options help (hot reload supported)\n", f);
            fputs(";========================================================\n", f);
            fputs("; ANIMATION_SPEED_DEFAULT: base speed scale at 0% (unit: percent).\n", f);
            fputs(";   100 = 1x speed, 200 = 2x, 50 = 0.5x.\n", f);
            fputs(";   Works with ANIMATION_SPEED_MAP_* breakpoints via linear interpolation.\n", f);
            fputs(";\n", f);
            fputs("; PERCENT_ICON_TEXT_COLOR: CPU/MEM percent tray icon text color.\n", f);
            fputs(";   Format: auto (theme-aware), #RRGGBB, or R,G,B (0-255).\n", f);
            fputs(";   'auto' = automatic text color based on Windows theme (Win10 1607+)\n", f);
            fputs(";            On older systems or Win7, 'auto' defaults to black.\n", f);
            fputs(";   Example: auto, #000000 (black), #FFFFFF (white), 255,0,0 (red)\n", f);
            fputs(";\n", f);
            fputs("; PERCENT_ICON_BG_COLOR: CPU/MEM percent tray icon background color.\n", f);
            fputs(";   Format: transparent, #RRGGBB, or R,G,B (0-255).\n", f);
            fputs(";   'transparent' = no background, blends with taskbar\n", f);
            fputs(";   Example: transparent, #FFFFFF (white bg), #000000 (black bg)\n", f);
            fputs(";\n", f);
            fputs("; ANIMATION_FOLDER_INTERVAL_MS: base animation playback speed (unit: milliseconds).\n", f);
            fputs(";   Controls how fast the animation plays (higher = slower, lower = faster).\n", f);
            fputs(";   Affects folder sequences and static images (.ico/.png/.bmp/.jpg/.jpeg/.tif/.tiff).\n", f);
            fputs(";   Does NOT affect GIF/WebP (they honor embedded per-frame delays).\n", f);
            fputs(";   Default: 150ms (~6.7 fps)\n", f);
            fputs(";   Suggested range: 50-500ms\n", f);
            fputs(";\n", f);
            fputs("; ANIMATION_MIN_INTERVAL_MS: optional minimum speed limit (unit: milliseconds).\n", f);
            fputs(";   Adds an extra lower speed limit on top of system optimizations.\n", f);
            fputs(";   0     => use system default (recommended for most users)\n", f);
            fputs(";   N>0   => enforce minimum N ms per frame (e.g., 100 = max 10 fps)\n", f);
            fputs(";   Note: System already uses high-precision timing with fixed 50ms tray updates\n", f);
            fputs(";         to eliminate flicker/stutter. This setting is optional.\n", f);
            fputs(";   Use case: Set to 100+ on very low-end devices to reduce CPU usage.\n", f);
            fputs(";========================================================\n", f);
        }

        /* Check if we just finished writing Hotkeys section */
        BOOL isLastHotkeyItem = (i + 1 >= CONFIG_METADATA_COUNT ||
                                 strcmp(CONFIG_METADATA[i + 1].section, INI_SECTION_HOTKEYS) != 0);
        if (strcmp(item->section, INI_SECTION_HOTKEYS) == 0 && isLastHotkeyItem) {
            fputs(";========================================================\n", f);
            fputs("; Hotkeys section help (hot reload supported)\n", f);
            fputs(";========================================================\n", f);
            fputs("; Format: KEY=Ctrl+Shift+Alt+Key  or  KEY=None  or  KEY=0xNN (hex VK)\n", f);
            fputs(";  - Modifiers: Ctrl, Shift, Alt (combine with '+')\n", f);
            fputs(";  - Keys: A-Z, 0-9, F1..F24, Backspace, Tab, Enter, Esc, Space,\n", f);
            fputs(";           PageUp, PageDown, End, Home, Left, Up, Right, Down, Insert, Delete,\n", f);
            fputs(";           Num0..Num9, Num*, Num+, Num-, Num., Num/\n", f);
            fputs(";  - Examples: Ctrl+Shift+K  |  Alt+F12  |  None  |  0x5B\n", f);
            fputs(";  - Note: Some combinations may be reserved by the system or other apps.\n", f);
            fputs(";\n", f);
            fputs("; Keys in [Hotkeys]:\n", f);
            fputs(";   HOTKEY_SHOW_TIME           - Toggle show current time\n", f);
            fputs(";   HOTKEY_COUNT_UP            - Start count-up timer\n", f);
            fputs(";   HOTKEY_COUNTDOWN           - Start countdown timer\n", f);
            fputs(";   HOTKEY_QUICK_COUNTDOWN1    - Quick countdown slot 1\n", f);
            fputs(";   HOTKEY_QUICK_COUNTDOWN2    - Quick countdown slot 2\n", f);
            fputs(";   HOTKEY_QUICK_COUNTDOWN3    - Quick countdown slot 3\n", f);
            fputs(";   HOTKEY_POMODORO            - Start Pomodoro\n", f);
            fputs(";   HOTKEY_TOGGLE_VISIBILITY   - Toggle window visibility\n", f);
            fputs(";   HOTKEY_EDIT_MODE           - Toggle edit mode\n", f);
            fputs(";   HOTKEY_PAUSE_RESUME        - Pause/Resume timer\n", f);
            fputs(";   HOTKEY_RESTART_TIMER       - Restart current timer\n", f);
            fputs(";   HOTKEY_CUSTOM_COUNTDOWN    - Custom countdown\n", f);
            fputs(";   HOTKEY_TOGGLE_MILLISECONDS - Toggle milliseconds display\n", f);
            fputs(";========================================================\n", f);
        }

        /* Check if we just finished writing Colors section */
        BOOL isLastColorItem = (i + 1 >= CONFIG_METADATA_COUNT ||
                                strcmp(CONFIG_METADATA[i + 1].section, INI_SECTION_COLORS) != 0);
        if (strcmp(item->section, INI_SECTION_COLORS) == 0 && isLastColorItem) {
            fputs(";========================================================\n", f);
            fputs("; Colors section help (hot reload supported)\n", f);
            fputs(";========================================================\n", f);
            fputs("; COLOR_OPTIONS: comma-separated quick color list used by dialogs/menus.\n", f);
            fputs(";   Token format: #RRGGBB or RRGGBB (6 hex digits).\n", f);
            fputs(";   Whitespace is allowed around commas; duplicates are ignored.\n", f);
            fputs(";   Example: COLOR_OPTIONS=#FFFFFF,#E3E3E5,#000000,#F6ABB7,#FB7FA4\n", f);
            fputs(";========================================================\n", f);
        }
    }

    /* Write RecentFiles section */
    fprintf(f, "[%s]\n", INI_SECTION_RECENTFILES);
    for (int i = 1; i <= MAX_RECENT_FILES; i++) {
        fprintf(f, "CLOCK_RECENT_FILE_%d=\n", i);
    }

    fclose(f);
}

void CreateDefaultConfig(const char* config_path) {
    if (!config_path) return;
    
    /* Detect system language and override default */
    int detectedLang = DetectSystemLanguage();
    
    /* Language enum to string mapping */
    const char* langNames[] = {
        "Chinese_Simplified",
        "Chinese_Traditional",
        "English",
        "Spanish",
        "French",
        "German",
        "Russian",
        "Portuguese",
        "Japanese",
        "Korean",
        "Italian"
    };
    
    const char* detectedLangName = (detectedLang >= 0 && detectedLang < APP_LANG_COUNT) 
                                   ? langNames[detectedLang] 
                                   : "English";
    
    /* Write all defaults */
    WriteDefaultsToConfig(config_path);

    /* Override language with detected value */
    WriteIniString(INI_SECTION_GENERAL, "LANGUAGE", detectedLangName, config_path);
}

typedef struct ConfigEntry {
    char section[64];
    char key[64];
    char value[2048];  /* Large enough for COLOR_OPTIONS */
    struct ConfigEntry* next;
} ConfigEntry;

static void FreeConfigEntryList(ConfigEntry* head) {
    while (head) {
        ConfigEntry* next = head->next;
        free(head);
        head = next;
    }
}

static BOOL IsConfigItemInMetadata(const char* section, const char* key) {
    if (!section || !key) return FALSE;

    for (int i = 0; i < CONFIG_METADATA_COUNT; i++) {
        if (strcmp(CONFIG_METADATA[i].section, section) == 0 &&
            strcmp(CONFIG_METADATA[i].key, key) == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

/* Helper to detect if a string is valid UTF-8 */
static BOOL IsUtf8String(const char* str) {
    const unsigned char* bytes = (const unsigned char*)str;
    while (*bytes) {
        if ((*bytes & 0x80) == 0) { /* ASCII */
            bytes++;
        } else if ((*bytes & 0xE0) == 0xC0) { /* 2-byte sequence */
            if ((bytes[1] & 0xC0) != 0x80) return FALSE;
            bytes += 2;
        } else if ((*bytes & 0xF0) == 0xE0) { /* 3-byte sequence */
            if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80) return FALSE;
            bytes += 3;
        } else if ((*bytes & 0xF8) == 0xF0) { /* 4-byte sequence */
            if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80 || (bytes[3] & 0xC0) != 0x80) return FALSE;
            bytes += 4;
        } else {
            return FALSE;
        }
    }
    return TRUE;
}

static ConfigEntry* ReadAllConfigEntries(const char* config_path) {
    /* Open file for reading (UTF-8) */
    wchar_t wConfigPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wConfigPath, MAX_PATH);

    FILE* f = _wfopen(wConfigPath, L"rb");
    if (!f) return NULL;

    ConfigEntry* head = NULL;
    ConfigEntry* tail = NULL;
    char currentSection[64] = "";
    char line[4096];

    /* Skip UTF-8 BOM if present */
    int c1 = fgetc(f);
    int c2 = fgetc(f);
    int c3 = fgetc(f);
    if (!(c1 == 0xEF && c2 == 0xBB && c3 == 0xBF)) {
        /* Not a BOM, rewind */
        if (c3 != EOF) ungetc(c3, f);
        if (c2 != EOF) ungetc(c2, f);
        if (c1 != EOF) ungetc(c1, f);
    }

    while (fgets(line, sizeof(line), f)) {
        /* Detect encoding: if not UTF-8, assume ANSI and convert */
        /* This handles migration from old ANSI config files to new UTF-8 ones */
        if (!IsUtf8String(line)) {
            wchar_t wLine[4096];
            char utf8Line[4096];
            
            /* ANSI -> Wide */
            int wLen = MultiByteToWideChar(CP_ACP, 0, line, -1, wLine, 4096);
            if (wLen > 0) {
                /* Wide -> UTF-8 */
                int uLen = WideCharToMultiByte(CP_UTF8, 0, wLine, -1, utf8Line, 4096, NULL, NULL);
                if (uLen > 0) {
                    strncpy(line, utf8Line, sizeof(line) - 1);
                    line[sizeof(line) - 1] = '\0';
                }
            }
        }

        /* Remove newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        /* Trim leading whitespace */
        char* trimmed = line;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;

        /* Skip empty lines and comments */
        if (*trimmed == '\0' || *trimmed == ';' || *trimmed == '#') {
            continue;
        }

        /* Section header */
        if (*trimmed == '[') {
            char* end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(currentSection, trimmed + 1, sizeof(currentSection) - 1);
                currentSection[sizeof(currentSection) - 1] = '\0';
            }
            continue;
        }

        /* Key=Value */
        char* eq = strchr(trimmed, '=');
        if (eq && currentSection[0]) {
            *eq = '\0';

            /* Create new entry */
            ConfigEntry* entry = (ConfigEntry*)calloc(1, sizeof(ConfigEntry));
            if (!entry) {
                fclose(f);
                FreeConfigEntryList(head);
                return NULL;
            }

            /* Copy section, key, value */
            strncpy(entry->section, currentSection, sizeof(entry->section) - 1);

            /* Trim key */
            char* key = trimmed;
            char* keyEnd = eq - 1;
            while (keyEnd > key && isspace((unsigned char)*keyEnd)) keyEnd--;
            size_t keyLen = keyEnd - key + 1;
            if (keyLen >= sizeof(entry->key)) keyLen = sizeof(entry->key) - 1;
            strncpy(entry->key, key, keyLen);
            entry->key[keyLen] = '\0';

            /* Trim value */
            char* value = eq + 1;
            while (*value && isspace((unsigned char)*value)) value++;
            strncpy(entry->value, value, sizeof(entry->value) - 1);

            /* Add to linked list */
            if (!head) {
                head = tail = entry;
            } else {
                tail->next = entry;
                tail = entry;
            }
        }
    }

    fclose(f);
    return head;
}

void MigrateConfig(const char* config_path) {
    if (!config_path) return;

    /* Step 1: Read ALL config entries from old file (automatic discovery) */
    ConfigEntry* oldConfig = ReadAllConfigEntries(config_path);
    if (!oldConfig) {
        /* If reading fails, just create default config */
        CreateDefaultConfig(config_path);
        return;
    }

    /* Step 2: Detect and convert legacy default PERCENT_ICON colors */
    ConfigEntry* textColorEntry = NULL;
    ConfigEntry* bgColorEntry = NULL;
    ConfigEntry* current = oldConfig;
    while (current) {
        if (strcmp(current->section, "Animation") == 0) {
            if (strcmp(current->key, "PERCENT_ICON_TEXT_COLOR") == 0) {
                textColorEntry = current;
            } else if (strcmp(current->key, "PERCENT_ICON_BG_COLOR") == 0) {
                bgColorEntry = current;
            }
        }
        current = current->next;
    }

    /* Convert old hardcoded defaults to new "auto"/"transparent" keywords */
    BOOL isOldHardcodedDefault = FALSE;
    if (textColorEntry && bgColorEntry) {
        /* Old buggy default: white text (#FFFFFF), black bg (#000000) */
        BOOL isOldBuggyDefault =
            (strcasecmp(textColorEntry->value, "#FFFFFF") == 0 || strcasecmp(textColorEntry->value, "#ffffff") == 0) &&
            (strcasecmp(bgColorEntry->value, "#000000") == 0 || strcasecmp(bgColorEntry->value, "#000") == 0);

        /* Old fixed default: black text (#000000), white bg (#FFFFFF) */
        BOOL isOldFixedDefault =
            (strcasecmp(textColorEntry->value, "#000000") == 0 || strcasecmp(textColorEntry->value, "#000") == 0) &&
            (strcasecmp(bgColorEntry->value, "#FFFFFF") == 0 || strcasecmp(bgColorEntry->value, "#ffffff") == 0);

        isOldHardcodedDefault = isOldBuggyDefault || isOldFixedDefault;

        /* Convert to new defaults */
        if (isOldHardcodedDefault) {
            strncpy(textColorEntry->value, "auto", sizeof(textColorEntry->value) - 1);
            strncpy(bgColorEntry->value, "transparent", sizeof(bgColorEntry->value) - 1);
        }
    }

    /* Step 3: Delete old config file to remove deprecated items */
    wchar_t wConfigPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wConfigPath, MAX_PATH);
    DeleteFileW(wConfigPath);

    /* Step 4: Create fresh default config */
    CreateDefaultConfig(config_path);

    /* Step 5: Restore user values that exist in CONFIG_METADATA */
    current = oldConfig;
    while (current) {
        /* Skip CONFIG_VERSION - must be updated to current version */
        if (strcmp(current->key, "CONFIG_VERSION") == 0) {
            current = current->next;
            continue;
        }

        /* Only restore if config item exists in current metadata (filters deprecated items) */
        if (IsConfigItemInMetadata(current->section, current->key)) {
            WriteIniString(current->section, current->key, current->value, config_path);
        }

        current = current->next;
    }

    /* Clean up */
    FreeConfigEntryList(oldConfig);
}

