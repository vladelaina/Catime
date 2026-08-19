/**
 * @file window_config_reload_values.c
 * @brief Validates and reloads shared scalar configuration values.
 */

#include "window_procedure/window_config_handlers_internal.h"
#include "color/color_parser.h"
#include "config.h"
#include "config/config_defaults.h"
#include "log.h"
#include "notification.h"
#include "window.h"
#include "window_procedure/window_utils.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NOTIFICATION_MAX_WINDOW_HEIGHT 900
#define MIN_BASE_FONT_SIZE 8
#define MAX_BASE_FONT_SIZE 500
#define MIN_DEFAULT_START_TIME_SECONDS 1
#define MAX_DEFAULT_START_TIME_SECONDS 86400

BOOL WindowConfigInternal_ReadStringExact(const char* section, const char* key,
                                     const char* def, char* target,
                                     DWORD targetSize) {
    if (!target || targetSize == 0) return FALSE;

    if (!ReadIniStringExact(section, key, def ? def : "", target, targetSize,
                            GetCachedConfigPath())) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Hot reload ignored %s.%s because the config value is too long",
                 section ? section : "(null)",
                 key ? key : "(null)");
        return FALSE;
    }

    return TRUE;
}

BOOL WindowConfigInternal_LoadAndCompareString(const char* section, const char* key,
                                  char* target, size_t size, const char* def) {
    if (!target || size == 0 || size > MAX_PATH) return FALSE;

    char temp[MAX_PATH] = {0};
    if (!WindowConfigInternal_ReadStringExact(section, key, def, temp, (DWORD)size)) {
        return FALSE;
    }

    if (strcmp(temp, target) != 0) {
        strncpy_s(target, size, temp, _TRUNCATE);
        return TRUE;
    }
    return FALSE;
}

BOOL WindowConfigInternal_LoadAndCompareBool(const char* section, const char* key, bool* target, bool def) {
    BOOL temp = ReadConfigBool(section, key, def);
    if (temp != *target) {
        *target = temp;
        return TRUE;
    }
    return FALSE;
}

int WindowConfigInternal_NormalizeBaseFontSize(int fontSize) {
    if (fontSize < MIN_BASE_FONT_SIZE || fontSize > MAX_BASE_FONT_SIZE) {
        WriteLog(LOG_LEVEL_WARNING, "Ignoring invalid hot-reload font size %d, using default %d",
                 fontSize, DEFAULT_FONT_SIZE);
        return DEFAULT_FONT_SIZE;
    }
    return fontSize;
}

int WindowConfigInternal_NormalizeDefaultStartTime(int seconds) {
    if (seconds < MIN_DEFAULT_START_TIME_SECONDS ||
        seconds > MAX_DEFAULT_START_TIME_SECONDS) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Ignoring invalid hot-reload default start time %d, using default %d",
                 seconds, DEFAULT_START_TIME_SECONDS);
        return DEFAULT_START_TIME_SECONDS;
    }
    return seconds;
}

int WindowConfigInternal_ClampInt(const char* key, int value, int minValue, int maxValue) {
    if (value < minValue) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Clamping invalid hot-reload %s value %d to %d",
                 key ? key : "config", value, minValue);
        return minValue;
    }
    if (value > maxValue) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Clamping invalid hot-reload %s value %d to %d",
                 key ? key : "config", value, maxValue);
        return maxValue;
    }
    return value;
}

void WindowConfigInternal_NormalizeTextColor(const char* color,
                                           char* output,
                                           size_t outputSize) {
    if (!output || outputSize == 0) return;

    const char* fallback = DEFAULT_TEXT_COLOR;
    output[0] = '\0';

    if (NormalizeColorConfigValue(color, output, outputSize)) {
        return;
    }

    WriteLog(LOG_LEVEL_WARNING,
             "Ignoring invalid hot-reload text color '%s', using default '%s'",
             color ? color : "", fallback);
    strncpy_s(output, outputSize, fallback, _TRUNCATE);
}

static BOOL IsValidStartupModeConfig(const char* mode) {
    if (!mode) return FALSE;

    return strcmp(mode, "COUNTDOWN") == 0 ||
           strcmp(mode, "DEFAULT") == 0 ||
           strcmp(mode, "COUNT_UP") == 0 ||
           strcmp(mode, "SHOW_TIME") == 0 ||
           strcmp(mode, "NO_DISPLAY") == 0 ||
           strcmp(mode, "POMODORO") == 0;
}

void WindowConfigInternal_NormalizeStartupMode(const char* mode,
                                             char* output,
                                             size_t outputSize) {
    if (!output || outputSize == 0) return;

    if (IsValidStartupModeConfig(mode)) {
        strncpy_s(output, outputSize, mode, _TRUNCATE);
        return;
    }

    WriteLog(LOG_LEVEL_WARNING,
             "Ignoring invalid hot-reload startup mode '%s', using SHOW_TIME",
             mode ? mode : "");
    strncpy_s(output, outputSize, "SHOW_TIME", _TRUNCATE);
}

BOOL WindowConfigInternal_ParseScaleFactor(const char* text, float* scale) {
    if (!text || !scale) return FALSE;

    while (isspace((unsigned char)*text)) text++;
    if (*text == '\0') return FALSE;

    errno = 0;
    char* end = NULL;
    float parsed = strtof(text, &end);
    if (end == text || errno == ERANGE || !isfinite(parsed) ||
        parsed < MIN_SCALE_FACTOR || parsed > MAX_SCALE_FACTOR) {
        return FALSE;
    }

    while (end && isspace((unsigned char)*end)) end++;
    if (end && *end != '\0') return FALSE;

    *scale = parsed;
    return TRUE;
}

int WindowConfigInternal_ClampNotificationWidth(int width) {
    if (width <= 0) return 0;
    if (width < NOTIFICATION_MIN_WIDTH) return NOTIFICATION_MIN_WIDTH;
    if (width > NOTIFICATION_MAX_WIDTH) return NOTIFICATION_MAX_WIDTH;
    return width;
}

int WindowConfigInternal_ClampNotificationHeight(int height) {
    if (height <= 0) return 0;
    if (height < NOTIFICATION_MIN_HEIGHT) return NOTIFICATION_MIN_HEIGHT;
    if (height > NOTIFICATION_MAX_WINDOW_HEIGHT) return NOTIFICATION_MAX_WINDOW_HEIGHT;
    return height;
}
