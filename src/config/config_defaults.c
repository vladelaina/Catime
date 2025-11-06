/**
 * @file config_defaults.c
 * @brief Configuration defaults implementation
 * 
 * Centralized storage of default configuration values.
 */

#include "../include/config_defaults.h"
#include "../include/language.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winnls.h>

/* ============================================================================
 * Configuration metadata table
 * ============================================================================ */

static const ConfigItemMeta CONFIG_METADATA[] = {
    /* General settings */
    {INI_SECTION_GENERAL, "CONFIG_VERSION", CATIME_VERSION, CONFIG_TYPE_STRING, "Configuration version"},
    {INI_SECTION_GENERAL, "LANGUAGE", "English", CONFIG_TYPE_ENUM, "UI language"},
    {INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", "FALSE", CONFIG_TYPE_BOOL, "Desktop shortcut check completed"},
    {INI_SECTION_GENERAL, "FIRST_RUN", "TRUE", CONFIG_TYPE_BOOL, "First run flag"},
    {INI_SECTION_GENERAL, "FONT_LICENSE_ACCEPTED", "FALSE", CONFIG_TYPE_BOOL, "Font license accepted"},
    {INI_SECTION_GENERAL, "FONT_LICENSE_VERSION_ACCEPTED", "", CONFIG_TYPE_STRING, "Accepted license version"},
    
    /* Display settings */
    {INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", DEFAULT_TEXT_COLOR, CONFIG_TYPE_STRING, "Text color (hex)"},
    {INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", "20", CONFIG_TYPE_INT, "Base font size"},
    {INI_SECTION_DISPLAY, "FONT_FILE_NAME", FONTS_PATH_PREFIX DEFAULT_FONT_NAME, CONFIG_TYPE_STRING, "Font file path"},
    {INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", "960", CONFIG_TYPE_INT, "Window X position"},
    {INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", "-1", CONFIG_TYPE_INT, "Window Y position"},
    {INI_SECTION_DISPLAY, "WINDOW_SCALE", DEFAULT_WINDOW_SCALE, CONFIG_TYPE_STRING, "Window scale factor"},
    {INI_SECTION_DISPLAY, "WINDOW_TOPMOST", "TRUE", CONFIG_TYPE_BOOL, "Always on top"},
    
    /* Timer settings */
    {INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", "1500", CONFIG_TYPE_INT, "Default timer duration (seconds)"},
    {INI_SECTION_TIMER, "CLOCK_USE_24HOUR", "FALSE", CONFIG_TYPE_BOOL, "Use 24-hour format"},
    {INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", "FALSE", CONFIG_TYPE_BOOL, "Show seconds in clock mode"},
    {INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", "DEFAULT", CONFIG_TYPE_ENUM, "Time format style"},
    {INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", "FALSE", CONFIG_TYPE_BOOL, "Show centiseconds"},
    {INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", "1500,600,300", CONFIG_TYPE_STRING, "Quick countdown presets"},
    {INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", "0", CONFIG_TYPE_STRING, "Timeout text"},
    {INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE", CONFIG_TYPE_ENUM, "Timeout action type"},
    {INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", "", CONFIG_TYPE_STRING, "File to open on timeout"},
    {INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "", CONFIG_TYPE_STRING, "Website to open on timeout"},
    {INI_SECTION_TIMER, "STARTUP_MODE", "COUNTDOWN", CONFIG_TYPE_ENUM, "Startup mode"},
    
    /* Pomodoro settings */
    {INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "1500,300,1500,600", CONFIG_TYPE_STRING, "Pomodoro time intervals"},
    {INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", "1", CONFIG_TYPE_INT, "Cycles before long break"},
    
    /* Notification settings */
    {INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", DEFAULT_TIMEOUT_MESSAGE, CONFIG_TYPE_STRING, "Timeout message"},
    {INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", DEFAULT_POMODORO_MESSAGE, CONFIG_TYPE_STRING, "Pomodoro complete message"},
    {INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", DEFAULT_POMODORO_COMPLETE_MSG, CONFIG_TYPE_STRING, "Cycle complete message"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", "3000", CONFIG_TYPE_INT, "Notification display duration"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", "95", CONFIG_TYPE_INT, "Notification opacity (1-100)"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", "CATIME", CONFIG_TYPE_ENUM, "Notification display type"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "", CONFIG_TYPE_STRING, "Notification sound file"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", "100", CONFIG_TYPE_INT, "Sound volume (0-100)"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", "FALSE", CONFIG_TYPE_BOOL, "Disable all notifications"},
    
    /* Animation settings */
    {"Animation", "ANIMATION_PATH", "__logo__", CONFIG_TYPE_STRING, "Tray icon animation path"},
    {"Animation", "ANIMATION_SPEED_METRIC", "MEMORY", CONFIG_TYPE_ENUM, "Animation speed metric (MEMORY/CPU/TIMER)"},
    {"Animation", "ANIMATION_SPEED_DEFAULT", "100", CONFIG_TYPE_INT, "Default animation speed percentage"},
    {"Animation", "ANIMATION_SPEED_MAP_10", "140", CONFIG_TYPE_STRING, "Speed at 10% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_20", "180", CONFIG_TYPE_STRING, "Speed at 20% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_30", "220", CONFIG_TYPE_STRING, "Speed at 30% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_40", "260", CONFIG_TYPE_STRING, "Speed at 40% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_50", "300", CONFIG_TYPE_STRING, "Speed at 50% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_60", "340", CONFIG_TYPE_STRING, "Speed at 60% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_70", "380", CONFIG_TYPE_STRING, "Speed at 70% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_80", "420", CONFIG_TYPE_STRING, "Speed at 80% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_90", "460", CONFIG_TYPE_STRING, "Speed at 90% metric"},
    {"Animation", "ANIMATION_SPEED_MAP_100", "500", CONFIG_TYPE_STRING, "Speed at 100% metric"},
    {"Animation", "PERCENT_ICON_TEXT_COLOR", DEFAULT_BLACK_COLOR, CONFIG_TYPE_STRING, "Percent icon text color"},
    {"Animation", "PERCENT_ICON_BG_COLOR", DEFAULT_WHITE_COLOR, CONFIG_TYPE_STRING, "Percent icon background color"},
    {"Animation", "ANIMATION_FOLDER_INTERVAL_MS", "150", CONFIG_TYPE_INT, "Folder animation interval"},
    {"Animation", "ANIMATION_MIN_INTERVAL_MS", "0", CONFIG_TYPE_INT, "Minimum animation interval"},
    
    /* Hotkeys */
    {INI_SECTION_HOTKEYS, "HOTKEY_SHOW_TIME", "None", CONFIG_TYPE_STRING, "Show current time hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_COUNT_UP", "None", CONFIG_TYPE_STRING, "Count up mode hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_COUNTDOWN", "None", CONFIG_TYPE_STRING, "Countdown mode hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN1", "None", CONFIG_TYPE_STRING, "Quick countdown 1 hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN2", "None", CONFIG_TYPE_STRING, "Quick countdown 2 hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN3", "None", CONFIG_TYPE_STRING, "Quick countdown 3 hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_POMODORO", "None", CONFIG_TYPE_STRING, "Pomodoro mode hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_TOGGLE_VISIBILITY", "None", CONFIG_TYPE_STRING, "Toggle visibility hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_EDIT_MODE", "None", CONFIG_TYPE_STRING, "Edit mode hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_PAUSE_RESUME", "None", CONFIG_TYPE_STRING, "Pause/resume hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_RESTART_TIMER", "None", CONFIG_TYPE_STRING, "Restart timer hotkey"},
    {INI_SECTION_HOTKEYS, "HOTKEY_CUSTOM_COUNTDOWN", "None", CONFIG_TYPE_STRING, "Custom countdown hotkey"},
    
    /* Colors */
    {INI_SECTION_COLORS, "COLOR_OPTIONS", DEFAULT_COLOR_OPTIONS_INI, CONFIG_TYPE_STRING, "Color palette"},
    
    /* Recent files (dynamically generated, not in metadata) */
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
    LANGID systemLangID = GetUserDefaultUILanguage();
    int defaultLanguage = APP_LANG_ENGLISH;
    
    switch (PRIMARYLANGID(systemLangID)) {
        case LANG_CHINESE:
            if (SUBLANGID(systemLangID) == SUBLANG_CHINESE_SIMPLIFIED) {
                defaultLanguage = APP_LANG_CHINESE_SIMP;
            } else {
                defaultLanguage = APP_LANG_CHINESE_TRAD;
            }
            break;
        case LANG_SPANISH:    defaultLanguage = APP_LANG_SPANISH;    break;
        case LANG_FRENCH:     defaultLanguage = APP_LANG_FRENCH;     break;
        case LANG_GERMAN:     defaultLanguage = APP_LANG_GERMAN;     break;
        case LANG_RUSSIAN:    defaultLanguage = APP_LANG_RUSSIAN;    break;
        case LANG_PORTUGUESE: defaultLanguage = APP_LANG_PORTUGUESE; break;
        case LANG_JAPANESE:   defaultLanguage = APP_LANG_JAPANESE;   break;
        case LANG_KOREAN:     defaultLanguage = APP_LANG_KOREAN;     break;
        case LANG_ENGLISH:
        default:
            defaultLanguage = APP_LANG_ENGLISH;
            break;
    }
    
    return defaultLanguage;
}

void WriteDefaultsToConfig(const char* config_path) {
    if (!config_path) return;
    
    /* Write all metadata-defined defaults */
    for (int i = 0; i < CONFIG_METADATA_COUNT; i++) {
        const ConfigItemMeta* item = &CONFIG_METADATA[i];
        
        switch (item->type) {
            case CONFIG_TYPE_INT:
                WriteIniInt(item->section, item->key, atoi(item->defaultValue), config_path);
                break;
                
            case CONFIG_TYPE_BOOL:
            case CONFIG_TYPE_STRING:
            case CONFIG_TYPE_ENUM:
            default:
                WriteIniString(item->section, item->key, item->defaultValue, config_path);
                break;
        }
    }
    
    /* Write recent files (not in metadata) */
    for (int i = 1; i <= MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        WriteIniString(INI_SECTION_RECENTFILES, key, "", config_path);
    }
}

void CreateDefaultConfig(const char* config_path) {
    if (!config_path) return;
    
    /* Detect system language and override default */
    int detectedLang = DetectSystemLanguage();
    
    /* Language enum to string mapping */
    const char* langNames[] = {
        "Chinese_Simplified",  /* APP_LANG_CHINESE_SIMP */
        "Chinese_Traditional", /* APP_LANG_CHINESE_TRAD */
        "English",             /* APP_LANG_ENGLISH */
        "Spanish",             /* APP_LANG_SPANISH */
        "French",              /* APP_LANG_FRENCH */
        "German",              /* APP_LANG_GERMAN */
        "Russian",             /* APP_LANG_RUSSIAN */
        "Portuguese",          /* APP_LANG_PORTUGUESE */
        "Japanese",            /* APP_LANG_JAPANESE */
        "Korean"               /* APP_LANG_KOREAN */
    };
    
    const char* detectedLangName = (detectedLang >= 0 && detectedLang < APP_LANG_COUNT) 
                                   ? langNames[detectedLang] 
                                   : "English";
    
    /* Write all defaults */
    WriteDefaultsToConfig(config_path);
    
    /* Override language with detected value */
    WriteIniString(INI_SECTION_GENERAL, "LANGUAGE", detectedLangName, config_path);
}

