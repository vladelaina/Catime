#include "window_procedure/window_procedure.h"
#include "window_procedure/window_message_handlers.h"
#include "window_procedure/window_commands.h"
#include "window_procedure/window_config_handlers.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_helpers.h"
#include "tray/tray_events.h"
#include "tray/tray_animation_core.h"
#include "tray/tray.h"
#include "config.h"
#include "timer/timer.h"
#include "timer/timer_events.h"
#include "timer/main_timer.h"
#include "audio_player.h"
#include "window.h"
#include "pomodoro.h"
#include "notification.h"
#include "drawing.h"
#include "../resource/resource.h"
#include "log.h"
#include <string.h>
#include <windowsx.h>
#include "window_procedure/window_drop_target.h"
#include "window_procedure/window_events.h"
#include "color/color_parser.h"
#include "plugin/plugin_manager.h"
#include "plugin/plugin_data.h"
#include "dialog/dialog_plugin_security.h"
#include "markdown/markdown_interactive.h"
#include "drag_scale.h" // Added this line
#include "preview_display.h"
extern UINT WM_TASKBARCREATED;
void ToggleShowTimeMode(HWND hwnd) {
    CleanupBeforeTimerAction(hwnd);
    if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
        ResetPomodoroState();
    }
    if (!CLOCK_SHOW_CURRENT_TIME) {
        TimerModeParams params = {0, TRUE, TRUE, TRUE};  /* showWindow = TRUE */
        SwitchTimerMode(hwnd, TIMER_MODE_SHOW_TIME, &params);
    } else {
        CLOCK_SHOW_CURRENT_TIME = false;
        CLOCK_COUNT_UP = false;
        CLOCK_IS_PAUSED = false;
        CLOCK_TOTAL_TIME = 0;
        countdown_elapsed_time = 0;
        countup_elapsed_time = 0;
        countdown_message_shown = true;
        MainTimer_Stop();
        InvalidateRect(hwnd, NULL, TRUE);
    }
}
void StartCountUp(HWND hwnd) {
    CleanupBeforeTimerAction(hwnd);
    if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
        ResetPomodoroState();
    }
    TimerModeParams params = {0, TRUE, TRUE, TRUE};  /* showWindow = TRUE */
    SwitchTimerMode(hwnd, TIMER_MODE_COUNTUP, &params);
    MainTimer_Stop();
    ResetTimerWithInterval(hwnd);
}
void StartDefaultCountDown(HWND hwnd) {
    CleanupBeforeTimerAction(hwnd);
    if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
        ResetPomodoroState();
    }
    if (g_AppConfig.timer.default_start_time > 0) {
        countdown_message_shown = false;
        TimerModeParams params = {g_AppConfig.timer.default_start_time, TRUE, TRUE, TRUE};  /* showWindow = TRUE */
        SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
        MainTimer_Stop();
        ResetTimerWithInterval(hwnd);
    } else {
        PostMessage(hwnd, WM_COMMAND, CLOCK_IDM_CUSTOM_COUNTDOWN, 0);
    }
}
void StartPomodoroTimer(HWND hwnd) {
    CleanupBeforeTimerAction(hwnd);
    EnsureWindowVisibleWithTopmostState(hwnd);
    InitializePomodoro();
    CLOCK_SHOW_CURRENT_TIME = false;
    CLOCK_COUNT_UP = false;
    CLOCK_IS_PAUSED = false;
    ResetTimer();
    MainTimer_Stop();
    ResetTimerWithInterval(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}
void ToggleEditMode(HWND hwnd) {
    if (CLOCK_EDIT_MODE) {
        EndEditMode(hwnd);
    } else {
        StartEditMode(hwnd);
    }
}
void RestartCurrentTimer(HWND hwnd) {
    CloseAllNotifications(); // Centralized cleanup
    StopNotificationSound();
    CleanupBeforeTimerAction(hwnd);
    if (!CLOCK_SHOW_CURRENT_TIME) {
        message_shown = FALSE;
        countdown_message_shown = false;
        if (CLOCK_COUNT_UP) {
            countdown_elapsed_time = 0;
            countup_elapsed_time = 0;
        } else {
            countdown_elapsed_time = 0;
            elapsed_time = 0;
        }
        CLOCK_IS_PAUSED = false;
        ResetTimer();
        MainTimer_Stop();
        ResetTimerWithInterval(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
    }
    HandleWindowReset(hwnd);
}
void StartQuickCountdownByIndex(HWND hwnd, int index) {
    if (index <= 0) return;
    CleanupBeforeTimerAction(hwnd);
    int zeroBased = index - 1;
    int timeOptionsCount = time_options_count;
    if (timeOptionsCount < 0) timeOptionsCount = 0;
    if (timeOptionsCount > MAX_TIME_OPTIONS) timeOptionsCount = MAX_TIME_OPTIONS;
    if (zeroBased < timeOptionsCount && time_options[zeroBased] > 0) {
        StartCountdownWithTime(hwnd, time_options[zeroBased]);
    } else {
        StartDefaultCountDown(hwnd);
    }
}
void CleanupBeforeTimerAction(HWND hwnd) {
    RestoreWindowVisibility(hwnd);
    ClosePluginSecurityDialog();
    StopNotificationSound();
    CloseAllNotifications();
    if (!PluginData_HasCatimeTag()) {
        if (!PluginManager_RequestStopAll(hwnd)) {
            PluginManager_StopAllPlugins();
        }
    }
}
BOOL StartCountdownWithTime(HWND hwnd, int seconds) {
    if (seconds <= 0) return FALSE;
    countdown_message_shown = false;
    if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
        ResetPomodoroState();
    }
    TimerModeParams params = {seconds, TRUE, TRUE, TRUE};
    BOOL result = SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
    MainTimer_Stop();
    ResetTimerWithInterval(hwnd);
    return result;
}
void ToggleMilliseconds(HWND hwnd) {
    BOOL previousState = g_AppConfig.display.time_format.show_milliseconds;
    BOOL newState = !previousState;
    if (!WriteConfigShowMilliseconds(newState) || previousState == g_AppConfig.display.time_format.show_milliseconds) {
        return;
    }
    ResetTimerWithInterval(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}
void ToggleTopmost(HWND hwnd) {
    MarkEditModeTopmostOverride();
    SetWindowTopmost(hwnd, !CLOCK_WINDOW_TOPMOST);
}
void ToggleWindowVisibility(HWND hwnd) {
    if (IsWindowVisible(hwnd)) {
        HideWindowIntentionally(hwnd);
    } else {
        EnsureWindowVisibleWithTopmostState(hwnd);
        SetForegroundWindow(hwnd);
    }
}
