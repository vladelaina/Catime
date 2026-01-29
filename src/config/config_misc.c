/**
 * @file config_misc.c
 * @brief Miscellaneous configuration management
 * 
 * Manages pomodoro settings, recent files, font license, language, time format, and other settings.
 */
#include "config.h"
#include "language.h"
#include "../resource/resource.h"
#include "color/gradient.h"
#include "color/color_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];

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

/** Enum-string mapping */
typedef struct {
    int value;
    const char* str;
} EnumStrMap;

static const EnumStrMap TIME_FORMAT_MAP[] = {
    {TIME_FORMAT_DEFAULT,      "DEFAULT"},
    {TIME_FORMAT_ZERO_PADDED,  "ZERO_PADDED"},
    {TIME_FORMAT_FULL_PADDED,  "FULL_PADDED"},
    {-1, NULL}
};

static const EnumStrMap TIMEOUT_ACTION_MAP[] = {
    {TIMEOUT_ACTION_MESSAGE,      "MESSAGE"},
    {TIMEOUT_ACTION_LOCK,         "LOCK"},
    {TIMEOUT_ACTION_OPEN_FILE,    "OPEN_FILE"},
    {TIMEOUT_ACTION_SHOW_TIME,    "SHOW_TIME"},
    {TIMEOUT_ACTION_COUNT_UP,     "COUNT_UP"},
    {TIMEOUT_ACTION_OPEN_WEBSITE, "OPEN_WEBSITE"},
    {TIMEOUT_ACTION_SLEEP,        "SLEEP"},
    {TIMEOUT_ACTION_SHUTDOWN,     "SHUTDOWN"},
    {TIMEOUT_ACTION_RESTART,      "RESTART"},
    {-1, NULL}
};

/* LANGUAGE_MAP definition removed - logic moved to language.c */

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
 * @brief Write pomodoro timing configuration to config file
 */
void WriteConfigPomodoroTimes(int work, int short_break, int long_break) {
    char timesStr[128];
    snprintf(timesStr, sizeof(timesStr), "%d,%d,%d", work, short_break, long_break);
    UpdateConfigKeyValueAtomic(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", timesStr);
}




/**
 * @brief Write pomodoro settings (combined times array)
 */
void WriteConfigPomodoroSettings(int work, int short_break, int long_break, int long_break2) {
    char timesStr[128];
    snprintf(timesStr, sizeof(timesStr), "%d,%d,%d,%d", work, short_break, long_break, long_break2);
    UpdateConfigKeyValueAtomic(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", timesStr);
}


/**
 * @brief Write pomodoro loop count to config file
 */
void WriteConfigPomodoroLoopCount(int loop_count) {
    g_AppConfig.pomodoro.loop_count = loop_count;
    UpdateConfigIntAtomic(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", loop_count);
}


/**
 * @brief Write custom pomodoro time intervals to config
 */
void WriteConfigPomodoroTimeOptions(int* times, int count) {
    if (!times || count <= 0) return;
    
    char timesStr[512] = {0};
    size_t offset = 0;
    for (int i = 0; i < count && offset < sizeof(timesStr) - 16; i++) {
        if (i > 0) {
            offset += snprintf(timesStr + offset, sizeof(timesStr) - offset, ",");
        }
        offset += snprintf(timesStr + offset, sizeof(timesStr) - offset, "%d", times[i]);
    }
    
    UpdateConfigKeyValueAtomic(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", timesStr);
}


/**
 * @brief Load recent files list from config with file existence validation
 */
void LoadRecentFiles(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    FOPEN_UTF8(config_path, L"r", file);
    if (!file) return;

    char line[MAX_PATH];
    g_AppConfig.recent_files.count = 0;

    while (fgets(line, sizeof(line), file)) {

        if (strncmp(line, "CLOCK_RECENT_FILE_", 18) == 0) {
            char *path = strchr(line + 18, '=');
            if (path) {
                path++;
                char *newline = strchr(path, '\n');
                if (newline) *newline = '\0';

                if (g_AppConfig.recent_files.count < MAX_RECENT_FILES) {

                    if (FileExistsUtf8(path)) {
                        strncpy(g_AppConfig.recent_files.files[g_AppConfig.recent_files.count].path, path, MAX_PATH - 1);
                        g_AppConfig.recent_files.files[g_AppConfig.recent_files.count].path[MAX_PATH - 1] = '\0';

                        char *filename = strrchr(g_AppConfig.recent_files.files[g_AppConfig.recent_files.count].path, '\\');
                        if (filename) filename++;
                        else filename = g_AppConfig.recent_files.files[g_AppConfig.recent_files.count].path;
                        
                        strncpy(g_AppConfig.recent_files.files[g_AppConfig.recent_files.count].name, filename, MAX_PATH - 1);
                        g_AppConfig.recent_files.files[g_AppConfig.recent_files.count].name[MAX_PATH - 1] = '\0';

                        g_AppConfig.recent_files.count++;
                    }
                }
            }
        }
    }

    fclose(file);
}


/**
 * @brief Add file to recent files list with MRU ordering
 */
void SaveRecentFile(const char* filePath) {
    if (!filePath || strlen(filePath) == 0) return;

    if (!FileExistsUtf8(filePath)) {
        return;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    const int kMax = MAX_RECENT_FILES;
    char items[MAX_RECENT_FILES][MAX_PATH];
    int count = 0;
    for (int i = 1; i <= kMax; ++i) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        ReadIniString(INI_SECTION_RECENTFILES, key, "", items[count], MAX_PATH, config_path);
        if (items[count][0] != '\0') {
            count++;
        }
    }

    /** Remove if exists */
    int writeIdx = 0;
    char newList[MAX_RECENT_FILES][MAX_PATH];
    memset(newList, 0, sizeof(newList));

    /** Insert new at top */
    strncpy(newList[writeIdx], filePath, MAX_PATH - 1);
    newList[writeIdx][MAX_PATH - 1] = '\0';
    writeIdx++;

    for (int i = 0; i < count && writeIdx < kMax; ++i) {
        if (strcmp(items[i], filePath) == 0) continue;
        strncpy(newList[writeIdx], items[i], MAX_PATH - 1);
        newList[writeIdx][MAX_PATH - 1] = '\0';
        writeIdx++;
    }

    /** Write back to INI */
    for (int i = 0; i < kMax; ++i) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i + 1);
        const char* val = (i < writeIdx) ? newList[i] : "";
        WriteIniString(INI_SECTION_RECENTFILES, key, val, config_path);
    }
}


/**
 * @brief Convert UTF-8 string to ANSI (GB2312)
 */
char* UTF8ToANSI(const char* utf8Str) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
    if (wlen == 0) {
        return _strdup(utf8Str);
    }

    wchar_t* wstr = (wchar_t*)malloc(sizeof(wchar_t) * wlen);
    if (!wstr) {
        return _strdup(utf8Str);
    }

    if (MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wstr, wlen) == 0) {
        free(wstr);
        return _strdup(utf8Str);
    }

    int len = WideCharToMultiByte(936, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (len == 0) {
        free(wstr);
        return _strdup(utf8Str);
    }

    char* str = (char*)malloc(len);
    if (!str) {
        free(wstr);
        return _strdup(utf8Str);
    }

    if (WideCharToMultiByte(936, 0, wstr, -1, str, len, NULL, NULL) == 0) {
        free(wstr);
        free(str);
        return _strdup(utf8Str);
    }

    free(wstr);
    return str;
}


/**
 * @brief Set font license agreement acceptance status
 */
void SetFontLicenseAccepted(BOOL accepted) {
    g_AppConfig.font_license.accepted = accepted;
    WriteConfigKeyValue("FONT_LICENSE_ACCEPTED", accepted ? "TRUE" : "FALSE");
}

/**
 * @brief Set font license version acceptance status
 */
void SetFontLicenseVersionAccepted(const char* version) {
    if (!version) return;
    
    strncpy(g_AppConfig.font_license.version_accepted, version, sizeof(g_AppConfig.font_license.version_accepted) - 1);
    g_AppConfig.font_license.version_accepted[sizeof(g_AppConfig.font_license.version_accepted) - 1] = '\0';
    
    WriteConfigKeyValue("FONT_LICENSE_VERSION_ACCEPTED", version);
}

/**
 * @brief Check if font license version needs acceptance
 */
BOOL NeedsFontLicenseVersionAcceptance(void) {
    if (!g_AppConfig.font_license.accepted) {
        return TRUE;
    }
    
    if (strlen(g_AppConfig.font_license.version_accepted) == 0) {
        return TRUE;
    }
    
    if (strcmp(FONT_LICENSE_VERSION, g_AppConfig.font_license.version_accepted) != 0) {
        return TRUE;
    }
    
    return FALSE;
}

/**
 * @brief Get current font license version
 */
const char* GetCurrentFontLicenseVersion(void) {
    return FONT_LICENSE_VERSION;
}


/**
 * @brief Write language setting to config file
 */
void WriteConfigLanguage(int language) {
    const char* langName = GetLanguageConfigKey(language);
    WriteConfigKeyValue("LANGUAGE", langName);
}


/**
 * @brief Write time format setting to config file
 */
void WriteConfigTimeFormat(TimeFormatType format) {
    g_AppConfig.display.time_format.format = format;
    const char* formatStr = EnumToString(TIME_FORMAT_MAP, format, "DEFAULT");
    UpdateConfigKeyValueAtomic(INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", formatStr);
}

/**
 * @brief Write milliseconds display setting to config file
 */
void WriteConfigShowMilliseconds(BOOL showMilliseconds) {
    g_AppConfig.display.time_format.show_milliseconds = showMilliseconds;
    UpdateConfigBoolAtomic(INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", showMilliseconds);
}

/**
 * @brief Get appropriate timer interval based on milliseconds display setting
 * 
 * @details Performance optimization:
 * - Milliseconds (centiseconds): 20ms = 50 FPS
 *   Rationale: Displays 0.01s precision, 50 FPS provides smooth updates
 *   while being 2x more CPU-efficient than 100 FPS. Imperceptible difference
 *   to human eye while maintaining accuracy.
 * - No milliseconds: 1000ms = 1 FPS (sufficient for seconds-only display)
 * - Animated gradient: 66ms = 15 FPS (smooth animation without excessive CPU)
 */
UINT GetTimerInterval(void) {
    extern BOOL GetActiveShowMilliseconds(void);
    extern void GetActiveColor(char* outColor, size_t bufferSize);
    
    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));
    
    /* Check for animated gradient */
    if (IsGradientAnimated(GetGradientTypeByName(activeColor))) {
        return 66; /* 15 FPS - sufficient for smooth gradient animation */
    }
    
    /* Optimized balance: 50 FPS for milliseconds (2x more efficient than 100 FPS) */
    return GetActiveShowMilliseconds() ? 20 : 1000;
}

/**
 * @brief Reset timer with appropriate interval
 * Uses high-precision multimedia timer for smooth milliseconds display
 */
void ResetTimerWithInterval(HWND hwnd) {
    extern void MainTimer_SetInterval(UINT intervalMs);
    extern BOOL MainTimer_IsHighPrecision(void);
    
    UINT interval = GetTimerInterval();
    
    /* Use multimedia timer for high precision, fallback to SetTimer */
    if (MainTimer_IsHighPrecision()) {
        MainTimer_SetInterval(interval);
    } else {
        KillTimer(hwnd, 1);
        SetTimer(hwnd, 1, interval, NULL);
    }
    
    extern void ResetTimerMilliseconds(void);
    ResetTimerMilliseconds();
}


/**
 * @brief Update startup mode configuration
 */
void WriteConfigStartupMode(const char* mode) {
    extern char CLOCK_STARTUP_MODE[20];
    
    /* Update in-memory variable */
    strncpy(CLOCK_STARTUP_MODE, mode, sizeof(CLOCK_STARTUP_MODE) - 1);
    CLOCK_STARTUP_MODE[sizeof(CLOCK_STARTUP_MODE) - 1] = '\0';
    
    /* Persist to config file */
    UpdateConfigKeyValueAtomic(INI_SECTION_TIMER, "STARTUP_MODE", mode);
}


/**
 * @brief Write arbitrary key-value pair to appropriate config section
 */
void WriteConfigKeyValue(const char* key, const char* value) {
    if (!key || !value) return;
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /** Determine appropriate section based on key prefix */
    const char* section;
    
    if (strcmp(key, "CONFIG_VERSION") == 0 ||
        strcmp(key, "LANGUAGE") == 0 ||
        strcmp(key, "SHORTCUT_CHECK_DONE") == 0 ||
        strcmp(key, "FIRST_RUN") == 0 ||
        strcmp(key, "FONT_LICENSE_ACCEPTED") == 0 ||
        strcmp(key, "FONT_LICENSE_VERSION_ACCEPTED") == 0) {
        section = INI_SECTION_GENERAL;
    }
    else if (strncmp(key, "CLOCK_TEXT_COLOR", 16) == 0 ||
           strncmp(key, "FONT_FILE_NAME", 14) == 0 ||
           strncmp(key, "CLOCK_BASE_FONT_SIZE", 20) == 0 ||
           strncmp(key, "WINDOW_SCALE", 12) == 0 ||
           strncmp(key, "PLUGIN_SCALE", 12) == 0 ||
           strncmp(key, "CLOCK_WINDOW_POS_X", 18) == 0 ||
           strncmp(key, "CLOCK_WINDOW_POS_Y", 18) == 0 ||
           strncmp(key, "WINDOW_TOPMOST", 14) == 0 ||
           strncmp(key, "WINDOW_OPACITY", 14) == 0 ||
           strncmp(key, "TEXT_", 5) == 0) {
        section = INI_SECTION_DISPLAY;
    }
    else if (strncmp(key, "CLOCK_DEFAULT_START_TIME", 24) == 0 ||
           strncmp(key, "CLOCK_USE_24HOUR", 16) == 0 ||
           strncmp(key, "CLOCK_SHOW_SECONDS", 18) == 0 ||
           strncmp(key, "CLOCK_TIME_FORMAT", 17) == 0 ||
           strncmp(key, "CLOCK_SHOW_MILLISECONDS", 23) == 0 ||
           strncmp(key, "CLOCK_TIME_OPTIONS", 18) == 0 ||
           strncmp(key, "STARTUP_MODE", 12) == 0 ||
           strncmp(key, "CLOCK_TIMEOUT_TEXT", 18) == 0 ||
           strncmp(key, "CLOCK_TIMEOUT_ACTION", 20) == 0 ||
           strncmp(key, "CLOCK_TIMEOUT_FILE", 18) == 0 ||
           strncmp(key, "CLOCK_TIMEOUT_WEBSITE", 21) == 0) {
        section = INI_SECTION_TIMER;
    }
    else if (strncmp(key, "POMODORO_", 9) == 0) {
        section = INI_SECTION_POMODORO;
    }
    else if (strncmp(key, "NOTIFICATION_", 13) == 0 ||
           strncmp(key, "CLOCK_TIMEOUT_MESSAGE_TEXT", 26) == 0) {
        section = INI_SECTION_NOTIFICATION;
    }
    else if (strncmp(key, "HOTKEY_", 7) == 0) {
        section = INI_SECTION_HOTKEYS;
    }
    else if (strncmp(key, "CLOCK_RECENT_FILE", 17) == 0) {
        section = INI_SECTION_RECENTFILES;
    }
    else if (strncmp(key, "COLOR_OPTIONS", 13) == 0) {
        section = INI_SECTION_COLORS;
    }
    else {
        section = INI_SECTION_OPTIONS;
    }
    
    WriteIniString(section, key, value, config_path);
}

/**
 * @brief Read opacity adjustment step for normal scroll
 * @return Step size (1-100), default 1
 */
int ReadConfigOpacityStepNormal(void) {
    return g_AppConfig.display.opacity_step_normal;
}

/**
 * @brief Read opacity adjustment step for Ctrl+scroll
 * @return Step size (1-100), default 5
 */
int ReadConfigOpacityStepFast(void) {
    return g_AppConfig.display.opacity_step_fast;
}

/**
 * @brief Write opacity adjustment steps to config
 * @param normal_step Normal scroll step (1-100)
 * @param fast_step Ctrl+scroll step (1-100)
 */
void WriteConfigOpacitySteps(int normal_step, int fast_step) {
    if (normal_step < 1) normal_step = 1;
    if (normal_step > 100) normal_step = 100;
    if (fast_step < 1) fast_step = 1;
    if (fast_step > 100) fast_step = 100;

    g_AppConfig.display.opacity_step_normal = normal_step;
    g_AppConfig.display.opacity_step_fast = fast_step;

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    WriteIniInt(INI_SECTION_DISPLAY, "OPACITY_STEP_NORMAL", normal_step, config_path);
    WriteIniInt(INI_SECTION_DISPLAY, "OPACITY_STEP_FAST", fast_step, config_path);
}

/* ============================================================================
 * Enum Conversion Functions (exported for use by other modules)
 * ============================================================================ */

TimeFormatType TimeFormatType_FromStr(const char* str) {
    return (TimeFormatType)StringToEnum(TIME_FORMAT_MAP, str, TIME_FORMAT_DEFAULT);
}

const char* TimeFormatType_ToStr(TimeFormatType val) {
    return EnumToString(TIME_FORMAT_MAP, val, "DEFAULT");
}

TimeoutActionType TimeoutActionType_FromStr(const char* str) {
    return (TimeoutActionType)StringToEnum(TIMEOUT_ACTION_MAP, str, TIMEOUT_ACTION_MESSAGE);
}

const char* TimeoutActionType_ToStr(TimeoutActionType val) {
    return EnumToString(TIMEOUT_ACTION_MAP, val, "MESSAGE");
}



