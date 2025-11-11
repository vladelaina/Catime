/**
 * @file config_defaults.c
 * @brief Configuration defaults implementation
 * 
 * Centralized storage of default configuration values.
 */

#include "config/config_defaults.h"
#include "language.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
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
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", "3000", CONFIG_TYPE_INT, "Notification display duration"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", "95", CONFIG_TYPE_INT, "Notification opacity (1-100)"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", "CATIME", CONFIG_TYPE_ENUM, "Notification display type"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "", CONFIG_TYPE_STRING, "Notification sound file"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", "100", CONFIG_TYPE_INT, "Sound volume (0-100)"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", "FALSE", CONFIG_TYPE_BOOL, "Disable all notifications"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_X", "-1", CONFIG_TYPE_INT, "Notification window X position"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_Y", "-1", CONFIG_TYPE_INT, "Notification window Y position"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_WIDTH", "0", CONFIG_TYPE_INT, "Notification window width"},
    {INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_HEIGHT", "0", CONFIG_TYPE_INT, "Notification window height"},
    
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
        "Chinese_Simplified",
        "Chinese_Traditional",
        "English",
        "Spanish",
        "French",
        "German",
        "Russian",
        "Portuguese",
        "Japanese",
        "Korean"
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
    char value[512];
    struct ConfigEntry* next;
} ConfigEntry;

static void FreeConfigEntryList(ConfigEntry* head) {
    while (head) {
        ConfigEntry* next = head->next;
        free(head);
        head = next;
    }
}

static ConfigEntry* ReadAllConfigEntries(const char* config_path) {
    wchar_t wConfigPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wConfigPath, MAX_PATH);

    ConfigEntry* head = NULL;
    ConfigEntry* tail = NULL;

    /* Buffer for section names (32KB should be enough) */
    wchar_t* sectionNames = (wchar_t*)malloc(32768 * sizeof(wchar_t));
    if (!sectionNames) return NULL;

    /* Get all section names */
    DWORD sectionsLen = GetPrivateProfileSectionNamesW(sectionNames, 32768, wConfigPath);
    if (sectionsLen == 0) {
        free(sectionNames);
        return NULL;
    }

    /* Iterate through each section */
    wchar_t* currentSection = sectionNames;
    while (*currentSection) {
        /* Buffer for section content */
        wchar_t* sectionData = (wchar_t*)malloc(32768 * sizeof(wchar_t));
        if (!sectionData) {
            free(sectionNames);
            FreeConfigEntryList(head);
            return NULL;
        }

        /* Get all key=value pairs in this section */
        DWORD dataLen = GetPrivateProfileSectionW(currentSection, sectionData, 32768, wConfigPath);

        if (dataLen > 0) {
            /* Parse key=value pairs */
            wchar_t* currentPair = sectionData;
            while (*currentPair) {
                /* Find the '=' separator */
                wchar_t* equalSign = wcschr(currentPair, L'=');
                if (equalSign) {
                    /* Create new entry */
                    ConfigEntry* entry = (ConfigEntry*)calloc(1, sizeof(ConfigEntry));
                    if (!entry) {
                        free(sectionData);
                        free(sectionNames);
                        FreeConfigEntryList(head);
                        return NULL;
                    }

                    /* Extract section name */
                    WideCharToMultiByte(CP_UTF8, 0, currentSection, -1,
                                       entry->section, sizeof(entry->section), NULL, NULL);

                    /* Extract key (before '=') */
                    size_t keyLen = equalSign - currentPair;
                    if (keyLen >= sizeof(entry->key)) keyLen = sizeof(entry->key) - 1;
                    wchar_t keyBuf[64];
                    wcsncpy(keyBuf, currentPair, keyLen);
                    keyBuf[keyLen] = L'\0';
                    WideCharToMultiByte(CP_UTF8, 0, keyBuf, -1,
                                       entry->key, sizeof(entry->key), NULL, NULL);

                    /* Extract value (after '=') */
                    WideCharToMultiByte(CP_UTF8, 0, equalSign + 1, -1,
                                       entry->value, sizeof(entry->value), NULL, NULL);

                    /* Add to linked list */
                    if (!head) {
                        head = tail = entry;
                    } else {
                        tail->next = entry;
                        tail = entry;
                    }
                }

                /* Move to next key=value pair */
                currentPair += wcslen(currentPair) + 1;
            }
        }

        free(sectionData);

        /* Move to next section */
        currentSection += wcslen(currentSection) + 1;
    }

    free(sectionNames);
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

    /* Step 2: Fix legacy color swap bug in PERCENT_ICON colors */
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

    /* Swap colors if they match the old buggy default (white text, black bg) */
    if (textColorEntry && bgColorEntry &&
        (strcasecmp(textColorEntry->value, "#FFFFFF") == 0 || strcasecmp(textColorEntry->value, "#ffffff") == 0) &&
        (strcasecmp(bgColorEntry->value, "#000000") == 0 || strcasecmp(bgColorEntry->value, "#000") == 0)) {
        /* Swap: TEXT=#FFFFFF -> #000000, BG=#000000 -> #FFFFFF */
        strncpy(textColorEntry->value, "#000000", sizeof(textColorEntry->value) - 1);
        strncpy(bgColorEntry->value, "#FFFFFF", sizeof(bgColorEntry->value) - 1);
    }

    /* Step 3: Delete old config file to remove deprecated items */
    wchar_t wConfigPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wConfigPath, MAX_PATH);
    DeleteFileW(wConfigPath);

    /* Step 4: Create fresh default config */
    CreateDefaultConfig(config_path);

    /* Step 5: Restore ALL user values (except CONFIG_VERSION) */
    current = oldConfig;
    while (current) {
        /* Skip CONFIG_VERSION - must be updated to current version */
        if (strcmp(current->key, "CONFIG_VERSION") != 0) {
            /* Restore value (WriteIniString handles all types) */
            WriteIniString(current->section, current->key, current->value, config_path);
        }
        current = current->next;
    }

    /* Clean up */
    FreeConfigEntryList(oldConfig);
}

