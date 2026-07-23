/**
 * @file window_commands_plugin.c
 * @brief Plugin command dispatch and runtime toggling.
 */

#include "window_commands_plugin_internal.h"

static BOOL HandlePluginToggle(HWND hwnd, int pluginIndex) {
    /* Check if this plugin is already running - toggle off */
    if (PluginManager_GetActivePluginIndex() == pluginIndex ||
        PluginManager_IsPluginRunning(pluginIndex)) {
        if (!PluginManager_StopPlugin(pluginIndex)) {
            PluginData_Clear();
        }

        /* Prevent countdown completion notification from triggering */
        countdown_message_shown = true;

        /* Switch to idle state - don't reset timer to avoid 1-minute fallback */
        CLOCK_SHOW_CURRENT_TIME = false;
        CLOCK_COUNT_UP = false;
        CLOCK_IS_PAUSED = true;
        CLOCK_TOTAL_TIME = 0;
        countdown_elapsed_time = 0;
        MainTimer_Stop();
        InvalidateRect(hwnd, NULL, TRUE);
        return TRUE;
    }

    /* Plugin not running - check if it needs security confirmation first */
    /* If security dialog is needed, don't change any state yet */
    if (PluginManager_NeedsSecurityCheck(pluginIndex)) {
        /* Show security dialog without changing current state */
        PluginManager_StartPlugin(pluginIndex);
        /* State will be changed in HandleDialogPluginSecurity when user confirms */
        return TRUE;
    }

    /* Plugin is trusted - proceed with state change and launch */

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
    BOOL hasPluginInfo = PluginManager_CopyPlugin(pluginIndex, &pluginInfo);
    if (hasPluginInfo) {
        wchar_t loadingText[256];
        PluginData_SetOutputDirectoryFromPluginPath(pluginInfo.path);
        _snwprintf_s(loadingText, 256, _TRUNCATE, L"Loading %ls...", pluginInfo.displayName);
        PluginData_SetText(loadingText);
    }

    /* Start plugin */
    BOOL startResult = PluginManager_StartPlugin(pluginIndex);

    if (!startResult) {
        /* Launch failed - show error */
        LOG_ERROR("Plugin failed to start: %ls", hasPluginInfo ? pluginInfo.displayName : L"unknown");

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

    /* Ensure window visible and consistent with topmost policy */
    EnsureWindowVisibleWithTopmostState(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);

    return TRUE;
}

void HandlePluginExit(HWND hwnd) {
    /* Cancel any pending exit countdown */
    PluginExit_Cancel();

    /* Stop all plugins */
    PluginManager_StopAllPlugins();

    /* Prevent countdown completion notification from triggering */
    countdown_message_shown = true;

    /* Switch to idle state - don't reset timer to avoid 1-minute fallback */
    CLOCK_SHOW_CURRENT_TIME = false;
    CLOCK_COUNT_UP = false;
    CLOCK_IS_PAUSED = true;
    CLOCK_TOTAL_TIME = 0;
    countdown_elapsed_time = 0;
    MainTimer_Stop();
    InvalidateRect(hwnd, NULL, TRUE);

    LOG_INFO("Plugin exit completed via <exit> tag");
}

/* ============================================================================
 * Plugin Command Dispatcher
 * ============================================================================ */

BOOL HandlePluginCommand(HWND hwnd, UINT cmd) {
    /* Plugin start/stop */
    if (cmd >= CLOCK_IDM_PLUGINS_BASE && cmd < CLOCK_IDM_PLUGINS_BASE + MAX_PLUGINS) {
        int pluginIndex = cmd - CLOCK_IDM_PLUGINS_BASE;
        return HandlePluginToggle(hwnd, pluginIndex);
    }

    /* Plugin settings (deprecated but kept for safety) */
    if (cmd >= CLOCK_IDM_PLUGINS_SETTINGS_BASE && cmd < CLOCK_IDM_CUSTOM_TEXT_DISPLAY) {
        return TRUE;
    }

    /* Custom text display */
    if (cmd == CLOCK_IDM_CUSTOM_TEXT_DISPLAY) {
        return WindowPlugin_HandleCustomTextDisplay(hwnd);
    }

    /* Open plugin folder */
    if (cmd == CLOCK_IDM_PLUGINS_OPEN_DIR) {
        PluginManager_OpenPluginFolder();
        return TRUE;
    }

    return FALSE;
}
