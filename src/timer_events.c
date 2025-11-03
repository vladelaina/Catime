/** @file timer_events.c @brief Timer event dispatch with callback-based retry */

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../include/timer_events.h"
#include "../include/timer.h"
#include "../include/language.h"
#include "../include/notification.h"
#include "../include/pomodoro.h"
#include "../include/config.h"
#include "../include/window.h"
#include "../include/drawing.h"
#include "../include/audio_player.h"
#include "../include/drag_scale.h"
#include "../include/utils/string_convert.h"

#define DEFAULT_POMODORO_DURATION 1500
#define MAX_RETRY_ATTEMPTS 3
#define RETRY_INTERVAL_MS 1500
#define FONT_CHECK_INTERVAL_MS 2000
#define TAIL_SEGMENT_THRESHOLD_SECONDS 2
#define TAIL_FAST_INTERVAL_MS 250
#define MAX_POMODORO_TIMES 10
#define MESSAGE_BUFFER_SIZE 256
extern char CLOCK_TIMEOUT_MESSAGE_TEXT[100];
extern char POMODORO_TIMEOUT_MESSAGE_TEXT[100];
extern char POMODORO_CYCLE_COMPLETE_TEXT[100];

int current_pomodoro_time_index = 0;
POMODORO_PHASE current_pomodoro_phase = POMODORO_PHASE_IDLE;
int complete_pomodoro_cycles = 0;
static DWORD last_timer_tick = 0;
static int ms_accumulator = 0;
static BOOL tail_fast_mode_active = FALSE;

static inline void ForceWindowRedraw(HWND hwnd) {
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

static wchar_t* SafeUtf8ToWide(const char* utf8String, wchar_t* buffer, size_t bufferSize) {
    if (!utf8String || !buffer || utf8String[0] == '\0') {
        return NULL;
    }
    
    return Utf8ToWide(utf8String, buffer, bufferSize) ? buffer : NULL;
}

static void ShowTimeoutNotification(HWND hwnd, const char* messageUtf8, BOOL playSound) {
    if (!messageUtf8 || messageUtf8[0] == '\0') {
        return;
    }

    wchar_t messageBuffer[MESSAGE_BUFFER_SIZE];
    wchar_t* messageW = SafeUtf8ToWide(messageUtf8, messageBuffer, MESSAGE_BUFFER_SIZE);
    
    if (messageW) {
        ShowNotification(hwnd, messageW);
    }
    
    if (playSound && CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE) {
        ReadNotificationSoundConfig();
        PlayNotificationSound(hwnd);
    }
}

static inline void ResetTimerState(int newTotalTime) {
    CLOCK_TOTAL_TIME = newTotalTime;
    countdown_elapsed_time = 0;
    countdown_message_shown = FALSE;
}

typedef struct {
    TimeoutActionType action;
    const char* command;
} SystemActionConfig;

static const SystemActionConfig SYSTEM_ACTIONS[] = {
    {TIMEOUT_ACTION_SLEEP,    "rundll32.exe powrprof.dll,SetSuspendState 0,1,0"},
    {TIMEOUT_ACTION_SHUTDOWN, "shutdown /s /t 0"},
    {TIMEOUT_ACTION_RESTART,  "shutdown /r /t 0"},
};

static BOOL ExecuteSystemAction(HWND hwnd, TimeoutActionType action) {
    for (size_t i = 0; i < sizeof(SYSTEM_ACTIONS) / sizeof(SYSTEM_ACTIONS[0]); i++) {
        if (SYSTEM_ACTIONS[i].action == action) {
            ResetTimerState(0);
            KillTimer(hwnd, TIMER_ID_MAIN);
            ForceWindowRedraw(hwnd);
            system(SYSTEM_ACTIONS[i].command);
            return TRUE;
        }
    }
    return FALSE;
}

typedef void (*RetrySetupCallback)(HWND);

/** Callback-based retry eliminates duplicate retry code */
static BOOL HandleRetryTimer(HWND hwnd, UINT timerId, int* retryCount, RetrySetupCallback callback) {
    if (*retryCount == 0) {
        *retryCount = MAX_RETRY_ATTEMPTS;
    }
    
    if (callback) {
        callback(hwnd);
    }
    
    (*retryCount)--;
    if (*retryCount > 0) {
        SetTimer(hwnd, timerId, RETRY_INTERVAL_MS, NULL);
    } else {
        KillTimer(hwnd, timerId);
    }
    
    return TRUE;
}

static void SetupTopmostWindow(HWND hwnd) {
        if (CLOCK_WINDOW_TOPMOST) {
            SetWindowTopmost(hwnd, TRUE);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
        if (!IsWindowVisible(hwnd)) {
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        }
}

static void SetupVisibilityWindow(HWND hwnd) {
        if (!CLOCK_WINDOW_TOPMOST) {
            HWND hProgman = FindWindowW(L"Progman", NULL);
            if (hProgman) {
                SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (LONG_PTR)hProgman);
            }
            
            if (!IsWindowVisible(hwnd)) {
                ShowWindow(hwnd, SW_SHOWNOACTIVATE);
            }
            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
}

static BOOL AdvancePomodoroState(void) {
    current_pomodoro_time_index++;
    
    if (current_pomodoro_time_index >= POMODORO_TIMES_COUNT) {
        current_pomodoro_time_index = 0;
        complete_pomodoro_cycles++;
        
        if (complete_pomodoro_cycles >= POMODORO_LOOP_COUNT) {
            return FALSE;
        }
    }
    
    return TRUE;
}

static void ResetPomodoroState(void) {
    current_pomodoro_phase = POMODORO_PHASE_IDLE;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;
}

static BOOL IsActivePomodoroTimer(void) {
    return current_pomodoro_phase != POMODORO_PHASE_IDLE &&
           current_pomodoro_time_index < POMODORO_TIMES_COUNT &&
           POMODORO_TIMES_COUNT > 0 &&
           CLOCK_TOTAL_TIME == POMODORO_TIMES[current_pomodoro_time_index];
}

static BOOL HandleFontValidation(HWND hwnd) {
    extern BOOL CheckAndFixFontPath(void);
    
    if (CheckAndFixFontPath()) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
    
    SetTimer(hwnd, TIMER_ID_FONT_VALIDATION, FONT_CHECK_INTERVAL_MS, NULL);
    return TRUE;
}

static BOOL HandleForceRedraw(HWND hwnd) {
    KillTimer(hwnd, TIMER_ID_FORCE_REDRAW);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(hwnd, NULL, TRUE);
        RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
        return TRUE;
    }
    
/** Faster updates in last 2 seconds for smoother UX */
static void AdjustTimerIntervalForTail(HWND hwnd) {
    if (CLOCK_SHOW_MILLISECONDS || CLOCK_COUNT_UP || CLOCK_SHOW_CURRENT_TIME || CLOCK_TOTAL_TIME == 0) {
        if (tail_fast_mode_active) {
            SetTimer(hwnd, TIMER_ID_MAIN, GetTimerInterval(), NULL);
            tail_fast_mode_active = FALSE;
        }
        return;
    }
    
    int remaining = CLOCK_TOTAL_TIME - countdown_elapsed_time;
    
    if (remaining <= TAIL_SEGMENT_THRESHOLD_SECONDS && remaining > 0) {
        if (!tail_fast_mode_active) {
            SetTimer(hwnd, TIMER_ID_MAIN, TAIL_FAST_INTERVAL_MS, NULL);
            tail_fast_mode_active = TRUE;
                }
            } else {
        if (tail_fast_mode_active) {
            SetTimer(hwnd, TIMER_ID_MAIN, GetTimerInterval(), NULL);
            tail_fast_mode_active = FALSE;
        }
    }
}

static void HandleTimeoutActions(HWND hwnd) {
                        switch (CLOCK_TIMEOUT_ACTION) {
                            case TIMEOUT_ACTION_MESSAGE:
                                break;
            
                            case TIMEOUT_ACTION_LOCK:
                                LockWorkStation();
                                break;
            
        case TIMEOUT_ACTION_OPEN_FILE:
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
            
                            case TIMEOUT_ACTION_SHOW_TIME:
                                StopNotificationSound();
                                CLOCK_SHOW_CURRENT_TIME = TRUE;
                                CLOCK_COUNT_UP = FALSE;
            KillTimer(hwnd, TIMER_ID_MAIN);
            SetTimer(hwnd, TIMER_ID_MAIN, GetTimerInterval(), NULL);
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
            KillTimer(hwnd, TIMER_ID_MAIN);
            SetTimer(hwnd, TIMER_ID_MAIN, GetTimerInterval(), NULL);
                                InvalidateRect(hwnd, NULL, TRUE);
                                break;
            
                            case TIMEOUT_ACTION_OPEN_WEBSITE:
                                if (wcslen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                                    ShellExecuteW(NULL, L"open", CLOCK_TIMEOUT_WEBSITE_URL, NULL, NULL, SW_NORMAL);
                                }
                                break;
                        }
}

static void HandlePomodoroCompletion(HWND hwnd) {
    ShowTimeoutNotification(hwnd, POMODORO_TIMEOUT_MESSAGE_TEXT, TRUE);
    
    if (!AdvancePomodoroState()) {
        ResetTimerState(0);
        ResetPomodoroState();
        
        ShowTimeoutNotification(hwnd, POMODORO_CYCLE_COMPLETE_TEXT, TRUE);
        
        CLOCK_COUNT_UP = FALSE;
        CLOCK_SHOW_CURRENT_TIME = FALSE;
        message_shown = TRUE;
        InvalidateRect(hwnd, NULL, TRUE);
        KillTimer(hwnd, TIMER_ID_MAIN);
        return;
    }
    
    ResetTimerState(POMODORO_TIMES[current_pomodoro_time_index]);
    
    if (current_pomodoro_time_index == 0 && complete_pomodoro_cycles > 0) {
        wchar_t cycleMsg[100];
        swprintf(cycleMsg, 100, 
                GetLocalizedString(L"开始第 %d 轮番茄钟", L"Starting Pomodoro cycle %d"),
                complete_pomodoro_cycles + 1);
        ShowNotification(hwnd, cycleMsg);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
}

static void HandleCountdownCompletion(HWND hwnd) {
    BOOL shouldNotify = (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_OPEN_FILE &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_LOCK &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHUTDOWN &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_RESTART &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SLEEP);
    
    if (shouldNotify) {
        ShowTimeoutNotification(hwnd, CLOCK_TIMEOUT_MESSAGE_TEXT, TRUE);
    }
    
    if (!IsActivePomodoroTimer()) {
        ResetPomodoroState();
    }
    
    if (ExecuteSystemAction(hwnd, CLOCK_TIMEOUT_ACTION)) {
        return;
    }
    
    HandleTimeoutActions(hwnd);
    
    if (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHOW_TIME &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_COUNT_UP) {
        ResetTimerState(0);
    }
}

static BOOL HandleMainTimer(HWND hwnd) {
    if (CLOCK_SHOW_CURRENT_TIME) {
        extern int last_displayed_second;
        last_displayed_second = -1;
                InvalidateRect(hwnd, NULL, TRUE);
        return TRUE;
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
    
    if (CLOCK_IS_PAUSED) {
        return TRUE;
    }
    
    DWORD current_tick = GetTickCount();
    if (last_timer_tick == 0) {
        last_timer_tick = current_tick;
        return TRUE;
    }
    
    DWORD elapsed_ms = current_tick - last_timer_tick;
    last_timer_tick = current_tick;
    ms_accumulator += elapsed_ms;
    
    AdjustTimerIntervalForTail(hwnd);
    
    if (ms_accumulator >= 1000) {
        int seconds_to_add = ms_accumulator / 1000;
        ms_accumulator %= 1000;
        
        if (!CLOCK_SHOW_MILLISECONDS && seconds_to_add > 1) {
            seconds_to_add = 1;
        }
        
        if (CLOCK_COUNT_UP) {
            countup_elapsed_time += seconds_to_add;
            } else {
            if (countdown_elapsed_time < CLOCK_TOTAL_TIME) {
                countdown_elapsed_time += seconds_to_add;
            }
            
            if (countdown_elapsed_time >= CLOCK_TOTAL_TIME && !countdown_message_shown) {
                countdown_message_shown = TRUE;
                
                extern void TrayAnimation_RecomputeTimerDelay(void);
                TrayAnimation_RecomputeTimerDelay();
                
                ReadNotificationMessagesConfig();
                ReadNotificationTypeConfig();
                
                if (IsActivePomodoroTimer()) {
                    HandlePomodoroCompletion(hwnd);
                } else {
                    HandleCountdownCompletion(hwnd);
                }
            }
        }
        
        InvalidateRect(hwnd, NULL, TRUE);
    }
    
    return TRUE;
}

void ResetMillisecondAccumulator(void) {
    last_timer_tick = GetTickCount();
    ms_accumulator = 0;
    ResetTimerMilliseconds();
}

void InitializePomodoro(void) {
    current_pomodoro_phase = POMODORO_PHASE_WORK;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;
    
    if (POMODORO_TIMES_COUNT > 0) {
        CLOCK_TOTAL_TIME = POMODORO_TIMES[0];
    } else {
        CLOCK_TOTAL_TIME = DEFAULT_POMODORO_DURATION;
    }
    
    countdown_elapsed_time = 0;
    countdown_message_shown = FALSE;
}

BOOL HandleTimerEvent(HWND hwnd, WPARAM wp) {
    static int topmost_retry = 0;
    static int visibility_retry = 0;
    
    switch (wp) {
        case TIMER_ID_TOPMOST_RETRY:
            return HandleRetryTimer(hwnd, TIMER_ID_TOPMOST_RETRY, &topmost_retry, SetupTopmostWindow);
            
        case TIMER_ID_VISIBILITY_RETRY:
            return HandleRetryTimer(hwnd, TIMER_ID_VISIBILITY_RETRY, &visibility_retry, SetupVisibilityWindow);
            
        case TIMER_ID_FORCE_REDRAW:
        case TIMER_ID_EDIT_MODE_REFRESH:
            return HandleForceRedraw(hwnd);
            
        case TIMER_ID_FONT_VALIDATION:
            return HandleFontValidation(hwnd);
            
        case TIMER_ID_MAIN:
            return HandleMainTimer(hwnd);
            
        default:
            return FALSE;
    }
}
