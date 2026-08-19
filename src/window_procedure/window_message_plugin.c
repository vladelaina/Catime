/**
 * @file window_message_plugin.c
 * @brief Handles plugin security decisions and hot-reload requests.
 */

#include "window_procedure/window_message_handlers_internal.h"
#include "audio_player.h"
#include "color/color.h"
#include "color/gradient.h"
#include "config.h"
#include "dialog/dialog_plugin_security.h"
#include "log.h"
#include "menu_preview.h"
#include "plugin/plugin_data.h"
#include "plugin/plugin_manager.h"
#include "plugin/plugin_process.h"
#include "pomodoro.h"
#include "timer/main_timer.h"
#include "window.h"
#include "window/window_visual_effects.h"
#include <stdlib.h>

static LONG g_pluginSecurityOperationSerial = 0;

static void ApplyStartedPluginWindowState(HWND hwnd) {
    StopNotificationSound();
    countdown_message_shown = true;
    CLOCK_SHOW_CURRENT_TIME = false;
    CLOCK_COUNT_UP = false;
    CLOCK_IS_PAUSED = true;
    MainTimer_Stop();
    current_pomodoro_phase = POMODORO_PHASE_IDLE;
    CLOCK_TOTAL_TIME = 0;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;

    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));
    if (IsGradientNameAnimated(activeColor)) MainTimer_Start(hwnd, 66);
}

LRESULT HandlePluginSecurityRequest(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    PluginSecurityRequest* request = (PluginSecurityRequest*)lp;
    if (request) {
        if (PluginManager_IsOperationCurrent(request->serial)) {
            if (IsPluginSecurityDialogOpen()) ClosePluginSecurityDialog();
            g_pluginSecurityOperationSerial = request->serial;
            ShowPluginSecurityDialog(hwnd, request->path, request->displayName,
                                     request->index, request->hash);
        }
        free(request);
    }
    return 0;
}

LRESULT HandlePluginOperationComplete(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    PluginOperationResult* result = (PluginOperationResult*)lp;
    if (!result) return 0;
    if (!PluginManager_IsOperationCurrent(result->serial)) {
        free(result);
        return 0;
    }
    if (!result->success &&
        (result->operation == PLUGIN_OPERATION_START ||
         result->operation == PLUGIN_OPERATION_START_AFTER_SECURITY)) {
        PluginData_SetStatusText(result->error[0] ? result->error : L"FAIL");
    }
    if (result->success &&
        (result->operation == PLUGIN_OPERATION_START ||
         result->operation == PLUGIN_OPERATION_START_AFTER_SECURITY)) {
        ApplyStartedPluginWindowState(hwnd);
    }
    if (result->operation == PLUGIN_OPERATION_START ||
        result->operation == PLUGIN_OPERATION_START_AFTER_SECURITY) {
        EnsureWindowVisibleWithTopmostState(hwnd);
    }
    InvalidateRect(hwnd, NULL, TRUE);
    free(result);
    return 0;
}

LRESULT HandleDialogPluginSecurity(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;

    int pluginIndex = GetPendingPluginIndex();
    const char* pendingPluginPath = GetPendingPluginPath();

    if (wp == IDCANCEL) {
        /* User cancelled - just clear pending info, don't change display state */
        g_pluginSecurityOperationSerial = 0;
        ClearPendingPluginInfo();
        return 0;
    }
    if ((wp != IDYES && wp != IDOK) ||
        !PluginManager_IsOperationCurrent(g_pluginSecurityOperationSerial) ||
        pluginIndex < 0 ||
        !pendingPluginPath ||
        pendingPluginPath[0] == '\0') {
        LOG_WARNING("Ignoring stale plugin security dialog result");
        g_pluginSecurityOperationSerial = 0;
        ClearPendingPluginInfo();
        return 0;
    }

    /* Copy dialog state and queue verification before changing the current
     * timer. A busy worker must leave the existing display untouched. */
    BOOL trustPlugin = (wp == IDYES);
    char expectedPath[MAX_PATH] = {0};
    char savedHash[65] = {0};
    strncpy_s(expectedPath, sizeof(expectedPath), pendingPluginPath, _TRUNCATE);
    const char* pendingHash = GetPendingPluginHash();
    if (pendingHash) strncpy_s(savedHash, sizeof(savedHash), pendingHash, _TRUNCATE);
    BOOL queued = PluginManager_RequestStartAfterSecurityCheck(
        hwnd, pluginIndex, trustPlugin, expectedPath, savedHash);
    g_pluginSecurityOperationSerial = 0;
    ClearPendingPluginInfo();
    if (!queued) {
        MessageBeep(MB_OK);
        return 0;
    }

    /* User confirmed - now change state while the worker verifies and starts. */

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

    /* Check if animated gradient needs timer for smooth animation */
    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));
    if (IsGradientNameAnimated(activeColor)) {
        MainTimer_Start(hwnd, 66);  /* 15 FPS for smooth animation */
    }

    /* Re-apply visibility/topmost policy to recover from any z-order drift */
    EnsureWindowVisibleWithTopmostState(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);

    return 0;
}

LRESULT HandlePluginHotReload(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;

    PluginManager_RequestHotReload(hwnd, (LONG)(LONG_PTR)wp);
    return 0;
}
