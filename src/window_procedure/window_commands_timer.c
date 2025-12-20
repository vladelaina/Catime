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
    if (CLOCK_SHOW_CURRENT_TIME) {
        CLOCK_SHOW_CURRENT_TIME = FALSE;
        extern time_t CLOCK_LAST_TIME_UPDATE;
        CLOCK_LAST_TIME_UPDATE = 0;
        KillTimer(hwnd, 1);
    }
    
    int total_seconds = 0;
    if (ValidatedTimeInputLoop(hwnd, CLOCK_IDD_DIALOG1, &total_seconds)) {
        CleanupBeforeTimerAction();
        StartCountdownWithTime(hwnd, total_seconds);
    }
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
        TimerModeParams params = {0, TRUE, FALSE, TRUE};
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
    int total_seconds = 0;
    if (ValidatedTimeInputLoop(hwnd, CLOCK_IDD_STARTUP_DIALOG, &total_seconds)) {
        WriteConfigDefaultStartTime(total_seconds);
        return CmdSetStartupMode(hwnd, "COUNTDOWN");
    }
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

    while (1) {
        ClearInputBuffer(inputText, sizeof(inputText));
        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_SHORTCUT_DIALOG),
                       hwnd, DlgProc, (LPARAM)CLOCK_IDD_SHORTCUT_DIALOG);
        
        if (isAllSpacesOnly(inputText)) break;
        
        Utf8String us = ToUtf8(inputText);
        char inputTextA[MAX_PATH];
        strcpy_s(inputTextA, sizeof(inputTextA), us.buf);
        
        char* token = strtok(inputTextA, " ");
        char options[256] = {0};
        int valid = 1, count = 0;
        
        while (token && count < MAX_TIME_OPTIONS) {
            int seconds = 0;
            if (!TimeParser_ParseBasic(token, &seconds) || seconds <= 0) {
                valid = 0;
                break;
            }
            
            if (count > 0) strcat_s(options, sizeof(options), ",");
            
            char secondsStr[32];
            snprintf(secondsStr, sizeof(secondsStr), "%d", seconds);
            strcat_s(options, sizeof(options), secondsStr);
            count++;
            token = strtok(NULL, " ");
        }
        
        if (valid && count > 0) {
            CleanupBeforeTimerAction();
            WriteConfigTimeOptions(options);
            break;
        } else {
            ShowErrorDialog(hwnd);
        }
    }
    return 0;
}

LRESULT CmdModifyDefaultTime(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    int total_seconds = 0;
    if (ValidatedTimeInputLoop(hwnd, CLOCK_IDD_STARTUP_DIALOG, &total_seconds)) {
        CleanupBeforeTimerAction();
        WriteConfigDefaultStartTime(total_seconds);
        WriteConfigStartupMode("COUNTDOWN");
    }
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

    extern int g_pomodoroSelectedIndex;
    g_pomodoroSelectedIndex = selectedIndex;

    memset(inputText, 0, sizeof(inputText));
    DialogBoxParamW(GetModuleHandle(NULL),
             MAKEINTRESOURCEW(CLOCK_IDD_POMODORO_TIME_DIALOG),
             hwnd, DlgProc, (LPARAM)CLOCK_IDD_POMODORO_TIME_DIALOG);
    
    if (inputText[0] && !isAllSpacesOnly(inputText)) {
        int total_seconds = 0;

        char inputTextA[256];
        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
        extern int ParseInput(const char*, int*);
        if (ParseInput(inputTextA, &total_seconds)) {
            g_AppConfig.pomodoro.times[selectedIndex] = total_seconds;
            
            WriteConfigPomodoroTimeOptions(g_AppConfig.pomodoro.times, g_AppConfig.pomodoro.times_count);
            
            if (selectedIndex == 0) g_AppConfig.pomodoro.work_time = total_seconds;
            else if (selectedIndex == 1) g_AppConfig.pomodoro.short_break = total_seconds;
            else if (selectedIndex == 2) g_AppConfig.pomodoro.long_break = total_seconds;

            g_pomodoroSelectedIndex = -1;
            return TRUE;
        }
    }

    g_pomodoroSelectedIndex = -1;
    return FALSE;
}
