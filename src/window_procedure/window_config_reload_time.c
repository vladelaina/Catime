/**
 * @file window_config_reload_time.c
 * @brief Parses countdown and Pomodoro reload options.
 */

#include "window_procedure/window_config_handlers_internal.h"
#include "config.h"
#include "config/config_defaults.h"
#include "log.h"
#include "timer/timer.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static BOOL ParsePositiveSecondsToken(const char* token, int* seconds) {
    if (!token || !seconds) return FALSE;

    while (isspace((unsigned char)*token)) token++;
    if (*token == '\0') return FALSE;

    errno = 0;
    char* end = NULL;
    long parsed = strtol(token, &end, 10);
    if (end == token || errno == ERANGE ||
        parsed <= 0 || parsed > MAX_TIME_OPTION_SECONDS) {
        return FALSE;
    }

    while (end && isspace((unsigned char)*end)) end++;
    if (end && *end != '\0') return FALSE;

    *seconds = (int)parsed;
    return TRUE;
}

BOOL WindowConfigInternal_ParseQuickCountdownOptions(char* optionsStr,
                                                int* parsedOptions,
                                                int* parsedCount) {
    if (!optionsStr || !parsedOptions || !parsedCount) {
        return FALSE;
    }

    *parsedCount = 0;
    char* cursor = optionsStr;
    while (cursor) {
        char* next = strchr(cursor, ',');
        if (next) {
            *next = '\0';
            next++;
        }

        if (*parsedCount >= MAX_TIME_OPTIONS) {
            WriteLog(LOG_LEVEL_WARNING,
                     "Too many countdown presets during reload; maximum is %d",
                     MAX_TIME_OPTIONS);
            return FALSE;
        }

        int seconds = 0;
        if (!ParsePositiveSecondsToken(cursor, &seconds)) {
            WriteLog(LOG_LEVEL_WARNING,
                     "Invalid countdown preset during reload: '%s'", cursor);
            return FALSE;
        }

        parsedOptions[*parsedCount] = seconds;
        (*parsedCount)++;
        cursor = next;
    }

    return *parsedCount > 0;
}
BOOL WindowConfigInternal_ParsePomodoroTimeOptions(char* optionsStr,
                                              int* parsedOptions,
                                              int* parsedCount) {
    if (!optionsStr || !parsedOptions || !parsedCount) {
        return FALSE;
    }

    *parsedCount = 0;
    char* cursor = optionsStr;
    while (cursor) {
        char* next = strchr(cursor, ',');
        if (next) {
            *next = '\0';
            next++;
        }

        if (*parsedCount >= MAX_POMODORO_TIMES) {
            WriteLog(LOG_LEVEL_WARNING,
                     "Too many Pomodoro intervals during reload; maximum is %d",
                     MAX_POMODORO_TIMES);
            return FALSE;
        }

        int seconds = 0;
        if (!ParsePositiveSecondsToken(cursor, &seconds)) {
            WriteLog(LOG_LEVEL_WARNING,
                     "Invalid Pomodoro interval during reload: '%s'", cursor);
            return FALSE;
        }

        parsedOptions[*parsedCount] = seconds;
        (*parsedCount)++;
        cursor = next;
    }

    return *parsedCount > 0;
}
