/**
 * @file config_misc.c
 * @brief Miscellaneous configuration management
 * 
 * Manages pomodoro settings, recent files, font license, language, time format, and other settings.
 */
#include "config.h"
#include "config/config_defaults.h"
#include "language.h"
#include "../resource/resource.h"
#include "color/gradient.h"
#include "color/color_parser.h"
#include "menu_preview.h"
#include "plugin/plugin_data.h"
#include "timer/main_timer.h"
#include "drawing/drawing_timer_precision.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];

static inline BOOL FileExistsUtf8(const char* utf8Path) {
    if (!utf8Path) return FALSE;

    wchar_t wPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH) <= 0) {
        return FALSE;
    }

    return GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES;
}

static void UpdateStartupModeBuffer(const char* mode) {
    strncpy(CLOCK_STARTUP_MODE, mode, sizeof(CLOCK_STARTUP_MODE) - 1);
    CLOCK_STARTUP_MODE[sizeof(CLOCK_STARTUP_MODE) - 1] = '\0';
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
        if (_stricmp(map[i].str, str) == 0) {
            return map[i].value;
        }
    }
    return defaultVal;
}

static BOOL BuildPomodoroTimesString(const int* times, int count,
                                     char* buffer, size_t bufferSize) {
    if (!times || count <= 0 || count > MAX_POMODORO_TIMES ||
        !buffer || bufferSize == 0) {
        return FALSE;
    }

    buffer[0] = '\0';
    size_t offset = 0;
    for (int i = 0; i < count; ++i) {
        if (times[i] <= 0 || times[i] > MAX_POMODORO_OPTION_SECONDS) {
            return FALSE;
        }

        int written = snprintf(buffer + offset, bufferSize - offset,
                               "%s%d", (i > 0) ? "," : "", times[i]);
        if (written < 0 || (size_t)written >= bufferSize - offset) {
            buffer[0] = '\0';
            return FALSE;
        }
        offset += (size_t)written;
    }

    return TRUE;
}

static BOOL PomodoroTimesStateMatches(const int* times, int count) {
    if (!times || count <= 0) return FALSE;

    if (count > MAX_POMODORO_TIMES ||
        count > (int)_countof(g_AppConfig.pomodoro.times)) {
        return FALSE;
    }

    if (g_AppConfig.pomodoro.times_count != count) {
        return FALSE;
    }

    for (int i = 0; i < count; ++i) {
        if (g_AppConfig.pomodoro.times[i] != times[i]) {
            return FALSE;
        }
    }

    if (count > 0 && g_AppConfig.pomodoro.work_time != times[0]) return FALSE;
    if (count > 1 && g_AppConfig.pomodoro.short_break != times[1]) return FALSE;
    if (count > 2 && g_AppConfig.pomodoro.long_break != times[2]) return FALSE;

    return TRUE;
}

static void UpdatePomodoroTimesState(const int* times, int count) {
    if (!times || count <= 0) return;

    if (count > MAX_POMODORO_TIMES ||
        count > (int)_countof(g_AppConfig.pomodoro.times)) {
        return;
    }

    g_AppConfig.pomodoro.times_count = count;
    ZeroMemory(g_AppConfig.pomodoro.times, sizeof(g_AppConfig.pomodoro.times));
    for (int i = 0; i < count; ++i) {
        g_AppConfig.pomodoro.times[i] = times[i];
    }

    g_AppConfig.pomodoro.work_time = times[0];
    if (count > 1) g_AppConfig.pomodoro.short_break = times[1];
    if (count > 2) g_AppConfig.pomodoro.long_break = times[2];
}

static BOOL WritePomodoroTimeOptionsStringIfChanged(const int* times, int count) {
    if (!times || count <= 0) return FALSE;
    if (count > MAX_POMODORO_TIMES ||
        count > (int)_countof(g_AppConfig.pomodoro.times)) {
        return FALSE;
    }

    char timesStr[POMODORO_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    if (!BuildPomodoroTimesString(times, count, timesStr, sizeof(timesStr))) {
        return FALSE;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentValue[POMODORO_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    BOOL currentValueComplete = ReadIniStringExact(
        INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "",
        currentValue, sizeof(currentValue), config_path);

    BOOL runtimeMatches = PomodoroTimesStateMatches(times, count);
    BOOL configMatches = currentValueComplete && strcmp(currentValue, timesStr) == 0;
    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    if (!configMatches &&
        !WriteIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", timesStr, config_path)) {
        return FALSE;
    }

    UpdatePomodoroTimesState(times, count);
    return TRUE;
}

/**
 * @brief Write pomodoro timing configuration to config file
 */
BOOL WriteConfigPomodoroTimes(int work, int short_break, int long_break) {
    const int times[] = {work, short_break, long_break};
    return WritePomodoroTimeOptionsStringIfChanged(times, (int)_countof(times));
}




/**
 * @brief Write pomodoro settings (combined times array)
 */
BOOL WriteConfigPomodoroSettings(int work, int short_break, int long_break, int long_break2) {
    const int times[] = {work, short_break, long_break, long_break2};
    return WritePomodoroTimeOptionsStringIfChanged(times, (int)_countof(times));
}


/**
 * @brief Write pomodoro loop count to config file
 */
BOOL WriteConfigPomodoroLoopCount(int loop_count) {
    if (loop_count < MIN_POMODORO_LOOP_COUNT) loop_count = MIN_POMODORO_LOOP_COUNT;
    if (loop_count > MAX_POMODORO_LOOP_COUNT) loop_count = MAX_POMODORO_LOOP_COUNT;

    char loopCountStr[32];
    if (snprintf(loopCountStr, sizeof(loopCountStr), "%d", loop_count) < 0) {
        return FALSE;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentValue[32] = {0};
    ReadIniString(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", "",
                  currentValue, sizeof(currentValue), config_path);

    BOOL runtimeMatches = g_AppConfig.pomodoro.loop_count == loop_count;
    BOOL configMatches = strcmp(currentValue, loopCountStr) == 0;
    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    if (!configMatches &&
        !WriteIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", loop_count, config_path)) {
        return FALSE;
    }

    g_AppConfig.pomodoro.loop_count = loop_count;
    return TRUE;
}


/**
 * @brief Write custom pomodoro time intervals to config
 */
BOOL WriteConfigPomodoroTimeOptions(const int* times, int count) {
    return WritePomodoroTimeOptionsStringIfChanged(times, count);
}


/**
 * @brief Load recent files list from config with file existence validation
 */
void LoadRecentFiles(void) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    g_AppConfig.recent_files.count = 0;

    for (int i = 1; i <= MAX_RECENT_FILES; ++i) {
        char key[32];
        char path[MAX_PATH] = {0};
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        if (!ReadIniStringExact(INI_SECTION_RECENTFILES, key, "", path,
                                sizeof(path), config_path)) {
            LOG_WARNING("Ignoring recent file entry %d because the config value is too long", i);
            continue;
        }

        if (path[0] == '\0' || !FileExistsUtf8(path)) {
            continue;
        }

        int index = g_AppConfig.recent_files.count;
        strncpy(g_AppConfig.recent_files.files[index].path, path, MAX_PATH - 1);
        g_AppConfig.recent_files.files[index].path[MAX_PATH - 1] = '\0';

        char *filename = strrchr(g_AppConfig.recent_files.files[index].path, '\\');
        filename = filename ? filename + 1 : g_AppConfig.recent_files.files[index].path;

        strncpy(g_AppConfig.recent_files.files[index].name, filename, MAX_PATH - 1);
        g_AppConfig.recent_files.files[index].name[MAX_PATH - 1] = '\0';

        g_AppConfig.recent_files.count++;
    }
}


/**
 * @brief Add file to recent files list with MRU ordering
 */
BOOL SaveRecentFile(const char* filePath) {
    if (!filePath || strlen(filePath) == 0) return FALSE;

    if (!FileExistsUtf8(filePath)) {
        return FALSE;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    const int kMax = MAX_RECENT_FILES;
    char currentValues[MAX_RECENT_FILES][MAX_PATH] = {{0}};
    char items[MAX_RECENT_FILES][MAX_PATH] = {{0}};
    int count = 0;
    for (int i = 1; i <= kMax; ++i) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        if (!ReadIniStringExact(INI_SECTION_RECENTFILES, key, "",
                                currentValues[i - 1], MAX_PATH, config_path)) {
            LOG_WARNING("Dropping recent file entry %d because the config value is too long", i);
            currentValues[i - 1][0] = '\0';
            continue;
        }
        if (currentValues[i - 1][0] != '\0') {
            strncpy(items[count], currentValues[i - 1], MAX_PATH - 1);
            items[count][MAX_PATH - 1] = '\0';
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

    BOOL changed = FALSE;
    for (int i = 0; i < kMax; ++i) {
        const char* nextValue = (i < writeIdx) ? newList[i] : "";
        if (strcmp(currentValues[i], nextValue) != 0) {
            changed = TRUE;
            break;
        }
    }
    if (!changed) {
        return TRUE;
    }

    /** Write back to INI */
    char keys[MAX_RECENT_FILES][32];
    IniKeyValue updates[MAX_RECENT_FILES];
    for (int i = 0; i < kMax; ++i) {
        snprintf(keys[i], sizeof(keys[i]), "CLOCK_RECENT_FILE_%d", i + 1);
        updates[i].section = INI_SECTION_RECENTFILES;
        updates[i].key = keys[i];
        updates[i].value = (i < writeIdx) ? newList[i] : "";
    }
    if (!WriteIniMultipleAtomic(config_path, updates, MAX_RECENT_FILES)) {
        LOG_WARNING("Failed to persist recent file list after adding: %s", filePath);
        return FALSE;
    }

    return TRUE;
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
    accepted = accepted ? TRUE : FALSE;
    if (!WriteConfigKeyValue("FONT_LICENSE_ACCEPTED", accepted ? "TRUE" : "FALSE")) {
        return;
    }
    g_AppConfig.font_license.accepted = accepted;
}

/**
 * @brief Set font license version acceptance status
 */
void SetFontLicenseVersionAccepted(const char* version) {
    if (!version) return;

    if (!WriteConfigKeyValue("FONT_LICENSE_VERSION_ACCEPTED", version)) {
        return;
    }

    strncpy(g_AppConfig.font_license.version_accepted, version, sizeof(g_AppConfig.font_license.version_accepted) - 1);
    g_AppConfig.font_license.version_accepted[sizeof(g_AppConfig.font_license.version_accepted) - 1] = '\0';
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
BOOL WriteConfigLanguage(int language) {
    const char* langName = GetLanguageConfigKey(language);
    if (!langName) {
        return FALSE;
    }
    return WriteConfigKeyValue("LANGUAGE", langName);
}


/**
 * @brief Write time format setting to config file
 */
BOOL WriteConfigTimeFormat(TimeFormatType format) {
    const char* formatStr = EnumToString(TIME_FORMAT_MAP, format, "DEFAULT");

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentValue[32] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", "DEFAULT",
                  currentValue, sizeof(currentValue), config_path);

    BOOL runtimeMatches = (g_AppConfig.display.time_format.format == format);
    BOOL configMatches = (strcmp(currentValue, formatStr) == 0);
    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    if (!configMatches &&
        !WriteIniString(INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", formatStr, config_path)) {
        return FALSE;
    }

    g_AppConfig.display.time_format.format = format;
    return TRUE;
}

/**
 * @brief Write milliseconds display setting to config file
 */
BOOL WriteConfigShowMilliseconds(BOOL showMilliseconds) {
    showMilliseconds = showMilliseconds ? TRUE : FALSE;

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentValue[16] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", "",
                  currentValue, sizeof(currentValue), config_path);
    const char* showMillisecondsStr = showMilliseconds ? "TRUE" : "FALSE";

    BOOL runtimeMatches = (g_AppConfig.display.time_format.show_milliseconds == showMilliseconds);
    BOOL configMatches = (strcmp(currentValue, showMillisecondsStr) == 0);
    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    if (!configMatches &&
        !WriteIniBool(INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", showMilliseconds, config_path)) {
        return FALSE;
    }

    g_AppConfig.display.time_format.show_milliseconds = showMilliseconds;
    return TRUE;
}

static BOOL IsActiveTimerColorAnimated(void) {
    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));

    static char s_lastActiveColor[COLOR_HEX_BUFFER] = {0};
    static BOOL s_lastActiveColorAnimated = FALSE;

    if (strcmp(activeColor, s_lastActiveColor) == 0) {
        return s_lastActiveColorAnimated;
    }

    strncpy_s(s_lastActiveColor, sizeof(s_lastActiveColor), activeColor, _TRUNCATE);
    s_lastActiveColorAnimated = IsGradientNameAnimated(activeColor);
    return s_lastActiveColorAnimated;
}

static BOOL IsRunningCountUpTimer(void) {
    return CLOCK_COUNT_UP && !CLOCK_IS_PAUSED;
}

static BOOL IsRunningCountdownTimer(void) {
    return !CLOCK_COUNT_UP &&
           !CLOCK_IS_PAUSED &&
           CLOCK_TOTAL_TIME > 0 &&
           countdown_elapsed_time < CLOCK_TOTAL_TIME &&
           !countdown_message_shown;
}

static BOOL ShouldRunMainTimer(HWND hwnd) {
    (void)hwnd;

    BOOL pluginActive = PluginData_IsActive();
    BOOL colorAnimated = IsActiveTimerColorAnimated();

    if (CLOCK_SHOW_CURRENT_TIME ||
        IsRunningCountUpTimer() ||
        IsRunningCountdownTimer() ||
        IsPreviewActive()) {
        return TRUE;
    }

    if (pluginActive && (PluginData_HasCatimeTag() || colorAnimated)) {
        return TRUE;
    }

    if (CLOCK_EDIT_MODE && (GetActiveShowMilliseconds() || colorAnimated)) {
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Get appropriate timer interval based on milliseconds display setting
 * 
 * @details Performance optimization:
 * - Milliseconds (centiseconds): 20ms = 50 FPS
 *   Rationale: sample from the real clock often enough for smooth display
 *   without forcing a costly 100 FPS layered-window redraw loop.
 * - No milliseconds: 1000ms = 1 FPS (sufficient for seconds-only display)
 * - Animated gradient: 66ms = 15 FPS (smooth animation without excessive CPU)
 */
UINT GetTimerInterval(void) {
    if (GetActiveShowMilliseconds()) {
        return 20;
    }

    /* Check for animated gradient */
    if (IsActiveTimerColorAnimated()) {
        return 66; /* 15 FPS - sufficient for smooth gradient animation */
    }

    if (CLOCK_SHOW_CURRENT_TIME && GetActiveShowSeconds()) {
        return 250;
    }

    /* Active timers still tick internally at 10 FPS so completion actions and
     * Pomodoro transitions are not delayed by a full second. Rendering remains
     * second-based when milliseconds are hidden. */
    if (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || CLOCK_TOTAL_TIME > 0)) {
        return 100;
    }

    return 1000;
}

/**
 * @brief Reset timer with appropriate interval
 * Uses high-precision multimedia timer for smooth milliseconds display
 */
void ResetTimerWithInterval(HWND hwnd) {
    UINT interval = GetTimerInterval();

    if (ShouldRunMainTimer(hwnd)) {
        /* Unified main timer start path keeps SetTimer/mmTimer behavior consistent */
        if (!MainTimer_Start(hwnd, interval)) {
            LOG_WARNING("Failed to reset main timer with interval %u; pausing active timer state",
                        interval);
            if (IsRunningCountUpTimer() || IsRunningCountdownTimer()) {
                CLOCK_IS_PAUSED = true;
            }
        }
    } else {
        MainTimer_Stop();
    }
    
    ResetTimerMilliseconds();
}


/**
 * @brief Update startup mode configuration
 */
BOOL WriteConfigStartupMode(const char* mode) {
    if (!mode || !*mode) {
        return FALSE;
    }

    char normalizedMode[sizeof(CLOCK_STARTUP_MODE)];
    strncpy(normalizedMode, mode, sizeof(normalizedMode) - 1);
    normalizedMode[sizeof(normalizedMode) - 1] = '\0';

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentValue[sizeof(CLOCK_STARTUP_MODE)] = {0};
    ReadIniString(INI_SECTION_TIMER, "STARTUP_MODE", "SHOW_TIME",
                  currentValue, sizeof(currentValue), config_path);

    BOOL runtimeMatches = (strcmp(CLOCK_STARTUP_MODE, normalizedMode) == 0);
    BOOL configMatches = (strcmp(currentValue, normalizedMode) == 0);
    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    if (!configMatches &&
        !WriteIniString(INI_SECTION_TIMER, "STARTUP_MODE", normalizedMode, config_path)) {
        return FALSE;
    }

    UpdateStartupModeBuffer(normalizedMode);
    return TRUE;
}


/**
 * @brief Write arbitrary key-value pair to appropriate config section
 */
BOOL WriteConfigKeyValue(const char* key, const char* value) {
    if (!key || !value) return FALSE;
    
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
           strcmp(key, WINDOW_POSITION_MANUAL_KEY) == 0 ||
           strcmp(key, WINDOW_MONITOR_ID_KEY) == 0 ||
           strcmp(key, WINDOW_MONITOR_OFFSET_X_KEY) == 0 ||
           strcmp(key, WINDOW_MONITOR_OFFSET_Y_KEY) == 0 ||
           strcmp(key, WINDOW_TASKBAR_ANCHORED_KEY) == 0 ||
           strcmp(key, WINDOW_TASKBAR_AXIS_RATIO_KEY) == 0 ||
           strcmp(key, WINDOW_TASKBAR_CROSS_OFFSET_KEY) == 0 ||
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
    
    return WriteIniString(section, key, value, config_path);
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

    char normalStepStr[32];
    char fastStepStr[32];
    if (snprintf(normalStepStr, sizeof(normalStepStr), "%d", normal_step) < 0 ||
        snprintf(fastStepStr, sizeof(fastStepStr), "%d", fast_step) < 0) {
        return;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentNormal[32] = {0};
    char currentFast[32] = {0};
    ReadIniString(INI_SECTION_DISPLAY, "OPACITY_STEP_NORMAL", "",
                  currentNormal, sizeof(currentNormal), config_path);
    ReadIniString(INI_SECTION_DISPLAY, "OPACITY_STEP_FAST", "",
                  currentFast, sizeof(currentFast), config_path);

    BOOL runtimeMatches =
        g_AppConfig.display.opacity_step_normal == normal_step &&
        g_AppConfig.display.opacity_step_fast == fast_step;
    BOOL configMatches =
        strcmp(currentNormal, normalStepStr) == 0 &&
        strcmp(currentFast, fastStepStr) == 0;
    if (runtimeMatches && configMatches) {
        return;
    }

    const IniKeyValue updates[] = {
        {INI_SECTION_DISPLAY, "OPACITY_STEP_NORMAL", normalStepStr},
        {INI_SECTION_DISPLAY, "OPACITY_STEP_FAST", fastStepStr},
    };
    if (!configMatches &&
        !WriteIniMultipleAtomic(config_path, updates, sizeof(updates) / sizeof(updates[0]))) {
        return;
    }

    g_AppConfig.display.opacity_step_normal = normal_step;
    g_AppConfig.display.opacity_step_fast = fast_step;
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
