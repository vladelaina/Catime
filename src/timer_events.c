/**
 * @file timer_events.c
 * @brief Core timer event handling with Pomodoro technique support and timeout actions
 * Manages timer state transitions, notifications, and various completion behaviors
 */
#include <windows.h>
#include <stdlib.h>
#include "../include/timer_events.h"
#include "../include/timer.h"
#include "../include/language.h"
#include "../include/notification.h"
#include "../include/pomodoro.h"
#include "../include/config.h"
#include <stdio.h>
#include <string.h>
#include "../include/window.h"
#include "audio_player.h"

#define MAX_POMODORO_TIMES 10
extern int POMODORO_TIMES[MAX_POMODORO_TIMES];
extern int POMODORO_TIMES_COUNT;

/** @brief Current index in the Pomodoro time sequence */
int current_pomodoro_time_index = 0;

/** @brief Current phase of Pomodoro cycle (work/break/idle) */
POMODORO_PHASE current_pomodoro_phase = POMODORO_PHASE_IDLE;

/** @brief Number of completed Pomodoro cycles */
int complete_pomodoro_cycles = 0;

extern void ShowNotification(HWND hwnd, const wchar_t* message);

extern int elapsed_time;
extern BOOL message_shown;

/** @brief Localized timeout message strings */
extern char CLOCK_TIMEOUT_MESSAGE_TEXT[100];
extern char POMODORO_TIMEOUT_MESSAGE_TEXT[100];
extern char POMODORO_CYCLE_COMPLETE_TEXT[100];

/** @brief Timer application states for different operating modes */
typedef enum {
    CLOCK_STATE_IDLE,       /**< Timer not running */
    CLOCK_STATE_COUNTDOWN,  /**< Standard countdown timer */
    CLOCK_STATE_COUNTUP,    /**< Stopwatch mode */
    CLOCK_STATE_POMODORO    /**< Pomodoro technique mode */
} ClockState;

/** @brief State tracking for Pomodoro cycle progression */
typedef struct {
    BOOL isLastCycle;   /**< True if this is the final cycle */
    int cycleIndex;     /**< Current cycle number (0-based) */
    int totalCycles;    /**< Total number of cycles to complete */
} PomodoroState;

extern HWND g_hwnd;
extern ClockState g_clockState;
extern PomodoroState g_pomodoroState;

extern void ShowTrayNotification(HWND hwnd, const char* message);
extern void OpenFileByPath(const char* filePath);
extern void OpenWebsite(const char* url);
extern void SleepComputer(void);
extern void ShutdownComputer(void);
extern void RestartComputer(void);
extern void SetTimeDisplay(void);
extern void ShowCountUp(HWND hwnd);

extern void StopNotificationSound(void);

/**
 * @brief Convert UTF-8 string to Windows wide character string
 * @param utf8String Input UTF-8 encoded string
 * @return Allocated wide char string or NULL on failure
 * Caller must free the returned string
 */
static wchar_t* Utf8ToWideChar(const char* utf8String) {
    if (!utf8String || utf8String[0] == '\0') {
        return NULL;
    }
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, NULL, 0);
    if (size_needed == 0) {
        return NULL;
    }
    wchar_t* wideString = (wchar_t*)malloc(size_needed * sizeof(wchar_t));
    if (!wideString) {
        return NULL;
    }
    int result = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, wideString, size_needed);
    if (result == 0) {
        free(wideString);
        return NULL;
    }
    return wideString;
}

/**
 * @brief Show notification with optional sound based on timeout action
 * @param hwnd Window handle for notification display
 * @param message Wide character message to display
 * Plays notification sound only for message-type timeout actions
 */
static void ShowLocalizedNotification(HWND hwnd, const wchar_t* message) {
    if (!message || message[0] == L'\0') {
        return;
    }

    ShowNotification(hwnd, message);
            
    /** Play sound only for message notifications, not for actions */
    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE) {
        ReadNotificationSoundConfig();
        
        PlayNotificationSound(hwnd);
    }
}

/**
 * @brief Initialize Pomodoro technique timer with configured time sequence
 * Sets up work phase with first configured time interval or 25-minute default
 */
void InitializePomodoro(void) {
    current_pomodoro_phase = POMODORO_PHASE_WORK;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;
    
    /** Use first configured time or fallback to 25 minutes (1500 seconds) */
    if (POMODORO_TIMES_COUNT > 0) {
        CLOCK_TOTAL_TIME = POMODORO_TIMES[0];
    } else {
        CLOCK_TOTAL_TIME = 1500;
    }
    
    countdown_elapsed_time = 0;
    countdown_message_shown = FALSE;
}

/**
 * @brief Main timer event dispatcher handling window positioning and timer updates
 * @param hwnd Main window handle
 * @param wp Timer ID identifying the event type
 * @return TRUE if event was handled, FALSE otherwise
 * Manages multiple timer types: positioning retries, main countdown, and special behaviors
 */
BOOL HandleTimerEvent(HWND hwnd, WPARAM wp) {
    /** Timer 999: Topmost window positioning retry mechanism */
    if (wp == 999) {
        static int s_topmost_retry_remaining = 0;
        if (s_topmost_retry_remaining == 0) {
            s_topmost_retry_remaining = 3;
        }

        if (CLOCK_WINDOW_TOPMOST) {
            SetWindowTopmost(hwnd, TRUE);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
        if (!IsWindowVisible(hwnd)) {
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        }

        /** Retry up to 3 times with 1.5 second intervals */
        s_topmost_retry_remaining--;
        if (s_topmost_retry_remaining > 0) {
            SetTimer(hwnd, 999, 1500, NULL);
        } else {
            KillTimer(hwnd, 999);
        }
        return TRUE;
    }

    /** Timer 1001: Desktop attachment retry for non-topmost windows */
    if (wp == 1001) {
        static int s_desktop_retry_remaining = 0;
        if (s_desktop_retry_remaining == 0) {
            s_desktop_retry_remaining = 3;
        }

        if (!CLOCK_WINDOW_TOPMOST) {
            ReattachToDesktop(hwnd);
            if (!IsWindowVisible(hwnd)) {
                ShowWindow(hwnd, SW_SHOWNOACTIVATE);
            }
        }

        s_desktop_retry_remaining--;
        if (s_desktop_retry_remaining > 0) {
            SetTimer(hwnd, 1001, 1500, NULL);
        } else {
            KillTimer(hwnd, 1001);
        }
        return TRUE;
    }
    /** Timer 1002: Force desktop reattachment with immediate redraw */
    if (wp == 1002) {
        KillTimer(hwnd, 1002);
        extern void ReattachToDesktop(HWND);
        ReattachToDesktop(hwnd);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(hwnd, NULL, TRUE);
        RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
        return TRUE;
    }
    
    /** Timer 1003: Font path validation and auto-fix (every 2 seconds) */
    if (wp == 1003) {
        extern char FONT_FILE_NAME[100];
        extern BOOL CheckAndFixFontPath(void);
        
        /** Check if current font path is valid, auto-fix if needed */
        if (CheckAndFixFontPath()) {
            /** Font path was fixed, force window redraw */
            InvalidateRect(hwnd, NULL, TRUE);
        }
        
        /** Continue periodic checking */
        SetTimer(hwnd, 1003, 2000, NULL);
        return TRUE;
    }
    /** Timer 1: Main application timer (1-second interval) */
    if (wp == 1) {
        /** Clock mode: display current time */
        if (CLOCK_SHOW_CURRENT_TIME) {
            extern int last_displayed_second;
            last_displayed_second = -1;  /**< Force time redraw */
            
            InvalidateRect(hwnd, NULL, TRUE);
            return TRUE;
        }

        /** Skip timer updates when paused */
        if (CLOCK_IS_PAUSED) {
            return TRUE;
        }

        /** Count-up mode: increment elapsed time */
        if (CLOCK_COUNT_UP) {
            countup_elapsed_time++;
            InvalidateRect(hwnd, NULL, TRUE);
        } else {
            /** Countdown mode: process timer completion and Pomodoro logic */
            if (countdown_elapsed_time < CLOCK_TOTAL_TIME) {
                countdown_elapsed_time++;
                if (countdown_elapsed_time >= CLOCK_TOTAL_TIME && !countdown_message_shown) {
                    countdown_message_shown = TRUE;

                    ReadNotificationMessagesConfig();
                    ReadNotificationTypeConfig();
                    
                    wchar_t* timeoutMsgW = NULL;

                    /** Active Pomodoro sequence: handle work/break transitions */
                    if (current_pomodoro_phase != POMODORO_PHASE_IDLE && 
                        POMODORO_TIMES_COUNT > 0 && 
                        current_pomodoro_time_index < POMODORO_TIMES_COUNT &&
                        CLOCK_TOTAL_TIME == POMODORO_TIMES[current_pomodoro_time_index]) {
                        
                        timeoutMsgW = Utf8ToWideChar(POMODORO_TIMEOUT_MESSAGE_TEXT);
                        
                        if (timeoutMsgW) {
                            ShowLocalizedNotification(hwnd, timeoutMsgW);
                        } else {
                            ShowLocalizedNotification(hwnd, L"番茄钟时间到！");
                        }
                        
                        /** Move to next time interval in sequence */
                        current_pomodoro_time_index++;
                        
                        /** Check if sequence is complete */
                        if (current_pomodoro_time_index >= POMODORO_TIMES_COUNT) {
                            current_pomodoro_time_index = 0;
                            
                            complete_pomodoro_cycles++;
                            
                            /** All cycles completed - end Pomodoro session */
                            if (complete_pomodoro_cycles >= POMODORO_LOOP_COUNT) {
                                countdown_elapsed_time = 0;
                                countdown_message_shown = FALSE;
                                CLOCK_TOTAL_TIME = 0;
                                
                                current_pomodoro_phase = POMODORO_PHASE_IDLE;
                                
                                wchar_t* cycleCompleteMsgW = Utf8ToWideChar(POMODORO_CYCLE_COMPLETE_TEXT);
                                if (cycleCompleteMsgW) {
                                    ShowLocalizedNotification(hwnd, cycleCompleteMsgW);
                                    free(cycleCompleteMsgW);
                                } else {
                                    ShowLocalizedNotification(hwnd, L"所有番茄钟循环完成！");
                                }
                                
                                CLOCK_COUNT_UP = FALSE;
                                CLOCK_SHOW_CURRENT_TIME = FALSE;
                                message_shown = TRUE;
                                
                                InvalidateRect(hwnd, NULL, TRUE);
                                KillTimer(hwnd, 1);
                                if (timeoutMsgW) free(timeoutMsgW);
                                return TRUE;
                            }
                        }
                        
                        /** Start next interval in sequence */
                        CLOCK_TOTAL_TIME = POMODORO_TIMES[current_pomodoro_time_index];
                        countdown_elapsed_time = 0;
                        countdown_message_shown = FALSE;
                        
                        /** Show cycle progress message when starting new cycle */
                        if (current_pomodoro_time_index == 0 && complete_pomodoro_cycles > 0) {
                            wchar_t cycleMsg[100];
                            const wchar_t* formatStr = GetLocalizedString(L"开始第 %d 轮番茄钟", L"Starting Pomodoro cycle %d");
                            swprintf(cycleMsg, 100, formatStr, complete_pomodoro_cycles + 1);
                            ShowLocalizedNotification(hwnd, cycleMsg);
                        }
                        
                        InvalidateRect(hwnd, NULL, TRUE);
                    } else {
                        /** Regular countdown timer: show message and execute timeout action */
                        timeoutMsgW = Utf8ToWideChar(CLOCK_TIMEOUT_MESSAGE_TEXT);
                        
                        /** Show timeout message only for actions that don't have immediate effects */
                        if (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_OPEN_FILE && 
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_LOCK &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHUTDOWN &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_RESTART &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SLEEP) {
                            if (timeoutMsgW) {
                                ShowLocalizedNotification(hwnd, timeoutMsgW);
                            } else {
                                ShowLocalizedNotification(hwnd, L"时间到！");
                            }
                        }
                        
                        /** Reset Pomodoro state if timer doesn't match current sequence */
                        if (current_pomodoro_phase != POMODORO_PHASE_IDLE &&
                            (current_pomodoro_time_index >= POMODORO_TIMES_COUNT ||
                             CLOCK_TOTAL_TIME != POMODORO_TIMES[current_pomodoro_time_index])) {
                            current_pomodoro_phase = POMODORO_PHASE_IDLE;
                            current_pomodoro_time_index = 0;
                            complete_pomodoro_cycles = 0;
                        }
                        
                        if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SLEEP) {
                            CLOCK_TOTAL_TIME = 0;
                            countdown_elapsed_time = 0;
                            
                            KillTimer(hwnd, 1);
                            
                            InvalidateRect(hwnd, NULL, TRUE);
                            UpdateWindow(hwnd);
                            
                            if (timeoutMsgW) {
                                free(timeoutMsgW);
                            }
                            
                            system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
                            return TRUE;
                        }
                        
                        if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN) {
                            CLOCK_TOTAL_TIME = 0;
                            countdown_elapsed_time = 0;
                            
                            KillTimer(hwnd, 1);
                            
                            InvalidateRect(hwnd, NULL, TRUE);
                            UpdateWindow(hwnd);
                            
                            if (timeoutMsgW) {
                                free(timeoutMsgW);
                            }
                            
                            system("shutdown /s /t 0");
                            return TRUE;
                        }
                        
                        if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART) {
                            CLOCK_TOTAL_TIME = 0;
                            countdown_elapsed_time = 0;
                            
                            KillTimer(hwnd, 1);
                            
                            InvalidateRect(hwnd, NULL, TRUE);
                            UpdateWindow(hwnd);
                            
                            if (timeoutMsgW) {
                                free(timeoutMsgW);
                            }
                            
                            system("shutdown /r /t 0");
                            return TRUE;
                        }
                        
                        switch (CLOCK_TIMEOUT_ACTION) {
                            case TIMEOUT_ACTION_MESSAGE:
                                break;
                            case TIMEOUT_ACTION_LOCK:
                                LockWorkStation();
                                break;
                            case TIMEOUT_ACTION_OPEN_FILE: {
                                if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
                                    wchar_t wPath[MAX_PATH];
                                    MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_FILE_PATH, -1, wPath, MAX_PATH);
                                    
                                    HINSTANCE result = ShellExecuteW(NULL, L"open", wPath, NULL, NULL, SW_SHOWNORMAL);
                                    
                                    if ((INT_PTR)result <= 32) {
                                        MessageBoxW(hwnd, 
                                            GetLocalizedString(L"无法打开文件", L"Failed to open file"),
                                            GetLocalizedString(L"错误", L"Error"),
                                            MB_ICONERROR);
                                    }
                                }
                                break;
                            }
                            case TIMEOUT_ACTION_SHOW_TIME:
                                StopNotificationSound();
                                
                                CLOCK_SHOW_CURRENT_TIME = TRUE;
                                CLOCK_COUNT_UP = FALSE;
                                KillTimer(hwnd, 1);
                                SetTimer(hwnd, 1, 1000, NULL);
                                InvalidateRect(hwnd, NULL, TRUE);
                                break;
                            case TIMEOUT_ACTION_COUNT_UP:
                                StopNotificationSound();
                                
                                CLOCK_COUNT_UP = TRUE;
                                CLOCK_SHOW_CURRENT_TIME = FALSE;
                                countup_elapsed_time = 0;
                                elapsed_time = 0;
                                message_shown = FALSE;
                                countdown_message_shown = FALSE;
                                countup_message_shown = FALSE;
                                
                                CLOCK_IS_PAUSED = FALSE;
                                KillTimer(hwnd, 1);
                                SetTimer(hwnd, 1, 1000, NULL);
                                InvalidateRect(hwnd, NULL, TRUE);
                                break;
                            case TIMEOUT_ACTION_OPEN_WEBSITE:
                                if (wcslen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                                    ShellExecuteW(NULL, L"open", CLOCK_TIMEOUT_WEBSITE_URL, NULL, NULL, SW_NORMAL);
                                }
                                break;
                            case TIMEOUT_ACTION_RUN_COMMAND:
                                MessageBoxW(hwnd, 
                                    GetLocalizedString(L"运行命令功能正在开发中", L"Run Command feature is under development"),
                                    GetLocalizedString(L"提示", L"Notice"),
                                    MB_ICONINFORMATION);
                                break;
                            case TIMEOUT_ACTION_HTTP_REQUEST:
                                MessageBoxW(hwnd, 
                                    GetLocalizedString(L"HTTP请求功能正在开发中", L"HTTP Request feature is under development"),
                                    GetLocalizedString(L"提示", L"Notice"),
                                    MB_ICONINFORMATION);
                                break;
                        }
                    }

                    if (timeoutMsgW) {
                        free(timeoutMsgW);
                    }
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        return TRUE;
    } else if (wp == 999) {
        if (CLOCK_WINDOW_TOPMOST) {
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, 
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        KillTimer(hwnd, 999);
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Handle timer timeout events with appropriate actions and notifications
 * @param hwnd Main window handle
 * Executes configured timeout action and shows context-appropriate messages
 */
void OnTimerTimeout(HWND hwnd) {
    switch (CLOCK_TIMEOUT_ACTION) {
        case TIMEOUT_ACTION_MESSAGE: {
            wchar_t unicodeMsg[256] = {0};
            const char* utf8Message = NULL;
            
            /** Select appropriate message based on timer context */
            if (g_clockState == CLOCK_STATE_POMODORO) {
                if (g_pomodoroState.isLastCycle && g_pomodoroState.cycleIndex >= g_pomodoroState.totalCycles - 1) {
                    utf8Message = POMODORO_CYCLE_COMPLETE_TEXT;
                } else {
                    utf8Message = POMODORO_TIMEOUT_MESSAGE_TEXT;
                }
            } else {
                utf8Message = CLOCK_TIMEOUT_MESSAGE_TEXT;
            }
            
            if (utf8Message && utf8Message[0] != '\0') {
                int len = MultiByteToWideChar(CP_UTF8, 0, utf8Message, -1, unicodeMsg, 
                                            sizeof(unicodeMsg)/sizeof(wchar_t));
                if (len > 0) {
                    ShowNotification(hwnd, unicodeMsg);
                }
            }
            
            ReadNotificationSoundConfig();
            PlayNotificationSound(hwnd);
            
            break;
        }
        case TIMEOUT_ACTION_RUN_COMMAND: {
            /** Placeholder for future command execution feature */
            MessageBoxW(hwnd, 
                GetLocalizedString(L"运行命令功能正在开发中", L"Run Command feature is under development"),
                GetLocalizedString(L"提示", L"Notice"),
                MB_ICONINFORMATION);
            break;
        }
        case TIMEOUT_ACTION_HTTP_REQUEST: {
            /** Placeholder for future HTTP request feature */
            MessageBoxW(hwnd, 
                GetLocalizedString(L"HTTP请求功能正在开发中", L"HTTP Request feature is under development"),
                GetLocalizedString(L"提示", L"Notice"),
                MB_ICONINFORMATION);
            break;
        }

    }
}

/** @brief Global state variables for timer application (fallback definitions) */
#ifndef STUB_VARIABLES_DEFINED
#define STUB_VARIABLES_DEFINED
HWND g_hwnd = NULL;
ClockState g_clockState = CLOCK_STATE_IDLE;
PomodoroState g_pomodoroState = {FALSE, 0, 1};
#endif

/** @brief Weak function definitions for system control features */
#ifndef STUB_FUNCTIONS_DEFINED
#define STUB_FUNCTIONS_DEFINED
/**
 * @brief Put computer into sleep/suspend state
 * Uses Windows power management APIs via rundll32
 */
__attribute__((weak)) void SleepComputer(void) {
    system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
}

/**
 * @brief Shutdown computer immediately
 * Uses Windows shutdown command with immediate timeout
 */
__attribute__((weak)) void ShutdownComputer(void) {
    system("shutdown /s /t 0");
}

/**
 * @brief Restart computer immediately
 * Uses Windows shutdown command with restart flag
 */
__attribute__((weak)) void RestartComputer(void) {
    system("shutdown /r /t 0");
}

/**
 * @brief Stub function for time display mode switching
 * Implementation provided by other modules
 */
__attribute__((weak)) void SetTimeDisplay(void) {
}

/**
 * @brief Stub function for count-up timer display
 * @param hwnd Window handle (unused in stub)
 * Implementation provided by other modules
 */
__attribute__((weak)) void ShowCountUp(HWND hwnd) {
}
#endif