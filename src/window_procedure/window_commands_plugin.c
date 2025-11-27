/**
 * @file window_commands_plugin.c
 * @brief Plugin-related command handlers
 */

#include "window_procedure/window_commands.h"
#include "window_procedure/window_helpers.h"
#include "window_procedure/window_procedure.h"
#include "plugin/plugin_manager.h"
#include "plugin/plugin_data.h"
#include "timer/timer.h"
#include "color/gradient.h"
#include "color/color_parser.h"
#include "window.h"
#include "pomodoro.h"
#include "notification.h"
#include <windows.h>

/* External function declarations */
extern void GetActiveColor(char* outColor, size_t bufferSize);

extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_COUNT_UP;
extern BOOL CLOCK_IS_PAUSED;

/* ============================================================================
 * Plugin Command Handlers
 * ============================================================================ */

/**
 * @brief Handle plugin start/stop toggle
 */
static BOOL HandlePluginToggle(HWND hwnd, int pluginIndex) {
    /* Check if this plugin is already running - toggle off */
    if (PluginManager_IsPluginRunning(pluginIndex)) {
        PluginManager_StopPlugin(pluginIndex);
        PluginData_Clear();
        
        /* Switch to idle state */
        TimerModeParams params = {0, TRUE, TRUE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
        KillTimer(hwnd, 1);
        InvalidateRect(hwnd, NULL, TRUE);
        return TRUE;
    }

    /* Plugin not running - start it */
    
    /* Stop notification sound */
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    /* Reset timer flags */
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    CLOCK_COUNT_UP = FALSE;
    CLOCK_IS_PAUSED = TRUE;
    
    /* Stop internal timer */
    KillTimer(hwnd, 1);
    
    /* Reset Pomodoro if active */
    extern POMODORO_PHASE current_pomodoro_phase;
    if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
        current_pomodoro_phase = POMODORO_PHASE_IDLE;
    }

    /* Reset timer values */
    extern int CLOCK_TOTAL_TIME;
    extern int countdown_elapsed_time;
    extern int countup_elapsed_time;
    CLOCK_TOTAL_TIME = 0;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;

    /* Start plugin */
    PluginManager_StartPlugin(pluginIndex);
    
    /* Show loading message */
    const PluginInfo* pluginInfo = PluginManager_GetPlugin(pluginIndex);
    if (pluginInfo) {
        wchar_t loadingText[256];
        wchar_t displayNameW[128];
        MultiByteToWideChar(CP_UTF8, 0, pluginInfo->displayName, -1, displayNameW, 128);
        _snwprintf(loadingText, 256, L"Loading %s...", displayNameW);
        PluginData_SetText(loadingText);
    }
    
    /* Check if animated gradient needs timer for smooth animation */
    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));
    if (IsGradientAnimated(GetGradientTypeByName(activeColor))) {
        SetTimer(hwnd, 1, 30, NULL);  /* ~33 FPS for smooth animation */
    }
    
    /* Ensure window visible and redraw */
    if (!IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_SHOW);
    }
    InvalidateRect(hwnd, NULL, TRUE);
    
    return TRUE;
}

/**
 * @brief Handle "Show plugin file" command
 */
static BOOL HandleShowPluginFile(HWND hwnd) {
    /* Check if already in show file mode */
    BOOL isShowFileMode = PluginData_IsActive();
    BOOL anyPluginRunning = FALSE;
    
    int pluginCount = PluginManager_GetPluginCount();
    for (int i = 0; i < pluginCount; i++) {
        if (PluginManager_IsPluginRunning(i)) {
            anyPluginRunning = TRUE;
            break;
        }
    }
    
    if (isShowFileMode && !anyPluginRunning) {
        /* Toggle off */
        BOOL hadCatimeTag = PluginData_HasCatimeTag();
        PluginData_Clear();
        
        if (hadCatimeTag) {
            /* Had <catime> tag - restore time display, keep timer */
            InvalidateRect(hwnd, NULL, TRUE);
        } else {
            /* No <catime> tag - switch to idle */
            TimerModeParams params = {0, TRUE, TRUE, TRUE};
            SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
            KillTimer(hwnd, 1);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return TRUE;
    }
    
    /* Toggle on - activate show file mode */
    PluginManager_StopAllPlugins();
    PluginData_SetActive(TRUE);
    
    /* Check for <catime> tag */
    if (!PluginData_HasCatimeTag()) {
        KillTimer(hwnd, 1);
        CLOCK_SHOW_CURRENT_TIME = FALSE;
        CLOCK_COUNT_UP = FALSE;
        CLOCK_IS_PAUSED = FALSE;
    }
    
    /* Check if animated gradient needs timer for smooth animation */
    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));
    if (IsGradientAnimated(GetGradientTypeByName(activeColor))) {
        SetTimer(hwnd, 1, 30, NULL);  /* ~33 FPS for smooth animation */
    }
    
    if (!IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_SHOW);
    }
    InvalidateRect(hwnd, NULL, TRUE);
    
    return TRUE;
}

/* ============================================================================
 * Plugin Command Dispatcher
 * ============================================================================ */

BOOL HandlePluginCommand(HWND hwnd, UINT cmd) {
    /* Plugin start/stop */
    if (cmd >= CLOCK_IDM_PLUGINS_BASE && cmd < CLOCK_IDM_PLUGINS_SETTINGS_BASE) {
        int pluginIndex = cmd - CLOCK_IDM_PLUGINS_BASE;
        return HandlePluginToggle(hwnd, pluginIndex);
    }

    /* Plugin settings (deprecated but kept for safety) */
    if (cmd >= CLOCK_IDM_PLUGINS_SETTINGS_BASE && cmd < CLOCK_IDM_PLUGINS_SHOW_FILE) {
        return TRUE;
    }

    /* Show plugin file */
    if (cmd == CLOCK_IDM_PLUGINS_SHOW_FILE) {
        return HandleShowPluginFile(hwnd);
    }

    /* Open plugin folder */
    if (cmd == CLOCK_IDM_PLUGINS_OPEN_DIR) {
        PluginManager_OpenPluginFolder();
        return TRUE;
    }

    return FALSE;
}
