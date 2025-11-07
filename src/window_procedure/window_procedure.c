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
#include <string.h>

/* ============================================================================
 * External Declarations
 * ============================================================================ */

extern UINT WM_TASKBARCREATED;
extern int time_options[];
extern int time_options_count;
extern BOOL PREVIOUS_TOPMOST_STATE;

#define OPACITY_FULL 255
#define TIMER_ID_FORCE_REDRAW 999

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
    {WM_HOTKEY, HandleHotkey, "Global hotkey"},
    {WM_COPYDATA, HandleCopyData, "Inter-process communication"},
    {WM_APP_QUICK_COUNTDOWN_INDEX, HandleQuickCountdownIndex, "Quick countdown by index"},
    {WM_APP_SHOW_CLI_HELP, HandleShowCliHelp, "Show CLI help"},
    {WM_USER + 100, HandleTrayUpdateIcon, "Tray icon update"},
    {WM_APP + 1, HandleAppReregisterHotkeys, "Hotkey re-registration"},
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
        TimerModeParams params = {0, TRUE, FALSE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_SHOW_TIME, &params);
    }
}

void StartCountUp(HWND hwnd) {
    CleanupBeforeTimerAction();
    
    TimerModeParams params = {0, TRUE, FALSE, TRUE};
    SwitchTimerMode(hwnd, TIMER_MODE_COUNTUP, &params);
}

void StartDefaultCountDown(HWND hwnd) {
    CleanupBeforeTimerAction();
    
    extern BOOL countdown_message_shown;
    extern void ReadNotificationTypeConfig(void);
    countdown_message_shown = FALSE;
    ReadNotificationTypeConfig();
    
    if (CLOCK_DEFAULT_START_TIME > 0) {
        TimerModeParams params = {CLOCK_DEFAULT_START_TIME, TRUE, FALSE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
    } else {
        PostMessage(hwnd, WM_COMMAND, 101, 0);
    }
}

void StartPomodoroTimer(HWND hwnd) {
    CleanupBeforeTimerAction();
    PostMessage(hwnd, WM_COMMAND, CLOCK_IDM_POMODORO_START, 0);
}

void ToggleEditMode(HWND hwnd) {
    CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
    
    if (CLOCK_EDIT_MODE) {
        PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
        
        if (!CLOCK_WINDOW_TOPMOST) {
            SetWindowTopmost(hwnd, TRUE);
        }
        
        SetBlurBehind(hwnd, TRUE);
        SetClickThrough(hwnd, FALSE);
        
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    } else {
        extern char CLOCK_TEXT_COLOR[10];
        SetBlurBehind(hwnd, FALSE);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), OPACITY_FULL, LWA_COLORKEY);
        
        SetClickThrough(hwnd, TRUE);
        
        if (!PREVIOUS_TOPMOST_STATE) {
            SetWindowTopmost(hwnd, FALSE);
            
            InvalidateRect(hwnd, NULL, TRUE);
            RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
            KillTimer(hwnd, TIMER_ID_FORCE_REDRAW);
            SetTimer(hwnd, TIMER_ID_FORCE_REDRAW, 150, NULL);
            return;
        }
        
        SaveWindowSettings(hwnd);
        WriteConfigColor(CLOCK_TEXT_COLOR);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
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
        InvalidateRect(hwnd, NULL, TRUE);
    }
    
    extern void HandleWindowReset(HWND);
    HandleWindowReset(hwnd);
}

void StartQuickCountdownByIndex(HWND hwnd, int index) {
    if (index <= 0) return;

    CleanupBeforeTimerAction();

    extern BOOL countdown_message_shown;
    extern void ReadNotificationTypeConfig(void);
    countdown_message_shown = FALSE;
    ReadNotificationTypeConfig();

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
    return SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
}

