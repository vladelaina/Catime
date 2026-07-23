    if (!GetMenuItemInfoW(hMenu, menuItem, FALSE, &itemInfo)) {
        return FALSE;
    }

    if (itemInfo.hSubMenu ||
        (itemInfo.fType & MFT_SEPARATOR) ||
        (itemInfo.fState & (MFS_DISABLED | MFS_GRAYED))) {
        return FALSE;
    }

    return TRUE;
}

static BOOL IsPreviewMenuItem(UINT menuItem, BOOL* isColorOrFontPreview,
                              BOOL* isAnimationPreview) {
    BOOL colorOrFont = FALSE;
    BOOL animation = FALSE;

    if (GetColorMenuColorFromId(menuItem, NULL, 0)) {
        colorOrFont = TRUE;
    }

    if (menuItem >= CMD_FONT_SELECTION_BASE &&
        menuItem < CMD_FONT_SELECTION_BASE + FONT_MENU_MAX_ENTRIES) {
        colorOrFont = TRUE;
    }

    if (menuItem == CLOCK_IDM_TIME_FORMAT_DEFAULT ||
        menuItem == CLOCK_IDM_TIME_FORMAT_ZERO_PADDED ||
        menuItem == CLOCK_IDM_TIME_FORMAT_FULL_PADDED ||
        menuItem == CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS ||
        menuItem == CLOCK_IDM_SHOW_SECONDS ||
        menuItem == CLOCK_IDM_24HOUR_FORMAT ||
        TextEffect_IsMenuId(menuItem)) {
        colorOrFont = TRUE;
    }

    if (menuItem == CLOCK_IDM_ANIMATIONS_USE_LOGO ||
        menuItem == CLOCK_IDM_ANIMATIONS_USE_CPU ||
        menuItem == CLOCK_IDM_ANIMATIONS_USE_MEM ||
        menuItem == CLOCK_IDM_ANIMATIONS_USE_BATTERY ||
        menuItem == CLOCK_IDM_ANIMATIONS_USE_CAPSLOCK ||
        menuItem == CLOCK_IDM_ANIMATIONS_USE_NONE ||
        (menuItem >= CLOCK_IDM_ANIMATIONS_BASE && menuItem < CLOCK_IDM_ANIMATIONS_END)) {
        animation = TRUE;
    }

    if (isColorOrFontPreview) {
        *isColorOrFontPreview = colorOrFont;
    }
    if (isAnimationPreview) {
        *isAnimationPreview = animation;
    }
    return colorOrFont || animation;
}

/* ============================================================================
 * Modeless Dialog Result Handlers
 * ============================================================================ */

/**
 * @brief Handle countdown dialog result
 * @param wp Countdown time in seconds (0 = cancelled)
 * @param lp Reserved
 */
LRESULT HandleDialogCountdown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    int seconds = (int)wp;
    if (seconds > 0) {
        CleanupBeforeTimerAction();
        StartCountdownWithTime(hwnd, seconds);
    }
    return 0;
}

/**
 * @brief Handle shortcut time dialog result
 * @param wp Reserved
 * @param lp Reserved
 * @note Config is saved by dialog, just need to acknowledge
 */
LRESULT HandleDialogShortcut(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    /* Shortcut time options already saved by dialog */
    return 0;
}

/**
 * @brief Handle color dialog result
 * @param wp 0 = cancelled, 1 = color applied
 * @param lp Reserved
 */
LRESULT HandleDialogColor(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == 0) {
        /* Color cancelled - restore preview if needed */
        CancelPreview(hwnd);
    }
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

/**
 * @brief Handle update dialog result
 * @param wp IDYES = update now, IDNO = later
 * @param lp Reserved
 */
LRESULT HandleDialogUpdate(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == IDYES) {
        /* User chose to update - trigger download and exit */
        TriggerUpdateDownload(hwnd);
    }
    return 0;
}

/**
 * @brief Handle update check result from background thread
 * @param wp 1 = update available, 0 = no update
 * @param lp 1 = silent startup check, 0 = interactive/manual check
 */
LRESULT HandleUpdateCheckResult(HWND hwnd, WPARAM wp, LPARAM lp) {
    if (wp == 1) {
        if (lp == 0) {
            ShowStoredUpdateDialog(hwnd);
        }
    } else {
        if (lp == 0) {
            ShowStoredNoUpdateDialog(hwnd);
        }
    }
    return 0;
}

/**
 * @brief Handle font license dialog result
 * @param wp IDOK = accepted, IDCANCEL = rejected
 * @param lp Reserved
 */
LRESULT HandleDialogFontLicense(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == IDOK) {
        SetFontLicenseAccepted(TRUE);
        SetFontLicenseVersionAccepted(GetCurrentFontLicenseVersion());
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
}

/**
 * @brief Handle plugin security dialog result
 * @param wp IDYES = trust and run, IDOK = run once, IDCANCEL = cancelled
 * @param lp Reserved
 */
LRESULT HandleDialogPluginSecurity(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;

    int pluginIndex = GetPendingPluginIndex();
    const char* pendingPluginPath = GetPendingPluginPath();

    if (wp == IDCANCEL) {
        /* User cancelled - just clear pending info, don't change display state */
        ClearPendingPluginInfo();
        return 0;
    }
    if ((wp != IDYES && wp != IDOK) ||
        pluginIndex < 0 ||
        !pendingPluginPath ||
        pendingPluginPath[0] == '\0') {
        LOG_WARNING("Ignoring stale plugin security dialog result");
        ClearPendingPluginInfo();
        return 0;
    }

    /* User confirmed - now change state and start plugin */

    /* Stop notification sound */
    StopNotificationSound();

    /* Prevent countdown completion notification from triggering */
    countdown_message_shown = true;

    /* Reset timer flags */
    CLOCK_SHOW_CURRENT_TIME = false;
    CLOCK_COUNT_UP = false;
    CLOCK_IS_PAUSED = true;

    /* Stop internal timer */
    MainTimer_Stop();

    /* Reset Pomodoro if active */
    current_pomodoro_phase = POMODORO_PHASE_IDLE;

    /* Reset timer values */
    CLOCK_TOTAL_TIME = 0;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;

    /* Show loading message */
    PluginInfo pluginInfo;
    if (PluginManager_CopyPlugin(pluginIndex, &pluginInfo)) {
        wchar_t loadingText[256];
        PluginData_SetOutputDirectoryFromPluginPath(pluginInfo.path);
        _snwprintf_s(loadingText, 256, _TRUNCATE, L"Loading %ls...", pluginInfo.displayName);
        PluginData_SetText(loadingText);
        PluginData_SetActive(TRUE);
    }

    /* IDYES = Trust & Run, IDOK = Run Once */
    BOOL trustPlugin = (wp == IDYES);
    BOOL startResult = PluginManager_StartPluginAfterSecurityCheck(pluginIndex, trustPlugin);

    if (!startResult) {
        /* Failed to start after security check - show specific error */
        const wchar_t* errorMsg = PluginProcess_GetLastError();
        if (errorMsg && errorMsg[0] != L'\0') {
            PluginData_SetStatusText(errorMsg);
        } else {
            PluginData_SetStatusText(L"FAIL");
        }
    }

    /* Check if animated gradient needs timer for smooth animation */
    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));
    if (IsGradientNameAnimated(activeColor)) {
        MainTimer_Start(hwnd, 66);  /* 15 FPS for smooth animation */
    }

    /* Re-apply visibility/topmost policy to recover from any z-order drift */
    EnsureWindowVisibleWithTopmostState(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);

    ClearPendingPluginInfo();

    return 0;
}

/**
 * @brief Handle plugin hot-reload request from background thread
 * @param wp Hot-reload request generation
 * @param lp Reserved
 */
LRESULT HandlePluginHotReload(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd;
    (void)lp;

    PluginManager_RestartPendingHotReload((LONG)(LONG_PTR)wp);

    return 0;
}

LRESULT HandleInitMenuPopup(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd;

    if (HIWORD(lp)) {
        return 0;
    }

    UpdateHelpSubmenuSupportFace((HMENU)wp);
    return 0;
}

LRESULT HandleMenuSelect(HWND hwnd, WPARAM wp, LPARAM lp) {
    UINT menuItem = LOWORD(wp);
    UINT flags = HIWORD(wp);
    HMENU hMenu = (HMENU)lp;

    /* Mouse moved outside any menu item - set debounce timer to cancel preview */
    if (menuItem == 0xFFFF) {
        ResetMenuPreviewTracking(hwnd);
        StartMenuDebounceTimer(hwnd);
        return 0;
    } else {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
    }

    if (hMenu == NULL) return 0;

    if (!(flags & MF_POPUP) && IsSelectableCommandMenuItem(hMenu, menuItem)) {
