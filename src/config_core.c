/**
 * @file config_core.c
 * @brief Core configuration management - ReadConfig, WriteConfig, CreateDefaultConfig
 * 
 * Central configuration loading and saving logic for the application.
 */
#include "../include/config.h"
#include "../include/language.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <windows.h>
#include <winnls.h>

#define MAX_POMODORO_TIMES 10

#define UTF8_TO_WIDE(utf8, wide) \
    wchar_t wide[MAX_PATH] = {0}; \
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, MAX_PATH)

#define FOPEN_UTF8(utf8Path, mode, filePtr) \
    wchar_t _w##filePtr[MAX_PATH] = {0}; \
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, _w##filePtr, MAX_PATH); \
    FILE* filePtr = _wfopen(_w##filePtr, mode)

static inline BOOL FileExistsUtf8(const char* utf8Path) {
    if (!utf8Path) return FALSE;
    UTF8_TO_WIDE(utf8Path, wPath);
    return GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES;
}

/** Note: All external variables are declared in their respective headers */

/** Forward declarations for variables defined in timer.c */
extern int POMODORO_TIMES[10];
extern int POMODORO_TIMES_COUNT;

/** Global configuration variables - definitions */
char CLOCK_TIMEOUT_MESSAGE_TEXT[100] = DEFAULT_TIMEOUT_MESSAGE;
char POMODORO_TIMEOUT_MESSAGE_TEXT[100] = DEFAULT_POMODORO_MESSAGE;
char POMODORO_CYCLE_COMPLETE_TEXT[100] = DEFAULT_POMODORO_COMPLETE_MSG;

int NOTIFICATION_TIMEOUT_MS = 3000;
int NOTIFICATION_MAX_OPACITY = 95;
NotificationType NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
BOOL NOTIFICATION_DISABLED = FALSE;

char NOTIFICATION_SOUND_FILE[MAX_PATH] = "";
int NOTIFICATION_SOUND_VOLUME = 100;

BOOL FONT_LICENSE_ACCEPTED = FALSE;
char FONT_LICENSE_VERSION_ACCEPTED[16] = "";

TimeFormatType CLOCK_TIME_FORMAT = TIME_FORMAT_DEFAULT;
BOOL IS_TIME_FORMAT_PREVIEWING = FALSE;
TimeFormatType PREVIEW_TIME_FORMAT = TIME_FORMAT_DEFAULT;
BOOL CLOCK_SHOW_MILLISECONDS = FALSE;
BOOL IS_MILLISECONDS_PREVIEWING = FALSE;
BOOL PREVIEW_SHOW_MILLISECONDS = FALSE;

/** Enum-string mappings */
typedef struct {
    int value;
    const char* str;
} EnumStrMap;

static const EnumStrMap NOTIFICATION_TYPE_MAP[] = {
    {NOTIFICATION_TYPE_CATIME,       "CATIME"},
    {NOTIFICATION_TYPE_SYSTEM_MODAL, "SYSTEM_MODAL"},
    {NOTIFICATION_TYPE_OS,           "OS"},
    {-1, NULL}
};

static const EnumStrMap TIME_FORMAT_MAP[] = {
    {TIME_FORMAT_DEFAULT,      "DEFAULT"},
    {TIME_FORMAT_ZERO_PADDED,  "ZERO_PADDED"},
    {TIME_FORMAT_FULL_PADDED,  "FULL_PADDED"},
    {-1, NULL}
};

static const EnumStrMap LANGUAGE_MAP[] = {
    {APP_LANG_CHINESE_SIMP, "Chinese_Simplified"},
    {APP_LANG_CHINESE_TRAD, "Chinese_Traditional"},
    {APP_LANG_ENGLISH,      "English"},
    {APP_LANG_SPANISH,      "Spanish"},
    {APP_LANG_FRENCH,       "French"},
    {APP_LANG_GERMAN,       "German"},
    {APP_LANG_RUSSIAN,      "Russian"},
    {APP_LANG_PORTUGUESE,   "Portuguese"},
    {APP_LANG_JAPANESE,     "Japanese"},
    {APP_LANG_KOREAN,       "Korean"},
    {-1, NULL}
};

static const EnumStrMap TIMEOUT_ACTION_MAP[] = {
    {TIMEOUT_ACTION_MESSAGE,       "MESSAGE"},
    {TIMEOUT_ACTION_LOCK,          "LOCK"},
    {TIMEOUT_ACTION_SHUTDOWN,      "SHUTDOWN"},
    {TIMEOUT_ACTION_RESTART,       "RESTART"},
    {TIMEOUT_ACTION_OPEN_FILE,     "OPEN_FILE"},
    {TIMEOUT_ACTION_SHOW_TIME,     "SHOW_TIME"},
    {TIMEOUT_ACTION_COUNT_UP,      "COUNT_UP"},
    {TIMEOUT_ACTION_OPEN_WEBSITE,  "OPEN_WEBSITE"},
    {TIMEOUT_ACTION_SLEEP,         "SLEEP"},
    {-1, NULL}
};

static const char* EnumToString(const EnumStrMap* map, int value, const char* defaultVal) {
    if (!map) return defaultVal;
    for (int i = 0; map[i].str != NULL; i++) {
        if (map[i].value == value) {
            return map[i].str;
        }
    }
    return defaultVal;
}

static int StringToEnum(const EnumStrMap* map, const char* str, int defaultVal) {
    if (!map || !str) return defaultVal;
    for (int i = 0; map[i].str != NULL; i++) {
        if (strcmp(map[i].str, str) == 0) {
            return map[i].value;
        }
    }
    return defaultVal;
}

/**
 * @brief Create default configuration file with system language detection
 */
void CreateDefaultConfig(const char* config_path) {
    /** Detect system language */
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
        case LANG_SPANISH:   defaultLanguage = APP_LANG_SPANISH;    break;
        case LANG_FRENCH:    defaultLanguage = APP_LANG_FRENCH;     break;
        case LANG_GERMAN:    defaultLanguage = APP_LANG_GERMAN;     break;
        case LANG_RUSSIAN:   defaultLanguage = APP_LANG_RUSSIAN;    break;
        case LANG_PORTUGUESE: defaultLanguage = APP_LANG_PORTUGUESE; break;
        case LANG_JAPANESE:  defaultLanguage = APP_LANG_JAPANESE;   break;
        case LANG_KOREAN:    defaultLanguage = APP_LANG_KOREAN;     break;
        case LANG_ENGLISH:
        default:
            defaultLanguage = APP_LANG_ENGLISH;
            break;
    }
    
    const char* langName = EnumToString(LANGUAGE_MAP, defaultLanguage, "English");
    const char* typeStr = EnumToString(NOTIFICATION_TYPE_MAP, NOTIFICATION_TYPE, "CATIME");
    
    WriteIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", CATIME_VERSION, config_path);
    WriteIniString(INI_SECTION_GENERAL, "LANGUAGE", langName, config_path);
    WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", "FALSE", config_path);
    WriteIniString(INI_SECTION_GENERAL, "FIRST_RUN", "TRUE", config_path);
    WriteIniString(INI_SECTION_GENERAL, "FONT_LICENSE_ACCEPTED", "FALSE", config_path);
    WriteIniString(INI_SECTION_GENERAL, "FONT_LICENSE_VERSION_ACCEPTED", "", config_path);
    
    WriteIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", DEFAULT_TEXT_COLOR, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", DEFAULT_FONT_SIZE, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", FONTS_PATH_PREFIX DEFAULT_FONT_NAME, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", DEFAULT_WINDOW_POS_X, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", DEFAULT_WINDOW_POS_Y, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", DEFAULT_WINDOW_SCALE, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", "TRUE", config_path);
    
    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", 1500, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", "DEFAULT", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", "1500,600,300", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", "0", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", "", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "", config_path);
    WriteIniString(INI_SECTION_TIMER, "STARTUP_MODE", "COUNTDOWN", config_path);
    
    WriteIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "1500,300,1500,600", config_path);
    WriteIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", 1, config_path);
    
    WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", DEFAULT_TIMEOUT_MESSAGE, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", DEFAULT_POMODORO_MESSAGE, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", DEFAULT_POMODORO_COMPLETE_MSG, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", 3000, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", 95, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "", config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", 100, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", "FALSE", config_path);
    
    /** Animation settings */
    WriteIniString("Animation", "ANIMATION_PATH", "__logo__", config_path);
    WriteIniString("Animation", "ANIMATION_SPEED_METRIC", "MEMORY", config_path);
    WriteIniInt("Animation", "ANIMATION_SPEED_DEFAULT", 100, config_path);
    WriteIniString("Animation", "ANIMATION_SPEED_MAP_10",  "140", config_path);
    WriteIniString("Animation", "ANIMATION_SPEED_MAP_20",  "180", config_path);
    WriteIniString("Animation", "ANIMATION_SPEED_MAP_30",  "220", config_path);
    WriteIniString("Animation", "ANIMATION_SPEED_MAP_40",  "260", config_path);
    WriteIniString("Animation", "ANIMATION_SPEED_MAP_50",  "300", config_path);
    WriteIniString("Animation", "ANIMATION_SPEED_MAP_60",  "340", config_path);
    WriteIniString("Animation", "ANIMATION_SPEED_MAP_70",  "380", config_path);
    WriteIniString("Animation", "ANIMATION_SPEED_MAP_80",  "420", config_path);
    WriteIniString("Animation", "ANIMATION_SPEED_MAP_90",  "460", config_path);
    WriteIniString("Animation", "ANIMATION_SPEED_MAP_100", "500", config_path);
    WriteIniString("Animation", "PERCENT_ICON_TEXT_COLOR", DEFAULT_BLACK_COLOR, config_path);
    WriteIniString("Animation", "PERCENT_ICON_BG_COLOR", DEFAULT_WHITE_COLOR, config_path);
    WriteIniInt("Animation", "ANIMATION_FOLDER_INTERVAL_MS", 150, config_path);
    WriteIniInt("Animation", "ANIMATION_MIN_INTERVAL_MS", 0, config_path);

    /** Hotkeys */
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_SHOW_TIME", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNT_UP", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNTDOWN", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN1", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN2", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN3", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_POMODORO", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_TOGGLE_VISIBILITY", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_EDIT_MODE", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_PAUSE_RESUME", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_RESTART_TIMER", "None", config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_CUSTOM_COUNTDOWN", "None", config_path);

    /** Recent files */
    for (int i = 1; i <= 5; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        WriteIniString(INI_SECTION_RECENTFILES, key, "", config_path);
    }
    
    /** Colors */
    WriteIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", DEFAULT_COLOR_OPTIONS_INI, config_path);
}


/**
 * @brief Read configuration from INI file
 */
void ReadConfig() {
    CheckAndCreateResourceFolders();
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    if (!FileExists(config_path)) {
        CreateDefaultConfig(config_path);
    }
    
    /** Version check */
    char version[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", "", version, sizeof(version), config_path);
    
    if (strcmp(version, CATIME_VERSION) != 0) {
        CreateDefaultConfig(config_path);
    }

    time_options_count = 0;
    memset(time_options, 0, sizeof(time_options));
    CLOCK_RECENT_FILES_COUNT = 0;
    
    /** Language */
    char language[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "LANGUAGE", "English", language, sizeof(language), config_path);
    
    FONT_LICENSE_ACCEPTED = ReadIniBool(INI_SECTION_GENERAL, "FONT_LICENSE_ACCEPTED", FALSE, config_path);
    ReadIniString(INI_SECTION_GENERAL, "FONT_LICENSE_VERSION_ACCEPTED", "", 
                  FONT_LICENSE_VERSION_ACCEPTED, sizeof(FONT_LICENSE_VERSION_ACCEPTED), config_path);
    
    /** Time format */
    char timeFormat[32] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", "DEFAULT", timeFormat, sizeof(timeFormat), config_path);
    CLOCK_TIME_FORMAT = StringToEnum(TIME_FORMAT_MAP, timeFormat, TIME_FORMAT_DEFAULT);
    
    CLOCK_SHOW_MILLISECONDS = ReadIniBool(INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", FALSE, config_path);
    
    int languageSetting = StringToEnum(LANGUAGE_MAP, language, APP_LANG_ENGLISH);
    
    /** Legacy numeric format */
    if (languageSetting == APP_LANG_ENGLISH && isdigit(language[0])) {
        int langValue = atoi(language);
        if (langValue >= 0 && langValue < APP_LANG_COUNT) {
            languageSetting = langValue;
        }
    }
    
    /** Display configuration */
    ReadIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", DEFAULT_TEXT_COLOR, CLOCK_TEXT_COLOR, sizeof(CLOCK_TEXT_COLOR), config_path);
    CLOCK_BASE_FONT_SIZE = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", DEFAULT_FONT_SIZE, config_path);
    ReadIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", FONTS_PATH_PREFIX DEFAULT_FONT_NAME, FONT_FILE_NAME, sizeof(FONT_FILE_NAME), config_path);
    
    /** Process font file name */
    char actualFontFileName[MAX_PATH];
    BOOL isFontsFolderFont = FALSE;
    
    const char* localappdata_prefix = FONTS_PATH_PREFIX;
    if (_strnicmp(FONT_FILE_NAME, localappdata_prefix, strlen(localappdata_prefix)) == 0) {
        strncpy(actualFontFileName, FONT_FILE_NAME + strlen(localappdata_prefix), sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
        isFontsFolderFont = TRUE;
    } else {
        strncpy(actualFontFileName, FONT_FILE_NAME, sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
    }
    
    /** Set FONT_INTERNAL_NAME */
    if (isFontsFolderFont) {
        char fontPath[MAX_PATH];
        BOOL fontFound = FALSE;
        
        wchar_t wConfigPath[MAX_PATH] = {0};
        char configPathUtf8[MAX_PATH] = {0};
        GetConfigPath(configPathUtf8, MAX_PATH);
        MultiByteToWideChar(CP_UTF8, 0, configPathUtf8, -1, wConfigPath, MAX_PATH);
        
        wchar_t* lastSep = wcsrchr(wConfigPath, L'\\');
        if (lastSep) {
            *lastSep = L'\0';
            
            wchar_t wActualFontFileName[MAX_PATH] = {0};
            MultiByteToWideChar(CP_UTF8, 0, actualFontFileName, -1, wActualFontFileName, MAX_PATH);
            
            wchar_t wFontPath[MAX_PATH] = {0};
            _snwprintf_s(wFontPath, MAX_PATH, _TRUNCATE, L"%s\\resources\\fonts\\%s", wConfigPath, wActualFontFileName);
            
            if (GetFileAttributesW(wFontPath) != INVALID_FILE_ATTRIBUTES) {
                fontFound = TRUE;
                WideCharToMultiByte(CP_UTF8, 0, wFontPath, -1, fontPath, MAX_PATH, NULL, NULL);
            }
            
            if (fontFound) {
                extern BOOL GetFontNameFromFile(const char* fontPath, char* fontName, size_t fontNameSize);
                if (!GetFontNameFromFile(fontPath, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
                    char* lastSlash = strrchr(actualFontFileName, '\\');
                    const char* filenameOnly = lastSlash ? (lastSlash + 1) : actualFontFileName;
                    strncpy(FONT_INTERNAL_NAME, filenameOnly, sizeof(FONT_INTERNAL_NAME) - 1);
                    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
                    char* dot = strrchr(FONT_INTERNAL_NAME, '.');
                    if (dot) *dot = '\0';
                }
            } else {
                char* lastSlash = strrchr(actualFontFileName, '\\');
                const char* filenameOnly = lastSlash ? (lastSlash + 1) : actualFontFileName;
                strncpy(FONT_INTERNAL_NAME, filenameOnly, sizeof(FONT_INTERNAL_NAME) - 1);
                FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
                char* dot = strrchr(FONT_INTERNAL_NAME, '.');
                if (dot) *dot = '\0';
            }
        } else {
            char* lastSlash = strrchr(actualFontFileName, '\\');
            const char* filenameOnly = lastSlash ? (lastSlash + 1) : actualFontFileName;
            strncpy(FONT_INTERNAL_NAME, filenameOnly, sizeof(FONT_INTERNAL_NAME) - 1);
            FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
            char* dot = strrchr(FONT_INTERNAL_NAME, '.');
            if (dot) *dot = '\0';
        }
    } else {
        strncpy(FONT_INTERNAL_NAME, actualFontFileName, sizeof(FONT_INTERNAL_NAME) - 1);
        FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
        char* dot = strrchr(FONT_INTERNAL_NAME, '.');
        if (dot) *dot = '\0';
    }
    
    CLOCK_WINDOW_POS_X = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", DEFAULT_WINDOW_POS_X, config_path);
    CLOCK_WINDOW_POS_Y = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", DEFAULT_WINDOW_POS_Y, config_path);
    
    char scaleStr[16] = {0};
    ReadIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", DEFAULT_WINDOW_SCALE, scaleStr, sizeof(scaleStr), config_path);
    CLOCK_WINDOW_SCALE = atof(scaleStr);
    
    CLOCK_WINDOW_TOPMOST = ReadIniBool(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", TRUE, config_path);
    
    /** Avoid pure black */
    if (strcasecmp(CLOCK_TEXT_COLOR, "#000000") == 0) {
        strncpy(CLOCK_TEXT_COLOR, "#000001", sizeof(CLOCK_TEXT_COLOR) - 1);
    }
    
    /** Timer configuration */
    CLOCK_DEFAULT_START_TIME = ReadIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", 1500, config_path);
    CLOCK_USE_24HOUR = ReadIniBool(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", FALSE, config_path);
    CLOCK_SHOW_SECONDS = ReadIniBool(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", FALSE, config_path);
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", "0", CLOCK_TIMEOUT_TEXT, sizeof(CLOCK_TIMEOUT_TEXT), config_path);
    
    char timeoutAction[32] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE", timeoutAction, sizeof(timeoutAction), config_path);
    CLOCK_TIMEOUT_ACTION = StringToEnum(TIMEOUT_ACTION_MAP, timeoutAction, TIMEOUT_ACTION_MESSAGE);
    
    /** Security: filter dangerous actions */
    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN || 
        CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART || 
        CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SLEEP) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    }
    
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", "", CLOCK_TIMEOUT_FILE_PATH, MAX_PATH, config_path);
    
    char tempWebsiteUrl[MAX_PATH] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "", tempWebsiteUrl, MAX_PATH, config_path);
    if (tempWebsiteUrl[0] != '\0') {
        MultiByteToWideChar(CP_UTF8, 0, tempWebsiteUrl, -1, CLOCK_TIMEOUT_WEBSITE_URL, MAX_PATH);
    } else {
        CLOCK_TIMEOUT_WEBSITE_URL[0] = L'\0';
    }
    
    if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        if (FileExistsUtf8(CLOCK_TIMEOUT_FILE_PATH)) {
            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
        }
    }
    
    if (wcslen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
    }
    
    /** Parse time options */
    char timeOptions[256] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", "1500,600,300", timeOptions, sizeof(timeOptions), config_path);
    
    char *token = strtok(timeOptions, ",");
    while (token && time_options_count < MAX_TIME_OPTIONS) {
        while (*token == ' ') token++;
        time_options[time_options_count++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    ReadIniString(INI_SECTION_TIMER, "STARTUP_MODE", "COUNTDOWN", CLOCK_STARTUP_MODE, sizeof(CLOCK_STARTUP_MODE), config_path);
    
    /** Pomodoro */
    char pomodoroTimeOptions[256] = {0};
    ReadIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "1500,300,1500,600", pomodoroTimeOptions, sizeof(pomodoroTimeOptions), config_path);
    
    POMODORO_TIMES_COUNT = 0;
    token = strtok(pomodoroTimeOptions, ",");
    while (token && POMODORO_TIMES_COUNT < MAX_POMODORO_TIMES) {
        POMODORO_TIMES[POMODORO_TIMES_COUNT++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    if (POMODORO_TIMES_COUNT > 0) {
        POMODORO_WORK_TIME = POMODORO_TIMES[0];
        if (POMODORO_TIMES_COUNT > 1) POMODORO_SHORT_BREAK = POMODORO_TIMES[1];
        if (POMODORO_TIMES_COUNT > 2) POMODORO_LONG_BREAK = POMODORO_TIMES[3];
    }
    
    POMODORO_LOOP_COUNT = ReadIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", 1, config_path);
    if (POMODORO_LOOP_COUNT < 1) POMODORO_LOOP_COUNT = 1;
    
    /** Notifications */
    ReadIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", DEFAULT_TIMEOUT_MESSAGE, 
                 CLOCK_TIMEOUT_MESSAGE_TEXT, sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT), config_path);
                 
    ReadIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", DEFAULT_POMODORO_MESSAGE, 
                 POMODORO_TIMEOUT_MESSAGE_TEXT, sizeof(POMODORO_TIMEOUT_MESSAGE_TEXT), config_path);
                 
    ReadIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", DEFAULT_POMODORO_COMPLETE_MSG, 
                 POMODORO_CYCLE_COMPLETE_TEXT, sizeof(POMODORO_CYCLE_COMPLETE_TEXT), config_path);
                 
    NOTIFICATION_TIMEOUT_MS = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", 3000, config_path);
    NOTIFICATION_MAX_OPACITY = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", 95, config_path);
    
    if (NOTIFICATION_MAX_OPACITY < 1) NOTIFICATION_MAX_OPACITY = 1;
    if (NOTIFICATION_MAX_OPACITY > 100) NOTIFICATION_MAX_OPACITY = 100;
    
    char notificationType[32] = {0};
    ReadIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", "CATIME", notificationType, sizeof(notificationType), config_path);
    NOTIFICATION_TYPE = StringToEnum(NOTIFICATION_TYPE_MAP, notificationType, NOTIFICATION_TYPE_CATIME);
    
    ReadIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", "", 
                NOTIFICATION_SOUND_FILE, MAX_PATH, config_path);
                
    NOTIFICATION_SOUND_VOLUME = ReadIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", 100, config_path);
    NOTIFICATION_DISABLED = ReadIniBool(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", FALSE, config_path);
    
    if (NOTIFICATION_SOUND_VOLUME < 0) NOTIFICATION_SOUND_VOLUME = 0;
    if (NOTIFICATION_SOUND_VOLUME > 100) NOTIFICATION_SOUND_VOLUME = 100;
    
    /** Colors */
    char colorOptions[1024] = {0};
    ReadIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", DEFAULT_COLOR_OPTIONS_INI, 
                colorOptions, sizeof(colorOptions), config_path);
                
    token = strtok(colorOptions, ",");
    COLOR_OPTIONS_COUNT = 0;
    while (token) {
        COLOR_OPTIONS = realloc(COLOR_OPTIONS, sizeof(PredefinedColor) * (COLOR_OPTIONS_COUNT + 1));
        if (COLOR_OPTIONS) {
            COLOR_OPTIONS[COLOR_OPTIONS_COUNT].hexColor = strdup(token);
            COLOR_OPTIONS_COUNT++;
        }
        token = strtok(NULL, ",");
    }
    
    /** Recent files */
    for (int i = 1; i <= MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        
        char filePath[MAX_PATH] = {0};
        ReadIniString(INI_SECTION_RECENTFILES, key, "", filePath, MAX_PATH, config_path);
        
        if (strlen(filePath) > 0) {
            if (FileExistsUtf8(filePath)) {
                strncpy(CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path, filePath, MAX_PATH - 1);
                CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].path[MAX_PATH - 1] = '\0';
                ExtractFileName(filePath, CLOCK_RECENT_FILES[CLOCK_RECENT_FILES_COUNT].name, MAX_PATH);
                CLOCK_RECENT_FILES_COUNT++;
            }
        }
    }
    
    /** Hotkeys */
    WORD showTimeHotkey = 0;
    WORD countUpHotkey = 0;
    WORD countdownHotkey = 0;
    WORD quickCountdown1Hotkey = 0;
    WORD quickCountdown2Hotkey = 0;
    WORD quickCountdown3Hotkey = 0;
    WORD pomodoroHotkey = 0;
    WORD toggleVisibilityHotkey = 0;
    WORD editModeHotkey = 0;
    WORD pauseResumeHotkey = 0;
    WORD restartTimerHotkey = 0;
    WORD customCountdownHotkey = 0;
    
    char hotkeyStr[32] = {0};
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_SHOW_TIME", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    showTimeHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNT_UP", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    countUpHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNTDOWN", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    countdownHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN1", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    quickCountdown1Hotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN2", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    quickCountdown2Hotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN3", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    quickCountdown3Hotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_POMODORO", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    pomodoroHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_TOGGLE_VISIBILITY", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    toggleVisibilityHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_EDIT_MODE", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    editModeHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_PAUSE_RESUME", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    pauseResumeHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_RESTART_TIMER", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    restartTimerHotkey = StringToHotkey(hotkeyStr);
    
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_CUSTOM_COUNTDOWN", "None", hotkeyStr, sizeof(hotkeyStr), config_path);
    customCountdownHotkey = StringToHotkey(hotkeyStr);
    
    last_config_time = time(NULL);

    HWND hwnd = FindWindowW(L"CatimeWindow", L"Catime");
    if (hwnd) {
        SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        InvalidateRect(hwnd, NULL, TRUE);
    }

    SetLanguage((AppLanguage)languageSetting);

    /** Load animation speed */
    {
        extern void ReloadAnimationSpeedFromConfig(void);
        ReloadAnimationSpeedFromConfig();
    }
}


/**
 * @brief Write complete configuration to INI file
 */
void WriteConfig(const char* config_path) {
    AppLanguage currentLang = GetCurrentLanguage();
    const char* langName = EnumToString(LANGUAGE_MAP, currentLang, "English");
    const char* typeStr = EnumToString(NOTIFICATION_TYPE_MAP, NOTIFICATION_TYPE, "CATIME");
    
    /** Read hotkeys */
    WORD showTimeHotkey = 0;
    WORD countUpHotkey = 0;
    WORD countdownHotkey = 0;
    WORD quickCountdown1Hotkey = 0;
    WORD quickCountdown2Hotkey = 0;
    WORD quickCountdown3Hotkey = 0;
    WORD pomodoroHotkey = 0;
    WORD toggleVisibilityHotkey = 0;
    WORD editModeHotkey = 0;
    WORD pauseResumeHotkey = 0;
    WORD restartTimerHotkey = 0;
    WORD customCountdownHotkey = 0;
    
    ReadConfigHotkeys(&showTimeHotkey, &countUpHotkey, &countdownHotkey,
                      &quickCountdown1Hotkey, &quickCountdown2Hotkey, &quickCountdown3Hotkey,
                      &pomodoroHotkey, &toggleVisibilityHotkey, &editModeHotkey,
                      &pauseResumeHotkey, &restartTimerHotkey);
    
    ReadCustomCountdownHotkey(&customCountdownHotkey);
    
    char showTimeStr[64] = {0};
    char countUpStr[64] = {0};
    char countdownStr[64] = {0};
    char quickCountdown1Str[64] = {0};
    char quickCountdown2Str[64] = {0};
    char quickCountdown3Str[64] = {0};
    char pomodoroStr[64] = {0};
    char toggleVisibilityStr[64] = {0};
    char editModeStr[64] = {0};
    char pauseResumeStr[64] = {0};
    char restartTimerStr[64] = {0};
    char customCountdownStr[64] = {0};
    
    HotkeyToString(showTimeHotkey, showTimeStr, sizeof(showTimeStr));
    HotkeyToString(countUpHotkey, countUpStr, sizeof(countUpStr));
    HotkeyToString(countdownHotkey, countdownStr, sizeof(countdownStr));
    HotkeyToString(quickCountdown1Hotkey, quickCountdown1Str, sizeof(quickCountdown1Str));
    HotkeyToString(quickCountdown2Hotkey, quickCountdown2Str, sizeof(quickCountdown2Str));
    HotkeyToString(quickCountdown3Hotkey, quickCountdown3Str, sizeof(quickCountdown3Str));
    HotkeyToString(pomodoroHotkey, pomodoroStr, sizeof(pomodoroStr));
    HotkeyToString(toggleVisibilityHotkey, toggleVisibilityStr, sizeof(toggleVisibilityStr));
    HotkeyToString(editModeHotkey, editModeStr, sizeof(editModeStr));
    HotkeyToString(pauseResumeHotkey, pauseResumeStr, sizeof(pauseResumeStr));
    HotkeyToString(restartTimerHotkey, restartTimerStr, sizeof(restartTimerStr));
    HotkeyToString(customCountdownHotkey, customCountdownStr, sizeof(customCountdownStr));
    
    char timeOptionsStr[256] = {0};
    for (int i = 0; i < time_options_count; i++) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", time_options[i]);
        if (i > 0) strcat(timeOptionsStr, ",");
        strcat(timeOptionsStr, buffer);
    }
    
    char pomodoroTimesStr[256] = {0};
    for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", POMODORO_TIMES[i]);
        if (i > 0) strcat(pomodoroTimesStr, ",");
        strcat(pomodoroTimesStr, buffer);
    }
    
    char colorOptionsStr[1024] = {0};
    for (int i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (i > 0) strcat(colorOptionsStr, ",");
        strcat(colorOptionsStr, COLOR_OPTIONS[i].hexColor);
    }
    
    int safeAction = CLOCK_TIMEOUT_ACTION;
    if (safeAction == TIMEOUT_ACTION_SHUTDOWN || 
        safeAction == TIMEOUT_ACTION_RESTART || 
        safeAction == TIMEOUT_ACTION_SLEEP) {
        safeAction = TIMEOUT_ACTION_MESSAGE;
    }
    const char* timeoutActionStr = EnumToString(TIMEOUT_ACTION_MAP, safeAction, "MESSAGE");
    
    WriteIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", CATIME_VERSION, config_path);
    WriteIniString(INI_SECTION_GENERAL, "LANGUAGE", langName, config_path);
    WriteIniString(INI_SECTION_GENERAL, "SHORTCUT_CHECK_DONE", IsShortcutCheckDone() ? "TRUE" : "FALSE", config_path);
    
    char currentFirstRun[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "FIRST_RUN", "FALSE", currentFirstRun, sizeof(currentFirstRun), config_path);
    WriteIniString(INI_SECTION_GENERAL, "FIRST_RUN", currentFirstRun, config_path);
    
    WriteIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", CLOCK_TEXT_COLOR, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", CLOCK_BASE_FONT_SIZE, config_path);
    WriteIniString(INI_SECTION_DISPLAY, "FONT_FILE_NAME", FONT_FILE_NAME, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", CLOCK_WINDOW_POS_X, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", CLOCK_WINDOW_POS_Y, config_path);
    
    char scaleStr[16];
    snprintf(scaleStr, sizeof(scaleStr), "%.2f", CLOCK_WINDOW_SCALE);
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", scaleStr, config_path);
    
    WriteIniString(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", CLOCK_WINDOW_TOPMOST ? "TRUE" : "FALSE", config_path);
    
    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", CLOCK_DEFAULT_START_TIME, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", CLOCK_USE_24HOUR ? "TRUE" : "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", CLOCK_SHOW_SECONDS ? "TRUE" : "FALSE", config_path);
    
    const char* timeFormatStr = EnumToString(TIME_FORMAT_MAP, CLOCK_TIME_FORMAT, "DEFAULT");
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", timeFormatStr, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", CLOCK_SHOW_MILLISECONDS ? "TRUE" : "FALSE", config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", CLOCK_TIMEOUT_TEXT, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", timeoutActionStr, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", CLOCK_TIMEOUT_FILE_PATH, config_path);

    char tempWebsiteUrl[MAX_PATH * 3] = {0};
    WideCharToMultiByte(CP_UTF8, 0, CLOCK_TIMEOUT_WEBSITE_URL, -1, tempWebsiteUrl, sizeof(tempWebsiteUrl), NULL, NULL);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", tempWebsiteUrl, config_path);
    WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", timeOptionsStr, config_path);
    WriteIniString(INI_SECTION_TIMER, "STARTUP_MODE", CLOCK_STARTUP_MODE, config_path);
    
    WriteIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", pomodoroTimesStr, config_path);
    WriteIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", POMODORO_LOOP_COUNT, config_path);
    
    WriteIniString(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT", CLOCK_TIMEOUT_MESSAGE_TEXT, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_TIMEOUT_MESSAGE_TEXT", POMODORO_TIMEOUT_MESSAGE_TEXT, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "POMODORO_CYCLE_COMPLETE_TEXT", POMODORO_CYCLE_COMPLETE_TEXT, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", NOTIFICATION_TIMEOUT_MS, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY", NOTIFICATION_MAX_OPACITY, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", typeStr, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE", NOTIFICATION_SOUND_FILE, config_path);
    WriteIniInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", NOTIFICATION_SOUND_VOLUME, config_path);
    WriteIniString(INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", NOTIFICATION_DISABLED ? "TRUE" : "FALSE", config_path);
    
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_SHOW_TIME", showTimeStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNT_UP", countUpStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_COUNTDOWN", countdownStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN1", quickCountdown1Str, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN2", quickCountdown2Str, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_QUICK_COUNTDOWN3", quickCountdown3Str, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_POMODORO", pomodoroStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_TOGGLE_VISIBILITY", toggleVisibilityStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_EDIT_MODE", editModeStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_PAUSE_RESUME", pauseResumeStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_RESTART_TIMER", restartTimerStr, config_path);
    WriteIniString(INI_SECTION_HOTKEYS, "HOTKEY_CUSTOM_COUNTDOWN", customCountdownStr, config_path);
    
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i + 1);
        WriteIniString(INI_SECTION_RECENTFILES, key, CLOCK_RECENT_FILES[i].path, config_path);
    }
    
    for (int i = CLOCK_RECENT_FILES_COUNT; i < MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i + 1);
        WriteIniString(INI_SECTION_RECENTFILES, key, "", config_path);
    }
    
    WriteIniString(INI_SECTION_COLORS, "COLOR_OPTIONS", colorOptionsStr, config_path);
    
    /** Persist animation settings */
    {
        extern const char* GetCurrentAnimationName();
        const char* anim = GetCurrentAnimationName();
        if (anim && anim[0] != '\0') {
            char animPath[MAX_PATH];
            snprintf(animPath, sizeof(animPath), "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", anim);
            WriteIniString("Animation", "ANIMATION_PATH", animPath, config_path);
        }
    }

    /** Write animation speed */
    extern void WriteAnimationSpeedToConfig(const char* config_path);
    WriteAnimationSpeedToConfig(config_path);
}


/**
 * @brief Update timeout action in config file
 */
void WriteConfigTimeoutAction(const char* action) {
    const char* actual_action = action;
    if (strcmp(action, "RESTART") == 0 || strcmp(action, "SHUTDOWN") == 0 || strcmp(action, "SLEEP") == 0) {
        actual_action = "MESSAGE";
    }
    
    UpdateConfigKeyValueAtomic(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", actual_action);
}


void WriteConfigTimeOptions(const char* options) {
    UpdateConfigKeyValueAtomic(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", options);
}


/**
 * @brief Write window topmost setting to config file
 */
void WriteConfigTopmost(const char* topmost) {
    UpdateConfigKeyValueAtomic(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", topmost);
}


/**
 * @brief Configure timeout action to open file
 */
void WriteConfigTimeoutFile(const char* filePath) {
    if (!filePath) filePath = "";
    WriteConfigKeyValue("CLOCK_TIMEOUT_ACTION", "OPEN_FILE");
    WriteConfigKeyValue("CLOCK_TIMEOUT_FILE", filePath);
}


/**
 * @brief Configure timeout action to open website
 */
void WriteConfigTimeoutWebsite(const char* url) {
    if (!url) url = "";
    WriteConfigKeyValue("CLOCK_TIMEOUT_ACTION", "OPEN_WEBSITE");
    WriteConfigKeyValue("CLOCK_TIMEOUT_WEBSITE", url);
}

