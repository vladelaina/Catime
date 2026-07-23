/**
 * @file config_writer_collect_timer.c
 * @brief Timer and Pomodoro configuration collection
 */
#include "config_writer_internal.h"

#include "config.h"
#include "timer/timer.h"

static const char* TimeFormatToString(TimeFormatType format) {
    switch (format) {
        case TIME_FORMAT_ZERO_PADDED: return "ZERO_PADDED";
        case TIME_FORMAT_FULL_PADDED: return "FULL_PADDED";
        case TIME_FORMAT_DEFAULT:
        default: return "DEFAULT";
    }
}

static const char* TimeoutActionToString(TimeoutActionType action) {
    switch (action) {
        case TIMEOUT_ACTION_LOCK: return "LOCK";
        case TIMEOUT_ACTION_OPEN_FILE: return "OPEN_FILE";
        case TIMEOUT_ACTION_SHOW_TIME: return "SHOW_TIME";
        case TIMEOUT_ACTION_COUNT_UP: return "COUNT_UP";
        case TIMEOUT_ACTION_OPEN_WEBSITE: return "OPEN_WEBSITE";
        case TIMEOUT_ACTION_MESSAGE:
        case TIMEOUT_ACTION_SHUTDOWN:
        case TIMEOUT_ACTION_RESTART:
        case TIMEOUT_ACTION_SLEEP:
        default: return "MESSAGE";
    }
}

static BOOL CollectTimerValues(ConfigItemBuilder* builder) {
    if (!ConfigWriter_AppendInt(builder, INI_SECTION_TIMER,
                                "CLOCK_DEFAULT_START_TIME",
                                g_AppConfig.timer.default_start_time) ||
        !ConfigWriter_AppendBool(builder, INI_SECTION_TIMER,
                                 "CLOCK_USE_24HOUR", CLOCK_USE_24HOUR) ||
        !ConfigWriter_AppendBool(builder, INI_SECTION_TIMER,
                                 "CLOCK_SHOW_SECONDS", CLOCK_SHOW_SECONDS) ||
        !ConfigWriter_AppendString(
            builder, INI_SECTION_TIMER, "CLOCK_TIME_FORMAT",
            TimeFormatToString(g_AppConfig.display.time_format.format)) ||
        !ConfigWriter_AppendBool(
            builder, INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS",
            g_AppConfig.display.time_format.show_milliseconds)) {
        return FALSE;
    }

    return ConfigWriter_AppendString(builder, INI_SECTION_TIMER,
                                     "CLOCK_TIMEOUT_TEXT",
                                     CLOCK_TIMEOUT_TEXT) &&
           ConfigWriter_AppendString(builder, INI_SECTION_TIMER,
                                     "CLOCK_TIMEOUT_ACTION",
                                     TimeoutActionToString(
                                         CLOCK_TIMEOUT_ACTION)) &&
           ConfigWriter_AppendString(builder, INI_SECTION_TIMER,
                                     "CLOCK_TIMEOUT_FILE",
                                     CLOCK_TIMEOUT_FILE_PATH) &&
           ConfigWriter_AppendString(builder, INI_SECTION_TIMER,
                                     "CLOCK_TIMEOUT_WEBSITE",
                                     CLOCK_TIMEOUT_WEBSITE_URL);
}

BOOL ConfigWriter_CollectTimerPomodoro(ConfigItemBuilder* builder) {
    int timerOptionCount = time_options_count;
    int pomodoroTimeCount = g_AppConfig.pomodoro.times_count;

    if (timerOptionCount < 0) timerOptionCount = 0;
    if (timerOptionCount > MAX_TIME_OPTIONS) {
        timerOptionCount = MAX_TIME_OPTIONS;
    }
    if (pomodoroTimeCount < 0) pomodoroTimeCount = 0;
    if (pomodoroTimeCount > (int)_countof(g_AppConfig.pomodoro.times)) {
        pomodoroTimeCount = (int)_countof(g_AppConfig.pomodoro.times);
    }

    if (!CollectTimerValues(builder) ||
        !ConfigWriter_AppendIntList(builder, INI_SECTION_TIMER,
                                    "CLOCK_TIME_OPTIONS", time_options,
                                    timerOptionCount,
                                    "CLOCK_TIME_OPTIONS") ||
        !ConfigWriter_AppendString(builder, INI_SECTION_TIMER,
                                   "STARTUP_MODE", CLOCK_STARTUP_MODE) ||
        !ConfigWriter_AppendIntList(
            builder, INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS",
            g_AppConfig.pomodoro.times, pomodoroTimeCount,
            "POMODORO_TIME_OPTIONS") ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_POMODORO,
                                "POMODORO_LOOP_COUNT",
                                g_AppConfig.pomodoro.loop_count)) {
        return FALSE;
    }
    return TRUE;
}
