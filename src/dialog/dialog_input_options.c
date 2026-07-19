/**
 * @file dialog_input_options.c
 * @brief Quick countdown preset parsing kept separate from window code.
 */

#include "dialog/dialog_input_options.h"
#include "dialog/dialog_common.h"
#include "config.h"
#include "utils/time_parser.h"
#include <stdio.h>
#include <string.h>
#include <strsafe.h>

#define QUICK_TIME_OPTIONS_TOKEN_DELIMITERS " \t\r\n"

static BOOL AppendText(char* dest, size_t destSize, const char* suffix) {
    if (!dest || destSize == 0 || !suffix) return FALSE;
    return SUCCEEDED(StringCbCatA(dest, destSize, suffix));
}

BOOL DialogInputOptions_BuildDisplay(char* dest, size_t destSize) {
    if (!dest || destSize == 0) return FALSE;

    dest[0] = '\0';
    int optionCount = time_options_count;
    if (optionCount < 0) optionCount = 0;
    if (optionCount > MAX_TIME_OPTIONS) optionCount = MAX_TIME_OPTIONS;

    int appendedCount = 0;
    for (int i = 0; i < optionCount; i++) {
        if (time_options[i] <= 0 ||
            time_options[i] > MAX_TIME_OPTION_SECONDS) {
            return FALSE;
        }

        char timeText[32] = {0};
        Dialog_FormatSecondsToString(time_options[i], timeText,
                                     sizeof(timeText));
        if (timeText[0] == '\0') return FALSE;

        if (appendedCount > 0 && !AppendText(dest, destSize, " ")) {
            return FALSE;
        }
        if (!AppendText(dest, destSize, timeText)) return FALSE;
        appendedCount++;
    }

    return TRUE;
}

BOOL DialogInputOptions_ParseConfig(char* inputUtf8, char* options,
                                    size_t optionsSize, int* parsedSeconds,
                                    int* parsedCount) {
    if (!inputUtf8 || !options || optionsSize == 0 ||
        !parsedSeconds || !parsedCount) {
        return FALSE;
    }

    options[0] = '\0';
    *parsedCount = 0;
    const char* token = strtok(inputUtf8, QUICK_TIME_OPTIONS_TOKEN_DELIMITERS);
    while (token) {
        if (*parsedCount >= MAX_TIME_OPTIONS) return FALSE;

        int seconds = 0;
        if (!TimeParser_ParseBasic(token, &seconds) || seconds <= 0 ||
            seconds > MAX_TIME_OPTION_SECONDS) {
            return FALSE;
        }

        char secondsText[32] = {0};
        int written = snprintf(secondsText, sizeof(secondsText), "%d", seconds);
        if (written < 0 || (size_t)written >= sizeof(secondsText)) {
            return FALSE;
        }

        if (*parsedCount > 0 && !AppendText(options, optionsSize, ",")) {
            return FALSE;
        }
        if (!AppendText(options, optionsSize, secondsText)) return FALSE;

        parsedSeconds[*parsedCount] = seconds;
        (*parsedCount)++;
        token = strtok(NULL, QUICK_TIME_OPTIONS_TOKEN_DELIMITERS);
    }

    return *parsedCount > 0;
}
