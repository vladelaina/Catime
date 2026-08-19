/**
 * @file window_commands_plugin.c
 * @brief Plugin command dispatch and runtime toggling.
 */

#include "window_commands_plugin_internal.h"
#include "preview_display.h"

static BOOL HandlePluginToggle(HWND hwnd, int pluginIndex) {
    ClosePluginSecurityDialog();
    /* Check if this plugin is already running - toggle off */
    if (PluginManager_GetActivePluginIndex() == pluginIndex) {
        if (!PluginManager_RequestStop(hwnd, pluginIndex)) {
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

    if (!PluginManager_RequestStart(hwnd, pluginIndex)) {
        LOG_WARNING("Plugin operation already in progress");
        MessageBeep(MB_OK);
    }

    return TRUE;
}

void HandlePluginExit(HWND hwnd) {
    /* An asynchronous <exit> can arrive while a menu preview temporarily owns
     * the main display. Retire that snapshot before changing timer state so a
     * later menu close cannot restore the pre-exit contents. */
    RestoreWindowVisibility(hwnd);

    /* Cancel any pending exit countdown */
    PluginExit_Cancel();
    ClosePluginSecurityDialog();

    /* Stop all plugins */
    PluginManager_RequestStopAll(hwnd);

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
