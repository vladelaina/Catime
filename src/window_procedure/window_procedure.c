/**
 * @file window_procedure.c
 * @brief Main window procedure and public API
 */

#include "window_procedure/window_procedure.h"
#include "window_procedure/window_message_handlers.h"
#include "window_procedure/window_commands.h"
#include "window_procedure/window_config_handlers.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_helpers.h"
#include "tray/tray_events.h"
#include "config.h"
#include "timer/timer.h"
#include "window.h"
#include "pomodoro.h"
#include "notification.h"
#include "drawing.h"
#include "../resource/resource.h"
#include "log.h"
#include <string.h>
#include <windowsx.h>

#include "window_procedure/window_drop_target.h"
#include "color/color_parser.h"
#include "plugin/plugin_manager.h"
#include "plugin/plugin_data.h"
#include "markdown/markdown_interactive.h"
#include "drag_scale.h" // Added this line

/* ============================================================================
 * External Declarations
 * ============================================================================ */

extern UINT WM_TASKBARCREATED;
extern int time_options[];
extern int time_options_count;
extern BOOL PREVIOUS_TOPMOST_STATE;

#define OPACITY_FULL 255

/* ============================================================================
 * Power Management Handler
 * ============================================================================ */

static LRESULT HandlePowerBroadcast(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    static volatile LONG s_handling = 0;

    /* Handle system resume from sleep/hibernate */
    if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
        /* Prevent re-entry if called multiple times in quick succession */
        if (InterlockedCompareExchange(&s_handling, 1, 0) != 0) {
            return TRUE;
        }

        LOG_INFO("System resumed from sleep/hibernate, reinitializing tray icon animation");

        /* Step 1: Clear animation name to force reload
         * This bypasses the "same name" check in ApplyAnimationPathValueNoPersist */
        extern void TrayAnimation_ClearCurrentName(void);
        TrayAnimation_ClearCurrentName();

        /* Step 2: Reload animation from config */
        extern LRESULT HandleAppAnimPathChanged(HWND);
        HandleAppAnimPathChanged(hwnd);

        /* Step 3: Recreate tray icon with newly loaded animation */
        extern void RecreateTaskbarIcon(HWND, HINSTANCE);
        RecreateTaskbarIcon(hwnd, GetModuleHandle(NULL));

        InterlockedExchange(&s_handling, 0);
    }

    return TRUE;
}

/* ============================================================================
 * Application Message Dispatch Table
 * ============================================================================ */

typedef LRESULT (*AppMessageHandler)(HWND hwnd);

typedef struct {
    UINT msgId;
    AppMessageHandler handler;
    const char* description;
} AppMessageDispatchEntry;

static const AppMessageDispatchEntry APP_MESSAGE_DISPATCH_TABLE[] = {
    {WM_APP_DISPLAY_CHANGED,       HandleAppDisplayChanged,       "Display settings reload"},
    {WM_APP_TIMER_CHANGED,         HandleAppTimerChanged,         "Timer settings reload"},
    {WM_APP_POMODORO_CHANGED,      HandleAppPomodoroChanged,      "Pomodoro settings reload"},
    {WM_APP_NOTIFICATION_CHANGED,  HandleAppNotificationChanged,  "Notification settings reload"},
    {WM_APP_HOTKEYS_CHANGED,       HandleAppHotkeysChanged,       "Hotkey assignments reload"},
    {WM_APP_RECENTFILES_CHANGED,   HandleAppRecentFilesChanged,   "Recent files list reload"},
    {WM_APP_COLORS_CHANGED,        HandleAppColorsChanged,        "Color options reload"},
    {WM_APP_ANIM_SPEED_CHANGED,    HandleAppAnimSpeedChanged,     "Animation speed reload"},
    {WM_APP_ANIM_PATH_CHANGED,     HandleAppAnimPathChanged,      "Animation path reload"},
    {0,                             NULL,                          NULL}
};

static inline BOOL DispatchAppMessage(HWND hwnd, UINT msg) {
    for (const AppMessageDispatchEntry* entry = APP_MESSAGE_DISPATCH_TABLE; entry->handler; entry++) {
        if (entry->msgId == msg) {
            entry->handler(hwnd);
            return TRUE;
        }
    }
    return FALSE;
}

/* Plugin exit message handler (for <exit> tag) */
static LRESULT HandlePluginExitMessage(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    HandlePluginExit(hwnd);
    return 0;
}

/* ============================================================================
 * Message Dispatch Table
 * ============================================================================ */

typedef LRESULT (*MessageHandler)(HWND hwnd, WPARAM wp, LPARAM lp);

typedef struct {
    UINT msg;
    MessageHandler handler;
    const char* description;
} MessageDispatchEntry;

static const MessageDispatchEntry MESSAGE_DISPATCH_TABLE[] = {
    {WM_CREATE, HandleCreate, "Window creation"},
    {WM_SETCURSOR, HandleSetCursor, "Cursor management"},
    {WM_LBUTTONDOWN, HandleLButtonDown, "Mouse left button down"},
    {WM_LBUTTONUP, HandleLButtonUp, "Mouse left button up"},
    {WM_LBUTTONDBLCLK, HandleLButtonDblClk, "Mouse double-click"},
    {WM_RBUTTONDOWN, HandleRButtonDown, "Mouse right button down"},
    {WM_RBUTTONUP, HandleRButtonUp, "Mouse right button up"},
    {WM_MOUSEWHEEL, HandleMouseWheel, "Mouse wheel scroll"},
    {WM_MOUSEMOVE, HandleMouseMove, "Mouse movement"},
    {WM_PAINT, HandlePaint, "Window painting"},
    {WM_TIMER, HandleTimer, "Timer tick"},
    {WM_DESTROY, HandleDestroy, "Window destruction"},
    {CLOCK_WM_TRAYICON, HandleTrayIcon, "Tray icon message"},
    {WM_COMMAND, HandleCommand, "Menu command"},
    {WM_WINDOWPOSCHANGED, HandleWindowPosChanged, "Window position changed"},
    {WM_DISPLAYCHANGE, HandleDisplayChange, "Display configuration changed"},
    {WM_MENUSELECT, HandleMenuSelect, "Menu item selection"},
    {WM_MEASUREITEM, HandleMeasureItem, "Owner-drawn menu measurement"},
    {WM_DRAWITEM, HandleDrawItem, "Owner-drawn menu rendering"},
    {WM_EXITMENULOOP, HandleExitMenuLoop, "Menu loop exit"},
    {WM_CLOSE, HandleClose, "Window close"},
    {WM_KEYDOWN, HandleKeyDown, "Key down"},
    {WM_HOTKEY, HandleHotkey, "Global hotkey"},
    {WM_COPYDATA, HandleCopyData, "Inter-process communication"},
    {WM_POWERBROADCAST, HandlePowerBroadcast, "Power management events"},
    {WM_APP_QUICK_COUNTDOWN_INDEX, HandleQuickCountdownIndex, "Quick countdown by index"},
    {WM_APP_SHOW_CLI_HELP, HandleShowCliHelp, "Show CLI help"},
    {WM_USER + 100, HandleTrayUpdateIcon, "Tray icon update"},
    {WM_APP + 1, HandleAppReregisterHotkeys, "Hotkey re-registration"},
    {CLOCK_WM_ANIMATION_PREVIEW_LOADED, HandleAnimationPreviewLoaded, "Animation preview loaded"},
    {CLOCK_WM_PLUGIN_EXIT, HandlePluginExitMessage, "Plugin exit via <exit> tag"},
    {0, NULL, NULL}
};

/* ============================================================================
 * Main Window Procedure
 * ============================================================================ */

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_TASKBARCREATED) {
        extern void RecreateTaskbarIcon(HWND, HINSTANCE);
        RecreateTaskbarIcon(hwnd, GetModuleHandle(NULL));
        return 0;
    }
    
    /* Handle WM_MOUSEACTIVATE to prevent window activation in non-topmost mode */
    if (msg == WM_MOUSEACTIVATE) {
        extern BOOL CLOCK_WINDOW_TOPMOST;
        if (!CLOCK_EDIT_MODE && !CLOCK_WINDOW_TOPMOST) {
            return MA_NOACTIVATE;  /* Don't activate window on click */
        }
    }
    
    /* Handle WM_NCHITTEST for click-through in non-edit mode */
    if (msg == WM_NCHITTEST) {
        if (!CLOCK_EDIT_MODE) {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            
            /* Update region positions based on current window position */
            RECT rcWindow;
            GetWindowRect(hwnd, &rcWindow);
            UpdateRegionPositions(rcWindow.left, rcWindow.top);
            
            /* Check if cursor is over a clickable region */
            const ClickableRegion* region = GetClickableRegionAt(pt);
            if (region) {
                return HTCLIENT;  /* Allow click */
            }
            return HTTRANSPARENT;  /* Pass through */
        }
    }
    
    if (DispatchAppMessage(hwnd, msg)) {
        return 0;
    }
    
    for (const MessageDispatchEntry* entry = MESSAGE_DISPATCH_TABLE; entry->handler; entry++) {
        if (entry->msg == msg) {
            return entry->handler(hwnd, wp, lp);
        }
    }
    
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ============================================================================
 * Public API - Timer Action Functions
 * ============================================================================ */

void ToggleShowTimeMode(HWND hwnd) {
    CleanupBeforeTimerAction();
    
    if (!CLOCK_SHOW_CURRENT_TIME) {
        extern POMODORO_PHASE current_pomodoro_phase;
        extern int current_pomodoro_time_index, complete_pomodoro_cycles;
        
        if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
            current_pomodoro_phase = POMODORO_PHASE_IDLE;
            current_pomodoro_time_index = 0;
            complete_pomodoro_cycles = 0;
        }
        
        TimerModeParams params = {0, TRUE, FALSE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_SHOW_TIME, &params);
        
        // Ensure timer is running
        KillTimer(hwnd, 1);
        ResetTimerWithInterval(hwnd);
    }
}

void StartCountUp(HWND hwnd) {
    CleanupBeforeTimerAction();
    
    extern POMODORO_PHASE current_pomodoro_phase;
    extern int current_pomodoro_time_index, complete_pomodoro_cycles;
    
    if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
        current_pomodoro_phase = POMODORO_PHASE_IDLE;
        current_pomodoro_time_index = 0;
        complete_pomodoro_cycles = 0;
    }
    
    TimerModeParams params = {0, TRUE, FALSE, TRUE};
    SwitchTimerMode(hwnd, TIMER_MODE_COUNTUP, &params);
    
    // Ensure timer is running
    KillTimer(hwnd, 1);
    ResetTimerWithInterval(hwnd);
}

void StartDefaultCountDown(HWND hwnd) {
    CleanupBeforeTimerAction();
    
    extern BOOL countdown_message_shown;
    extern POMODORO_PHASE current_pomodoro_phase;
    extern int current_pomodoro_time_index, complete_pomodoro_cycles;
    
    countdown_message_shown = FALSE;
    
    if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
        current_pomodoro_phase = POMODORO_PHASE_IDLE;
        current_pomodoro_time_index = 0;
        complete_pomodoro_cycles = 0;
    }
    
    if (g_AppConfig.timer.default_start_time > 0) {
        TimerModeParams params = {g_AppConfig.timer.default_start_time, TRUE, FALSE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
        
        // Ensure timer is running
        KillTimer(hwnd, 1);
        ResetTimerWithInterval(hwnd);
    } else {
        PostMessage(hwnd, WM_COMMAND, CLOCK_IDM_CUSTOM_COUNTDOWN, 0);
    }
}

void StartPomodoroTimer(HWND hwnd) {
    CleanupBeforeTimerAction();
    
    if (!IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_SHOW);

    extern void InitializePomodoro(void);
    InitializePomodoro();

    CLOCK_SHOW_CURRENT_TIME = FALSE;
    CLOCK_COUNT_UP = FALSE;
    CLOCK_IS_PAUSED = FALSE;

    KillTimer(hwnd, 1);
    ResetTimerWithInterval(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

#define TIMER_ID_TRANSITION_END 100
extern BOOL g_IsTransitioning;

void ToggleEditMode(HWND hwnd) {
    if (CLOCK_EDIT_MODE) {
        EndEditMode(hwnd);
    } else {
        StartEditMode(hwnd);
    }
}

void TogglePauseResume(HWND hwnd) {
    CleanupBeforeTimerAction();
    TogglePauseResumeTimer(hwnd);
}

void RestartCurrentTimer(HWND hwnd) {
    extern void StopNotificationSound(void);
    extern int message_shown, countdown_message_shown, countup_message_shown;
    extern int countdown_elapsed_time, countup_elapsed_time;
    extern void ResetMillisecondAccumulator(void);
    
    CloseAllNotifications(); // Centralized cleanup
    StopNotificationSound();
    
    CleanupBeforeTimerAction();
    
    if (!CLOCK_SHOW_CURRENT_TIME) {
        message_shown = FALSE;
        countdown_message_shown = FALSE;
        countup_message_shown = FALSE;
        
        if (CLOCK_COUNT_UP) {
            countdown_elapsed_time = 0;
            countup_elapsed_time = 0;
        } else {
            countdown_elapsed_time = 0;
            extern int elapsed_time;
            elapsed_time = 0;
        }
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();
        
        // Restart the timer after resetting pause state
        KillTimer(hwnd, 1);
        ResetTimerWithInterval(hwnd);
        
        InvalidateRect(hwnd, NULL, TRUE);
    }
    
    extern void HandleWindowReset(HWND);
    HandleWindowReset(hwnd);
}

void StartQuickCountdownByIndex(HWND hwnd, int index) {
    if (index <= 0) return;

    CleanupBeforeTimerAction();

    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;

    int zeroBased = index - 1;
    if (zeroBased >= 0 && zeroBased < time_options_count) {
        StartCountdownWithTime(hwnd, time_options[zeroBased]);
    } else {
        StartDefaultCountDown(hwnd);
    }
}

void CleanupBeforeTimerAction(void) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    CloseAllNotifications();
    
    // Check if plugin text has <catime> tag - if so, keep plugin active
    // The time will be embedded within the plugin text via the tag
    if (!PluginData_HasCatimeTag()) {
        // No <catime> tag, stop all plugins and disable plugin data mode
        PluginManager_StopAllPlugins();
        PluginData_SetActive(FALSE);
    }
    // If <catime> tag is present, keep plugin running and data active
}

BOOL StartCountdownWithTime(HWND hwnd, int seconds) {
    if (seconds <= 0) return FALSE;
    
    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;
    
    if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
        current_pomodoro_phase = POMODORO_PHASE_IDLE;
        current_pomodoro_time_index = 0;
        complete_pomodoro_cycles = 0;
    }
    
    TimerModeParams params = {seconds, TRUE, TRUE, TRUE};
    BOOL result = SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
    
    // Ensure timer is running
    KillTimer(hwnd, 1);
    ResetTimerWithInterval(hwnd);
    
    return result;
}

void ToggleMilliseconds(HWND hwnd) {
    extern void WriteConfigShowMilliseconds(BOOL showMilliseconds);
    extern void ResetTimerWithInterval(HWND hwnd);
    
    BOOL newState = !g_AppConfig.display.time_format.show_milliseconds;
    WriteConfigShowMilliseconds(newState);
    
    /* Reset timer with new interval (10ms for milliseconds, 1000ms without) */
    ResetTimerWithInterval(hwnd);
    
    InvalidateRect(hwnd, NULL, TRUE);
}

void ToggleTopmost(HWND hwnd) {
    extern void WriteConfigTopmost(const char* value);
    // Use "true"/"false" literals to avoid dependency on specific header macros if not present
    WriteConfigTopmost(!CLOCK_WINDOW_TOPMOST ? "true" : "false");
}

void ToggleWindowVisibility(HWND hwnd) {
    if (IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_HIDE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    }
}

