
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
        BOOL manualPosition = ReadConfigBool(CFG_SECTION_DISPLAY,
                                             CFG_KEY_WINDOW_POSITION_MANUAL,
                                             FALSE);
        CLOCK_WINDOW_POSITION_MANUAL = manualPosition;

        /* Skip position handling for special/default sentinels during hot-reload.
         * Reset/apply paths resolve them with finalized window dimensions. */
        BOOL skipPositionUpdate = ((!manualPosition &&
                                    (configPosX == -2 || configPosX == -1 ||
                                     posY == DEFAULT_WINDOW_POS_Y)) ||
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
