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

static void NormalizeStartupModeForHotReload(const char* mode,
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

static BOOL ResolveFontLoadNameForHotReload(const char* configFont,
                                            char* loadName,
                                            size_t loadNameSize) {
    if (!configFont || !*configFont || !loadName || loadNameSize == 0) {
        return FALSE;
    }

    const char* relativePath = ExtractRelativePath(configFont);
    const char* source = relativePath ? relativePath : configFont;
    if (!*source) {
        return FALSE;
    }

    strncpy_s(loadName, loadNameSize, source, _TRUNCATE);
    return TRUE;
}

static BOOL ApplyFontForHotReload(const char* configFont) {
    if (!configFont || !*configFont || strcmp(configFont, FONT_FILE_NAME) == 0) {
        return FALSE;
    }

    char loadName[MAX_PATH] = {0};
    if (!ResolveFontLoadNameForHotReload(configFont, loadName, sizeof(loadName))) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Ignoring invalid hot-reload font config value '%s'",
                 configFont);
        return FALSE;
    }

    char internalName[MAX_PATH] = {0};
    if (!LoadFontByNameAndGetRealName(GetModuleHandle(NULL), loadName,
                                      internalName, sizeof(internalName))) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Ignoring hot-reload font '%s' because it could not be loaded",
                 configFont);
        return FALSE;
    }

    strncpy_s(FONT_FILE_NAME, sizeof(FONT_FILE_NAME), configFont, _TRUNCATE);
    strncpy_s(FONT_RUNTIME_FILE_NAME, sizeof(FONT_RUNTIME_FILE_NAME),
              configFont, _TRUNCATE);
    strncpy_s(FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME), internalName, _TRUNCATE);
    return TRUE;
}

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

static BOOL ParseQuickCountdownOptionsForReload(char* optionsStr,
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

static BOOL ParsePomodoroTimeOptionsForReload(char* optionsStr,
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

static BOOL ParseScaleFactor(const char* text, float* scale) {
    if (!text || !scale) return FALSE;

    while (isspace((unsigned char)*text)) text++;
    if (*text == '\0') return FALSE;

    errno = 0;
    char* end = NULL;
    float parsed = strtof(text, &end);
    if (end == text || errno == ERANGE || !isfinite(parsed) ||
        parsed < MIN_SCALE_FACTOR) {
        return FALSE;
    }

    while (end && isspace((unsigned char)*end)) end++;
    if (end && *end != '\0') return FALSE;

    *scale = parsed;
    return TRUE;
}

static int ClampNotificationConfigWidth(int width) {
    if (width <= 0) return 0;
    if (width < NOTIFICATION_MIN_WIDTH) return NOTIFICATION_MIN_WIDTH;
    if (width > NOTIFICATION_MAX_WIDTH) return NOTIFICATION_MAX_WIDTH;
    return width;
}

static int ClampNotificationConfigHeight(int height) {
    if (height <= 0) return 0;
    if (height < NOTIFICATION_MIN_HEIGHT) return NOTIFICATION_MIN_HEIGHT;
    if (height > NOTIFICATION_MAX_WINDOW_HEIGHT) return NOTIFICATION_MAX_WINDOW_HEIGHT;
    return height;
}

/* ============================================================================
 * Handler Implementations
 * ============================================================================ */

LRESULT HandleAppConfigChanged(HWND hwnd) {
    ConfigWatcher_BeginConfigReloadHandling();
    HandleAppAnimSpeedChanged(hwnd);
    HandleAppAnimPathChanged(hwnd);
    HandleAppDisplayChanged(hwnd);
    HandleAppTimerChanged(hwnd);
    HandleAppPomodoroChanged(hwnd);
    HandleAppNotificationChanged(hwnd);
    HandleAppHotkeysChanged(hwnd);
    HandleAppRecentFilesChanged(hwnd);
    HandleAppColorsChanged(hwnd);
    ConfigWatcher_EndConfigReloadHandling(hwnd);
    return 0;
}

LRESULT HandleAppDisplayChanged(HWND hwnd) {
    BOOL changed = FALSE;
    BOOL textColorChanged = FALSE;

    /* Text color */
    char textColorBuf[COLOR_HEX_BUFFER];
    ReadConfigStr(CFG_SECTION_DISPLAY, CFG_KEY_TEXT_COLOR, CLOCK_TEXT_COLOR,
                  textColorBuf, sizeof(textColorBuf));

    char normalizedTextColor[COLOR_HEX_BUFFER];
    NormalizeTextColorForHotReload(textColorBuf, normalizedTextColor,
                                   sizeof(normalizedTextColor));
    if (strcmp(normalizedTextColor, CLOCK_TEXT_COLOR) != 0) {
        strncpy_s(CLOCK_TEXT_COLOR, sizeof(CLOCK_TEXT_COLOR),
                  normalizedTextColor, _TRUNCATE);
        changed = TRUE;
        textColorChanged = TRUE;
    }

    /* Font size */
    int newBaseFontSize = NormalizeBaseFontSize(
        ReadConfigInt(CFG_SECTION_DISPLAY, CFG_KEY_BASE_FONT_SIZE, CLOCK_BASE_FONT_SIZE));
    if (newBaseFontSize != CLOCK_BASE_FONT_SIZE) {
        CLOCK_BASE_FONT_SIZE = newBaseFontSize;
        changed = TRUE;
    }

    char fontConfigValue[MAX_PATH] = {0};
    if (ReadHotReloadStringExact(CFG_SECTION_DISPLAY, "FONT_FILE_NAME",
                                 FONTS_PATH_PREFIX DEFAULT_FONT_NAME,
                                 fontConfigValue, sizeof(fontConfigValue)) &&
        ApplyFontForHotReload(fontConfigValue)) {
        changed = TRUE;
    }

    /* Window settings (only if not in edit mode) */
    if (!CLOCK_EDIT_MODE) {
        int configPosX = ReadConfigInt(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_POS_X, CLOCK_WINDOW_POS_X);
        int posY = ReadConfigInt(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_POS_Y, CLOCK_WINDOW_POS_Y);
        int posX = configPosX;

        /* Skip position handling for special/default sentinels during hot-reload.
         * Reset/apply paths resolve them with finalized window dimensions. */
        BOOL skipPositionUpdate = (configPosX == -2 || configPosX == -1 ||
                                   posY == DEFAULT_WINDOW_POS_Y ||
                                   IsSystemPositionChangeGuardActive());
        if (skipPositionUpdate) {
            posX = CLOCK_WINDOW_POS_X;
            posY = CLOCK_WINDOW_POS_Y;
        }

        char scaleStr[64];
        ReadConfigStr(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_SCALE, "1.62", scaleStr, sizeof(scaleStr));
        float newScale = CLOCK_WINDOW_SCALE;
        BOOL hasValidScale = ParseScaleFactor(scaleStr, &newScale);

        char pluginScaleStr[64];
        ReadConfigStr(CFG_SECTION_DISPLAY, "PLUGIN_SCALE", "1.0", pluginScaleStr, sizeof(pluginScaleStr));
        float newPluginScale = PLUGIN_FONT_SCALE_FACTOR;
        BOOL hasValidPluginScale = ParseScaleFactor(pluginScaleStr, &newPluginScale);

        BOOL newTopmost = ReadConfigBool(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_TOPMOST, CLOCK_WINDOW_TOPMOST);
        int newOpacity = ReadConfigInt(CFG_SECTION_DISPLAY, "WINDOW_OPACITY", CLOCK_WINDOW_OPACITY);

        BOOL posChanged = !skipPositionUpdate && ((posX != CLOCK_WINDOW_POS_X) || (posY != CLOCK_WINDOW_POS_Y));
        BOOL scaleChanged = hasValidScale && fabsf(newScale - CLOCK_WINDOW_SCALE) > 0.0001f;


        if (scaleChanged) {
            CLOCK_WINDOW_SCALE = newScale;
            CLOCK_FONT_SCALE_FACTOR = newScale;
            changed = TRUE;
        }

        if (hasValidPluginScale && fabsf(newPluginScale - PLUGIN_FONT_SCALE_FACTOR) > 0.0001f) {
            PLUGIN_FONT_SCALE_FACTOR = newPluginScale;
            changed = TRUE;
        }

        if (posChanged || scaleChanged) {
            int width = ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_WIDTH, CLOCK_WINDOW_SCALE);
            int height = ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_HEIGHT, CLOCK_WINDOW_SCALE);

            if (!posChanged) {
                RECT currentRect;
                if (GetWindowRect(hwnd, &currentRect)) {
                    posX = currentRect.left;
                    posY = currentRect.top;
                }
            }

            SetWindowPos(hwnd, NULL, posX, posY, width, height,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            if (posChanged) {
                CLOCK_WINDOW_POS_X = posX;
                CLOCK_WINDOW_POS_Y = posY;
            }
            changed = TRUE;
        }

        if (newTopmost != CLOCK_WINDOW_TOPMOST) {
            SetWindowTopmostFromConfig(hwnd, newTopmost);
            changed = TRUE;
        }

        if (newOpacity != CLOCK_WINDOW_OPACITY) {
            if (newOpacity < MIN_VISIBLE_OPACITY) newOpacity = MIN_VISIBLE_OPACITY;
            if (newOpacity > MAX_OPACITY) newOpacity = MAX_OPACITY;
            CLOCK_WINDOW_OPACITY = newOpacity;
            SetBlurBehind(hwnd, CLOCK_EDIT_MODE);
            changed = TRUE;
        }
    }

    g_AppConfig.display.move_step_small = ClampHotReloadInt(
        "MOVE_STEP_SMALL",
        ReadConfigInt(CFG_SECTION_DISPLAY, "MOVE_STEP_SMALL",
                      g_AppConfig.display.move_step_small),
        MIN_MOVE_STEP, MAX_MOVE_STEP);
    g_AppConfig.display.move_step_large = ClampHotReloadInt(
        "MOVE_STEP_LARGE",
        ReadConfigInt(CFG_SECTION_DISPLAY, "MOVE_STEP_LARGE",
                      g_AppConfig.display.move_step_large),
        MIN_MOVE_STEP, MAX_MOVE_STEP);
    g_AppConfig.display.opacity_step_normal = ClampHotReloadInt(
        "OPACITY_STEP_NORMAL",
        ReadConfigInt(CFG_SECTION_DISPLAY, "OPACITY_STEP_NORMAL",
                      g_AppConfig.display.opacity_step_normal),
        MIN_OPACITY, MAX_OPACITY);
    g_AppConfig.display.opacity_step_fast = ClampHotReloadInt(
        "OPACITY_STEP_FAST",
        ReadConfigInt(CFG_SECTION_DISPLAY, "OPACITY_STEP_FAST",
                      g_AppConfig.display.opacity_step_fast),
        MIN_OPACITY, MAX_OPACITY);
    g_AppConfig.display.scale_step_normal = ClampHotReloadInt(
        "SCALE_STEP_NORMAL",
        ReadConfigInt(CFG_SECTION_DISPLAY, "SCALE_STEP_NORMAL",
                      g_AppConfig.display.scale_step_normal),
        MIN_OPACITY, MAX_OPACITY);
    g_AppConfig.display.scale_step_fast = ClampHotReloadInt(
        "SCALE_STEP_FAST",
        ReadConfigInt(CFG_SECTION_DISPLAY, "SCALE_STEP_FAST",
                      g_AppConfig.display.scale_step_fast),
        MIN_OPACITY, MAX_OPACITY);

    char effectBuf[32];
    ReadConfigStr(CFG_SECTION_DISPLAY, "TEXT_EFFECT", "NONE", effectBuf, sizeof(effectBuf));
    TextEffectType previousTextEffect = CLOCK_TEXT_EFFECT;
    TextEffectType newTextEffect = TextEffect_FromConfigString(effectBuf);
    if (newTextEffect != previousTextEffect) {
        CLOCK_TEXT_EFFECT = newTextEffect;
        g_AppConfig.display.text_effect = newTextEffect;
        if (TextEffect_UsesSharedEffectBuffer(previousTextEffect) &&
            !TextEffect_UsesSharedEffectBuffer(newTextEffect)) {
            CleanupDrawingEffects();
        }
        changed = TRUE;
    }

    if (changed) {
        ResetTimerWithInterval(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
    }
    if (textColorChanged) {
        RefreshToastNotificationColors();
    }

    return 0;
}

LRESULT HandleAppTimerChanged(HWND hwnd) {
    bool changed = false;

    /* Basic timer settings */
    changed |= LoadAndCompareBool(CFG_SECTION_TIMER, CFG_KEY_USE_24HOUR,
                                  &CLOCK_USE_24HOUR, CLOCK_USE_24HOUR);
    changed |= LoadAndCompareBool(CFG_SECTION_TIMER, CFG_KEY_SHOW_SECONDS,
                                  &CLOCK_SHOW_SECONDS, CLOCK_SHOW_SECONDS);

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
    LoadAndCompareString(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_TEXT,
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
    LoadAndCompareString(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_FILE,
                         CLOCK_TIMEOUT_FILE_PATH, sizeof(CLOCK_TIMEOUT_FILE_PATH), "");

    LoadAndCompareString(CFG_SECTION_TIMER, CFG_KEY_TIMEOUT_WEBSITE,
                         CLOCK_TIMEOUT_WEBSITE_URL, sizeof(CLOCK_TIMEOUT_WEBSITE_URL), "");

    /* Default start time */
    int newDefaultStartTime = NormalizeDefaultStartTime(
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
    } else if (ParseQuickCountdownOptionsForReload(optionsBuf, newArr, &newCnt) &&
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
    NormalizeStartupModeForHotReload(startupModeBuf, normalizedStartupMode,
                                     sizeof(normalizedStartupMode));
    if (strcmp(normalizedStartupMode, CLOCK_STARTUP_MODE) != 0) {
        strncpy_s(CLOCK_STARTUP_MODE, sizeof(CLOCK_STARTUP_MODE),
                  normalizedStartupMode, _TRUNCATE);
    }

    if (changed) {
        InvalidateRect(hwnd, NULL, TRUE);
    }

    return 0;
}

LRESULT HandleAppPomodoroChanged(HWND hwnd) {
    (void)hwnd;

    /* Pomodoro time options */
    char buf[POMODORO_OPTIONS_CONFIG_BUFFER_SIZE] = {0};
    BOOL optionsComplete = ReadIniStringExact(CFG_SECTION_POMODORO, CFG_KEY_POMODORO_OPTIONS,
                                              DEFAULT_POMODORO_OPTIONS_INI, buf,
                                              sizeof(buf), GetCachedConfigPath());
    int tmp[MAX_POMODORO_TIMES] = {0};
    int cnt = 0;
    if (!optionsComplete) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Pomodoro intervals config is too long during reload; keeping current intervals");
    } else if (ParsePomodoroTimeOptionsForReload(buf, tmp, &cnt) &&
               cnt <= (int)_countof(g_AppConfig.pomodoro.times) &&
               (cnt != g_AppConfig.pomodoro.times_count ||
                memcmp(tmp, g_AppConfig.pomodoro.times, (size_t)cnt * sizeof(tmp[0])) != 0)) {
        g_AppConfig.pomodoro.times_count = cnt;
        ZeroMemory(g_AppConfig.pomodoro.times, sizeof(g_AppConfig.pomodoro.times));
        memcpy(g_AppConfig.pomodoro.times, tmp, (size_t)cnt * sizeof(tmp[0]));

        g_AppConfig.pomodoro.work_time = g_AppConfig.pomodoro.times[0];
        if (cnt > 1) g_AppConfig.pomodoro.short_break = g_AppConfig.pomodoro.times[1];
        if (cnt > 2) g_AppConfig.pomodoro.long_break = g_AppConfig.pomodoro.times[2];
    }

    /* Loop count */
    g_AppConfig.pomodoro.loop_count = ClampHotReloadInt(
        CFG_KEY_POMODORO_LOOP_COUNT,
        ReadConfigInt(CFG_SECTION_POMODORO, CFG_KEY_POMODORO_LOOP_COUNT,
                      DEFAULT_POMODORO_LOOP_COUNT),
        MIN_POMODORO_LOOP_COUNT, MAX_POMODORO_LOOP_COUNT);

    return 0;
}

LRESULT HandleAppNotificationChanged(HWND hwnd) {
    (void)hwnd;

    /* Reload notification settings from INI file for hot-reload support */
    int timeoutMs = ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_TIMEOUT_MS", 3000);
    if (timeoutMs < 0 || timeoutMs > 60000) {
        timeoutMs = 3000;
    }
    g_AppConfig.notification.display.timeout_ms = timeoutMs;

    g_AppConfig.notification.display.max_opacity = ClampHotReloadInt(
        "NOTIFICATION_MAX_OPACITY",
        ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_MAX_OPACITY",
                      DEFAULT_NOTIFICATION_MAX_OPACITY),
        MIN_VISIBLE_OPACITY, MAX_OPACITY);

    g_AppConfig.notification.display.corner_radius = ClampHotReloadInt(
        "NOTIFICATION_CORNER_RADIUS",
        ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_CORNER_RADIUS",
                      DEFAULT_NOTIFICATION_CORNER_RADIUS),
        MIN_NOTIFICATION_CORNER_RADIUS, MAX_NOTIFICATION_CORNER_RADIUS);

    g_AppConfig.notification.display.font_size = ClampHotReloadInt(
        "NOTIFICATION_FONT_SIZE",
        ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_FONT_SIZE",
                      DEFAULT_NOTIFICATION_FONT_SIZE),
        MIN_NOTIFICATION_FONT_SIZE, MAX_NOTIFICATION_FONT_SIZE);

    char typeBuf[32];
    ReadConfigStr(INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE", "CATIME", typeBuf, sizeof(typeBuf));
    if (_stricmp(typeBuf, "SYSTEM_MODAL") == 0) {
        g_AppConfig.notification.display.type = NOTIFICATION_TYPE_SYSTEM_MODAL;
    } else if (_stricmp(typeBuf, "OS") == 0) {
        g_AppConfig.notification.display.type = NOTIFICATION_TYPE_OS;
    } else {
        g_AppConfig.notification.display.type = NOTIFICATION_TYPE_CATIME;
    }

    g_AppConfig.notification.display.disabled = ReadConfigBool(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_DISABLED", FALSE);

    g_AppConfig.notification.display.window_x = ReadConfigInt(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_X", -1);
    g_AppConfig.notification.display.window_y = ReadConfigInt(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_Y", -1);
    g_AppConfig.notification.display.window_width = ClampNotificationConfigWidth(
        ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_WIDTH", 0));
    g_AppConfig.notification.display.window_height = ClampNotificationConfigHeight(
        ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_WINDOW_HEIGHT", 0));

    char timeoutMessage[sizeof(g_AppConfig.notification.messages.timeout_message)] = {0};
    if (ReadHotReloadStringExact(INI_SECTION_NOTIFICATION, "CLOCK_TIMEOUT_MESSAGE_TEXT",
                                 DEFAULT_TIMEOUT_MESSAGE, timeoutMessage,
                                 sizeof(timeoutMessage))) {
        strncpy_s(g_AppConfig.notification.messages.timeout_message,
                  sizeof(g_AppConfig.notification.messages.timeout_message),
                  timeoutMessage, _TRUNCATE);
    }

    char soundBuf[MAX_PATH] = {0};
    BOOL soundConfigComplete = ReadHotReloadStringExact(
        INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE",
        "", soundBuf, sizeof(soundBuf));
    if (soundConfigComplete && soundBuf[0] != '\0') {
        char resolved[MAX_PATH] = {0};
        if (ExpandEffectiveLocalAppDataPath(soundBuf, resolved, sizeof(resolved))) {
            strncpy(g_AppConfig.notification.sound.sound_file, resolved, MAX_PATH - 1);
        } else {
            strncpy(g_AppConfig.notification.sound.sound_file, soundBuf, MAX_PATH - 1);
        }
        g_AppConfig.notification.sound.sound_file[MAX_PATH - 1] = '\0';
    } else if (soundConfigComplete) {
        g_AppConfig.notification.sound.sound_file[0] = '\0';
    }

    int volume = ReadConfigInt(INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_VOLUME", 100);
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    g_AppConfig.notification.sound.volume = volume;

    return 0;
}

LRESULT HandleAppHotkeysChanged(HWND hwnd) {
    RegisterGlobalHotkeys(hwnd);
    return 0;
}

LRESULT HandleAppRecentFilesChanged(HWND hwnd) {
    (void)hwnd;

    HotReloadRecentFilesConfig recentConfig = {0};
    ReadHotReloadRecentFilesConfig(&recentConfig);

    BOOL recentFilesChanged = HotReloadRecentFilesChanged(&recentConfig);
    BOOL validationInputChanged = HotReloadRecentValidationInputChanged(&recentConfig);
    if (!recentFilesChanged && !validationInputChanged) {
        return 0;
    }

    if (recentFilesChanged) {
        extern void LoadRecentFiles(void);
        LoadRecentFiles();
    }

    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE) {
        int recentFilesCount = g_AppConfig.recent_files.count;
        if (recentFilesCount < 0) recentFilesCount = 0;
        if (recentFilesCount > MAX_RECENT_FILES) recentFilesCount = MAX_RECENT_FILES;

        if (CLOCK_TIMEOUT_FILE_PATH[0] == '\0' && recentFilesCount > 0 &&
            !WriteConfigTimeoutFile(g_AppConfig.recent_files.files[0].path)) {
            LOG_WARNING("Failed to save hot-reloaded timeout file: %s",
                        g_AppConfig.recent_files.files[0].path);
        }
    }

    RememberHotReloadRecentFilesConfig(&recentConfig);
    return 0;
}

LRESULT HandleAppColorsChanged(HWND hwnd) {
    HotReloadColorConfig colorConfig = {0};
    ReadHotReloadColorConfig(&colorConfig);

    BOOL colorOptionsChanged = HotReloadColorOptionsChanged(&colorConfig);
    BOOL percentIconColorsChanged = HotReloadPercentIconColorsChanged(&colorConfig);
    if (!colorOptionsChanged && !percentIconColorsChanged) {
        return 0;
    }

    if (colorOptionsChanged) {
        if (ReplaceColorOptionsFromConfigValue(colorConfig.colorOptions)) {
            InvalidateRect(hwnd, NULL, TRUE);
        } else {
            LOG_WARNING("Hot reload ignored invalid COLOR_OPTIONS; keeping current palette");
        }
    }

    if (percentIconColorsChanged) {
        ReadPercentIconColorsConfig();
        TrayAnimation_UpdatePercentIconIfNeeded();
    }

    RememberHotReloadColorConfig(&colorConfig);
    return 0;
}

LRESULT HandleAppAnimSpeedChanged(HWND hwnd) {
    (void)hwnd;
    ReloadAnimationSpeedFromConfig();
    TrayAnimation_RecomputeTimerDelay();
    return 0;
}

LRESULT HandleAppAnimPathChanged(HWND hwnd) {
    (void)hwnd;
    char buffer[MAX_PATH] = {0};
    if (!ReadIniStringExact("Animation", "ANIMATION_PATH", "__logo__",
                            buffer, sizeof(buffer), GetCachedConfigPath())) {
        LOG_WARNING("Hot reload ignored ANIMATION_PATH because the config value is too long");
        return 0;
    }
    ApplyAnimationPathValueNoPersist(buffer);
    return 0;
}
