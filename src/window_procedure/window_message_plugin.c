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

LRESULT HandlePluginHotReload(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd;
    (void)lp;

    PluginManager_RestartPendingHotReload((LONG)(LONG_PTR)wp);

    return 0;
}
