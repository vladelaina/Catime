        BOOL newTopmost = ReadConfigBool(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_TOPMOST, CLOCK_WINDOW_TOPMOST);
        int newOpacity = ReadConfigInt(CFG_SECTION_DISPLAY, "WINDOW_OPACITY", CLOCK_WINDOW_OPACITY);

        BOOL posChanged = !skipPositionUpdate && ((posX != CLOCK_WINDOW_POS_X) || (posY != CLOCK_WINDOW_POS_Y));
        BOOL scaleChanged = hasValidScale && fabsf(newScale - CLOCK_WINDOW_SCALE) > 0.0001f;

        /* Placement metadata can change independently of the legacy absolute
         * X/Y keys (for example after a taskbar moves to another edge). */
        if (!skipPositionUpdate && manualPosition) {
            RECT currentRect = {0};
            BOOL hasCurrentRect = GetWindowRect(hwnd, &currentRect);
            int placementWidth = hasCurrentRect
                ? currentRect.right - currentRect.left
                : ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_WIDTH,
                                              CLOCK_WINDOW_SCALE);
            int placementHeight = hasCurrentRect
                ? currentRect.bottom - currentRect.top
                : ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_HEIGHT,
                                              CLOCK_WINDOW_SCALE);
            int resolvedX = posX;
            int resolvedY = posY;
            ResolveConfiguredWindowPosition(placementWidth, placementHeight,
                                            &resolvedX, &resolvedY);
            long long deltaX = llabs((long long)resolvedX -
                                     CLOCK_WINDOW_POS_X);
            long long deltaY = llabs((long long)resolvedY -
                                     CLOCK_WINDOW_POS_Y);
            if (deltaX > 1 || deltaY > 1) {
                posX = resolvedX;
                posY = resolvedY;
                posChanged = TRUE;
            } else {
                posX = CLOCK_WINDOW_POS_X;
                posY = CLOCK_WINDOW_POS_Y;
                posChanged = FALSE;
            }
        }


        if (scaleChanged) {
            CLOCK_WINDOW_SCALE = newScale;
            CLOCK_FONT_SCALE_FACTOR = newScale;
            changed = TRUE;
        }

        if (hasValidPluginScale && fabsf(newPluginScale - PLUGIN_FONT_SCALE_FACTOR) > 0.0001f) {
            PLUGIN_FONT_SCALE_FACTOR = newPluginScale;
            changed = TRUE;
        }

        if (posChanged) {
            RECT currentRect = {0};
            BOOL hasCurrentRect = GetWindowRect(hwnd, &currentRect);
            int width = hasCurrentRect
                ? currentRect.right - currentRect.left
                : ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_WIDTH,
                                              CLOCK_WINDOW_SCALE);
            int height = hasCurrentRect
                ? currentRect.bottom - currentRect.top
                : ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_HEIGHT,
                                              CLOCK_WINDOW_SCALE);

            ResolveConfiguredWindowPosition(width, height, &posX, &posY);

            LOG_DEBUG("Hot reload applying window position: pos=(%d, %d), scaleChanged=%d",
                      posX, posY, scaleChanged);
            SetWindowPos(hwnd, NULL, posX, posY, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            CLOCK_WINDOW_POS_X = posX;
            CLOCK_WINDOW_POS_Y = posY;
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
    bool intervalChanged = false;

    /* Basic timer settings */
    changed |= LoadAndCompareBool(CFG_SECTION_TIMER, CFG_KEY_USE_24HOUR,
                                  &CLOCK_USE_24HOUR, CLOCK_USE_24HOUR);
    intervalChanged = LoadAndCompareBool(CFG_SECTION_TIMER, CFG_KEY_SHOW_SECONDS,
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
        if (intervalChanged) {
            ResetTimerWithInterval(hwnd);
        }
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
