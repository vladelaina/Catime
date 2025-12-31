/**
 * @file window_commands_timer.c
 * @brief Timer and Pomodoro related command handlers
 */

#include "window_procedure/window_commands.h"
#include "window_procedure/window_helpers.h"
#include "window_procedure/window_procedure.h"
#include "window_procedure/window_events.h"
#include "window_procedure/window_utils.h"
#include "tray/tray_events.h"
#include "dialog/dialog_procedure.h"
#include "dialog/dialog_common.h"
#include "timer/timer.h"
#include "timer/timer_events.h"
#include "config.h"
#include "window.h"
#include "pomodoro.h"
#include "notification.h"
#include "utils/time_parser.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <string.h>

extern wchar_t inputText[256];
extern int time_options[];
extern int time_options_count;

/* ============================================================================
 * Custom Countdown
 * ============================================================================ */

LRESULT CmdCustomCountdown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    /* Don't change timer state here - wait until user confirms input
     * State will be changed in StartCountdownWithTime when dialog returns result */
    
    /* Use modeless dialog - result handled via WM_DIALOG_COUNTDOWN */
    ShowCountdownInputDialog(hwnd);
    return 0;
}

/* ============================================================================
 * Timer Mode Commands
 * ============================================================================ */

LRESULT CmdShowCurrentTime(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleShowTimeMode(hwnd);
    return 0;
}

LRESULT CmdCountUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    StartCountUp(hwnd);
    return 0;
}

LRESULT CmdCountUpStart(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CleanupBeforeTimerAction();
    
    if (!CLOCK_COUNT_UP) {
        TimerModeParams params = {0, TRUE, TRUE, TRUE};  /* showWindow = TRUE */
        SwitchTimerMode(hwnd, TIMER_MODE_COUNTUP, &params);
        KillTimer(hwnd, 1);
        ResetTimerWithInterval(hwnd);
    } else {
        TogglePauseResumeTimer(hwnd);
    }
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

LRESULT CmdCountUpReset(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CleanupBeforeTimerAction();
    ResetTimer();
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

LRESULT CmdCountdownReset(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CleanupBeforeTimerAction();
    if (CLOCK_COUNT_UP) CLOCK_COUNT_UP = FALSE;
    ResetTimer();
    KillTimer(hwnd, 1);
    ResetTimerWithInterval(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
    HandleWindowReset(hwnd);
    return 0;
}

LRESULT CmdPauseResume(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    TogglePauseResumeTimer(hwnd);
    return 0;
}

LRESULT CmdRestartTimer(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    RestartCurrentTimer(hwnd);
    return 0;
}

/* ============================================================================
 * Time Format Commands (Simplified)
 * ============================================================================ */

LRESULT CmdTimeFormat(HWND hwnd, TimeFormatType format) {
    WriteConfigTimeFormat(format);
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

LRESULT CmdToggleMilliseconds(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleMilliseconds(hwnd);
    return 0;
}

LRESULT Cmd24HourFormat(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleConfigBool(hwnd, CFG_KEY_USE_24HOUR, &CLOCK_USE_24HOUR, TRUE);
    return 0;
}

LRESULT CmdShowSeconds(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleConfigBool(hwnd, CFG_KEY_SHOW_SECONDS, &CLOCK_SHOW_SECONDS, TRUE);
    return 0;
}

/* ============================================================================
 * Timeout Action Commands (Simplified with table)
 * ============================================================================ */

typedef struct {
    TimeoutActionType action;
    const char* configStr;
} TimeoutActionEntry;

static const TimeoutActionEntry TIMEOUT_ACTIONS[] = {
    {TIMEOUT_ACTION_SHOW_TIME, "SHOW_TIME"},
    {TIMEOUT_ACTION_COUNT_UP,  "COUNT_UP"},
    {TIMEOUT_ACTION_MESSAGE,   "MESSAGE"},
    {TIMEOUT_ACTION_LOCK,      "LOCK"},
    {TIMEOUT_ACTION_SHUTDOWN,  "SHUTDOWN"},
    {TIMEOUT_ACTION_RESTART,   "RESTART"},
    {TIMEOUT_ACTION_SLEEP,     "SLEEP"},
};

LRESULT CmdSetTimeoutAction(HWND hwnd, TimeoutActionType action) {
    (void)hwnd;
    extern TimeoutActionType CLOCK_TIMEOUT_ACTION;
    CLOCK_TIMEOUT_ACTION = action;
    
    for (size_t i = 0; i < sizeof(TIMEOUT_ACTIONS)/sizeof(TIMEOUT_ACTIONS[0]); i++) {
        if (TIMEOUT_ACTIONS[i].action == action) {
            WriteConfigTimeoutAction(TIMEOUT_ACTIONS[i].configStr);
            break;
        }
    }
    return 0;
}

/* ============================================================================
 * Startup Mode Commands (Simplified)
 * ============================================================================ */

LRESULT CmdSetStartupMode(HWND hwnd, const char* mode) {
    SetStartupMode(hwnd, mode);
    return 0;
}

LRESULT CmdSetCountdownTime(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    /* Use modeless dialog - config saved directly by dialog */
    ShowStartupTimeDialog(hwnd);
    return 0;
}

/* ============================================================================
 * Pomodoro Commands
 * ============================================================================ */

LRESULT CmdPomodoroStart(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    StartPomodoroTimer(hwnd);
    return 0;
}

LRESULT CmdPomodoroReset(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CleanupBeforeTimerAction();

    extern POMODORO_PHASE current_pomodoro_phase;
    extern int current_pomodoro_time_index, complete_pomodoro_cycles;

    current_pomodoro_phase = POMODORO_PHASE_IDLE;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;

    ResetTimer();

    if (CLOCK_TOTAL_TIME == g_AppConfig.pomodoro.work_time ||
        CLOCK_TOTAL_TIME == g_AppConfig.pomodoro.short_break ||
        CLOCK_TOTAL_TIME == g_AppConfig.pomodoro.long_break) {
        KillTimer(hwnd, 1);
        ResetTimerWithInterval(hwnd);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
    HandleWindowReset(hwnd);
    return 0;
}

LRESULT CmdPomodoroLoopCount(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowPomodoroLoopDialog(hwnd);
    return 0;
}

LRESULT CmdPomodoroCombo(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowPomodoroComboDialog(hwnd);
    return 0;
}

/* ============================================================================
 * Time Options Configuration
 * ============================================================================ */

LRESULT CmdModifyTimeOptions(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    /* Use modeless dialog - result handled via WM_DIALOG_SHORTCUT */
    ShowShortcutTimeDialog(hwnd);
    return 0;
}

LRESULT CmdModifyDefaultTime(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    /* Use modeless dialog - config saved directly by dialog */
    ShowStartupTimeDialog(hwnd);
    return 0;
}

/* ============================================================================
 * Range Command Handlers
 * ============================================================================ */

BOOL HandleQuickCountdown(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    if (index >= 0 && index < time_options_count && time_options[index] > 0) {
        CleanupBeforeTimerAction();
        StartCountdownWithTime(hwnd, time_options[index]);
    }
    return TRUE;
}

BOOL HandlePomodoroTime(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    HandlePomodoroTimeConfig(hwnd, index);
    return TRUE;
}

/* ============================================================================
 * Pomodoro Time Configuration
 * ============================================================================ */

BOOL HandlePomodoroTimeConfig(HWND hwnd, int selectedIndex) {
    if (selectedIndex < 0 || selectedIndex >= g_AppConfig.pomodoro.times_count) {
        return FALSE;
    }

    /* Use modeless dialog - config saved directly by dialog */
    ShowPomodoroTimeEditDialog(hwnd, selectedIndex);
    return TRUE;
}
