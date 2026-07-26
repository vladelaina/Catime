/**
 * @file window_config_handlers_timer.c
 * @brief Reloads timer-related configuration.
 */

#include "window_procedure/window_config_handlers_internal.h"
#include "config.h"
#include "config/config_defaults.h"
#include "log.h"
#include "timer/main_timer.h"
#include "timer/timer.h"
#include "window_procedure/window_utils.h"

#include <string.h>

extern char CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH];

LRESULT HandleAppTimerChanged(HWND hwnd) {
    bool changed = false;
    bool intervalChanged = false;

    /* Basic timer settings */
    changed |= WindowConfigInternal_LoadAndCompareBool(CFG_SECTION_TIMER, CFG_KEY_USE_24HOUR,
                                  &CLOCK_USE_24HOUR, CLOCK_USE_24HOUR);
    intervalChanged = WindowConfigInternal_LoadAndCompareBool(CFG_SECTION_TIMER, CFG_KEY_SHOW_SECONDS,
                                         &CLOCK_SHOW_SECONDS, CLOCK_SHOW_SECONDS);
    changed |= intervalChanged;

    /* Time format */
    char formatBuf[32];
    ReadConfigStr(CFG_SECTION_TIMER, CFG_KEY_TIME_FORMAT, "DEFAULT", formatBuf, sizeof(formatBuf));
    TimeFormatType newFormat = TimeFormatType_FromStr(formatBuf);
    if (newFormat != g_AppConfig.display.time_format.format) {
        g_AppConfig.display.time_format.format = newFormat;
        changed = true;
    }

    /* Milliseconds display */
    BOOL newShowMs = ReadConfigBool(CFG_SECTION_TIMER, CFG_KEY_SHOW_MILLISECONDS, FALSE);
    if (newShowMs != g_AppConfig.display.time_format.show_milliseconds) {
        g_AppConfig.display.time_format.show_milliseconds = newShowMs;
        MainTimer_Stop();
        ResetTimerWithInterval(hwnd);
        changed = true;
    }

    /* Timeout settings */
    WindowConfigInternal_LoadAndCompareString(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_TEXT,
                         CLOCK_TIMEOUT_TEXT, sizeof(CLOCK_TIMEOUT_TEXT), "0");

    /* Timeout action (preserve one-time actions) */
    if (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHUTDOWN &&
        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_RESTART &&
        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SLEEP) {
        char actionBuf[32];
        ReadConfigStr(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_ACTION, "MESSAGE", actionBuf, sizeof(actionBuf));
        TimeoutActionType newAction = TimeoutActionType_FromStr(actionBuf);
        if (newAction != CLOCK_TIMEOUT_ACTION) {
            CLOCK_TIMEOUT_ACTION = newAction;
        }
    }

    /* Timeout file and website */
    WindowConfigInternal_LoadAndCompareString(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_FILE,
                         CLOCK_TIMEOUT_FILE_PATH, sizeof(CLOCK_TIMEOUT_FILE_PATH), "");

    WindowConfigInternal_LoadAndCompareString(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_WEBSITE,
                         CLOCK_TIMEOUT_WEBSITE_URL, sizeof(CLOCK_TIMEOUT_WEBSITE_URL), "");

    /* Default start time */
    int newDefaultStartTime = WindowConfigInternal_NormalizeDefaultStartTime(
        ReadConfigInt(CFG_SECTION_TIMER, CFG_KEY_DEFAULT_START_TIME,
                      g_AppConfig.timer.default_start_time));
    if (newDefaultStartTime != g_AppConfig.timer.default_start_time) {
        g_AppConfig.timer.default_start_time = newDefaultStartTime;
    }

    /* Time options */
    char optionsBuf[TIME_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    BOOL optionsComplete = ReadIniStringExact(CFG_SECTION_TIMER, CFG_KEY_TIME_OPTIONS,
                                              DEFAULT_TIME_OPTIONS_INI, optionsBuf,
                                              sizeof(optionsBuf), GetCachedConfigPath());
    int newArr[MAX_TIME_OPTIONS] = {0};
    int newCnt = 0;
    if (!optionsComplete) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Countdown presets config is too long during reload; keeping current presets");
    } else if (WindowConfigInternal_ParseQuickCountdownOptions(optionsBuf, newArr, &newCnt) &&
               (newCnt != time_options_count ||
                memcmp(newArr, time_options, (size_t)newCnt * sizeof(int)) != 0)) {
        ZeroMemory(time_options, sizeof(time_options));
        time_options_count = newCnt;
        memcpy(time_options, newArr, (size_t)newCnt * sizeof(int));
    }

    /* Startup mode */
    char startupModeBuf[sizeof(CLOCK_STARTUP_MODE)] = {0};
    ReadConfigStr(CFG_SECTION_TIMER, CFG_KEY_STARTUP_MODE, CLOCK_STARTUP_MODE,
                  startupModeBuf, sizeof(startupModeBuf));

    char normalizedStartupMode[sizeof(CLOCK_STARTUP_MODE)] = {0};
    WindowConfigInternal_NormalizeStartupMode(startupModeBuf, normalizedStartupMode,
                                     sizeof(normalizedStartupMode));
    if (strcmp(normalizedStartupMode, CLOCK_STARTUP_MODE) != 0) {
        strncpy_s(CLOCK_STARTUP_MODE, sizeof(CLOCK_STARTUP_MODE),
                  normalizedStartupMode, _TRUNCATE);
    }

    if (changed) {
        if (intervalChanged) {
            ResetTimerWithInterval(hwnd);
        }
        InvalidateRect(hwnd, NULL, TRUE);
    }

    return 0;
}
