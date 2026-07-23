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
