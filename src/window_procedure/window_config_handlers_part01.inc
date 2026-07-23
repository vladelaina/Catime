/**
 * @file window_config_handlers.c
 * @brief Configuration reload handlers implementation (simplified)
 *
 * Handles WM_APP_*_CHANGED messages to reload configuration from INI file.
 * Each handler directly reads and applies configuration changes.
 */

#include "window_procedure/window_config_handlers.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_hotkeys.h"
#include "config.h"
#include "config/config_defaults.h"
#include "config/config_watcher.h"
#include "text_effect.h"
#include "timer/timer.h"
#include "timer/main_timer.h"
#include "window.h"
#include "drawing/drawing_effect.h"
#include "font.h"
#include "color/color.h"
#include "color/color_parser.h"
#include "color/gradient.h"
#include "tray/tray_animation_core.h"
#include "notification.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];
extern char CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH];

#define NOTIFICATION_MAX_WINDOW_HEIGHT 900
#define MIN_BASE_FONT_SIZE 8
#define MAX_BASE_FONT_SIZE 500
#define MIN_DEFAULT_START_TIME_SECONDS 1
#define MAX_DEFAULT_START_TIME_SECONDS 86400
#define COLOR_OPTIONS_CONFIG_BUFFER 2048
#define PERCENT_ICON_COLOR_CONFIG_BUFFER 32

typedef struct {
    BOOL colorOptionsValid;
    char colorOptions[COLOR_OPTIONS_CONFIG_BUFFER];
    char percentIconTextColor[PERCENT_ICON_COLOR_CONFIG_BUFFER];
    char percentIconBgColor[PERCENT_ICON_COLOR_CONFIG_BUFFER];
} HotReloadColorConfig;

static BOOL g_hotReloadColorConfigValid = FALSE;
static HotReloadColorConfig g_lastHotReloadColorConfig = {0};

typedef struct {
    char recentFiles[MAX_RECENT_FILES][MAX_PATH];
    TimeoutActionType timeoutAction;
    char timeoutFile[MAX_PATH];
} HotReloadRecentFilesConfig;

static BOOL g_hotReloadRecentFilesConfigValid = FALSE;
static HotReloadRecentFilesConfig g_lastHotReloadRecentFilesConfig = {0};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static BOOL ReadHotReloadStringExact(const char* section, const char* key,
                                     const char* def, char* target,
                                     DWORD targetSize);

static BOOL LoadAndCompareString(const char* section, const char* key,
                                  char* target, size_t size, const char* def) {
    if (!target || size == 0 || size > MAX_PATH) return FALSE;

    char temp[MAX_PATH] = {0};
    if (!ReadHotReloadStringExact(section, key, def, temp, (DWORD)size)) {
        return FALSE;
    }

    if (strcmp(temp, target) != 0) {
        strncpy_s(target, size, temp, _TRUNCATE);
        return TRUE;
    }
    return FALSE;
}

static BOOL ReadHotReloadStringExact(const char* section, const char* key,
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

static BOOL LoadAndCompareBool(const char* section, const char* key, bool* target, bool def) {
    BOOL temp = ReadConfigBool(section, key, def);
    if (temp != *target) {
        *target = temp;
        return TRUE;
    }
    return FALSE;
}

static void ReadHotReloadColorConfig(HotReloadColorConfig* config) {
    if (!config) return;

    config->colorOptionsValid = ReadIniStringExact(
        CFG_SECTION_COLORS, "COLOR_OPTIONS", DEFAULT_COLOR_OPTIONS_INI,
        config->colorOptions, sizeof(config->colorOptions), GetCachedConfigPath());
    if (!config->colorOptionsValid) {
        config->colorOptions[0] = '\0';
        LOG_WARNING("Hot reload ignored COLOR_OPTIONS because the config value is too long");
    }
    ReadConfigStr("Animation", "PERCENT_ICON_TEXT_COLOR", "auto",
                  config->percentIconTextColor, sizeof(config->percentIconTextColor));
    ReadConfigStr("Animation", "PERCENT_ICON_BG_COLOR", "transparent",
                  config->percentIconBgColor, sizeof(config->percentIconBgColor));
}

static BOOL HotReloadColorOptionsChanged(const HotReloadColorConfig* config) {
    if (!config || !config->colorOptionsValid) return FALSE;
    return !g_hotReloadColorConfigValid ||
           strcmp(config->colorOptions, g_lastHotReloadColorConfig.colorOptions) != 0;
}

static BOOL HotReloadPercentIconColorsChanged(const HotReloadColorConfig* config) {
    return !g_hotReloadColorConfigValid ||
           strcmp(config->percentIconTextColor,
                  g_lastHotReloadColorConfig.percentIconTextColor) != 0 ||
           strcmp(config->percentIconBgColor,
                  g_lastHotReloadColorConfig.percentIconBgColor) != 0;
}

static void RememberHotReloadColorConfig(const HotReloadColorConfig* config) {
    if (!config) return;

    if (config->colorOptionsValid) {
        g_lastHotReloadColorConfig.colorOptionsValid = TRUE;
        memcpy(g_lastHotReloadColorConfig.colorOptions,
               config->colorOptions,
               sizeof(g_lastHotReloadColorConfig.colorOptions));
    } else if (!g_hotReloadColorConfigValid) {
        g_lastHotReloadColorConfig.colorOptionsValid = FALSE;
        g_lastHotReloadColorConfig.colorOptions[0] = '\0';
    }
    memcpy(g_lastHotReloadColorConfig.percentIconTextColor,
           config->percentIconTextColor,
           sizeof(g_lastHotReloadColorConfig.percentIconTextColor));
    memcpy(g_lastHotReloadColorConfig.percentIconBgColor,
           config->percentIconBgColor,
           sizeof(g_lastHotReloadColorConfig.percentIconBgColor));
    g_hotReloadColorConfigValid = TRUE;
}

static void ReadHotReloadRecentFilesConfig(HotReloadRecentFilesConfig* config) {
    if (!config) return;

    ZeroMemory(config, sizeof(*config));
    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i + 1);
        if (!ReadHotReloadStringExact(INI_SECTION_RECENTFILES, key, "",
                                      config->recentFiles[i],
                                      sizeof(config->recentFiles[i]))) {
            config->recentFiles[i][0] = '\0';
        }
    }

    config->timeoutAction = CLOCK_TIMEOUT_ACTION;
    strncpy_s(config->timeoutFile, sizeof(config->timeoutFile),
              CLOCK_TIMEOUT_FILE_PATH, _TRUNCATE);
}

static BOOL HotReloadRecentFilesChanged(const HotReloadRecentFilesConfig* config) {
    if (!config || !g_hotReloadRecentFilesConfigValid) {
        return TRUE;
    }

    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        if (strcmp(config->recentFiles[i],
                   g_lastHotReloadRecentFilesConfig.recentFiles[i]) != 0) {
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL HotReloadRecentValidationInputChanged(const HotReloadRecentFilesConfig* config) {
    return !config ||
           !g_hotReloadRecentFilesConfigValid ||
           config->timeoutAction != g_lastHotReloadRecentFilesConfig.timeoutAction ||
           strcmp(config->timeoutFile, g_lastHotReloadRecentFilesConfig.timeoutFile) != 0;
}

static void RememberHotReloadRecentFilesConfig(HotReloadRecentFilesConfig* config) {
    if (!config) return;

    config->timeoutAction = CLOCK_TIMEOUT_ACTION;
    strncpy_s(config->timeoutFile, sizeof(config->timeoutFile),
              CLOCK_TIMEOUT_FILE_PATH, _TRUNCATE);
    g_lastHotReloadRecentFilesConfig = *config;
    g_hotReloadRecentFilesConfigValid = TRUE;
}

static int NormalizeBaseFontSize(int fontSize) {
    if (fontSize < MIN_BASE_FONT_SIZE || fontSize > MAX_BASE_FONT_SIZE) {
        WriteLog(LOG_LEVEL_WARNING, "Ignoring invalid hot-reload font size %d, using default %d",
                 fontSize, DEFAULT_FONT_SIZE);
        return DEFAULT_FONT_SIZE;
    }
    return fontSize;
}

static int NormalizeDefaultStartTime(int seconds) {
    if (seconds < MIN_DEFAULT_START_TIME_SECONDS ||
        seconds > MAX_DEFAULT_START_TIME_SECONDS) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Ignoring invalid hot-reload default start time %d, using default %d",
                 seconds, DEFAULT_START_TIME_SECONDS);
        return DEFAULT_START_TIME_SECONDS;
    }
    return seconds;
}

static int ClampHotReloadInt(const char* key, int value, int minValue, int maxValue) {
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

static void NormalizeTextColorForHotReload(const char* color,
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
