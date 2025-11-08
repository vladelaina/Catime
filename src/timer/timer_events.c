/**
 * @file timer_events.c
 * @brief Timer event dispatch with callback-based retry
 */

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "timer/timer_events.h"
#include "timer/timer.h"
#include "language.h"
#include "notification.h"
#include "pomodoro.h"
#include "config.h"
#include "window.h"
#include "drawing.h"
#include "audio_player.h"
#include "drag_scale.h"
#include "tray/tray_animation_core.h"
#include "utils/string_convert.h"

/* Pomodoro and timer constants:
 * - 1500s = 25 min (standard Pomodoro work duration)
 * - 1500ms retry: Balance between responsiveness and avoiding spam on persistent failures
 * - 2000ms font check: Periodic validation without excessive overhead
 * - 250ms tail mode: Fast updates for final 2 seconds of countdown (visual precision)
 */
#define DEFAULT_POMODORO_DURATION 1500
#define MAX_RETRY_ATTEMPTS 3
#define RETRY_INTERVAL_MS 1500
#define FONT_CHECK_INTERVAL_MS 2000
#define TAIL_SEGMENT_THRESHOLD_SECONDS 2
#define TAIL_FAST_INTERVAL_MS 250
#define MAX_POMODORO_TIMES 10
#define MESSAGE_BUFFER_SIZE 256

int current_pomodoro_time_index = 0;
POMODORO_PHASE current_pomodoro_phase = POMODORO_PHASE_IDLE;
int complete_pomodoro_cycles = 0;
static int pomodoro_initial_times_count = 0;
static int pomodoro_initial_loop_count = 0;
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
            
            int result = system(SYSTEM_ACTIONS[i].command);
            if (result != 0) {
                char errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg), 
                        "System action failed (code: %d). You may need administrator privileges.", 
                        result);
                wchar_t wErrorMsg[256];
                MultiByteToWideChar(CP_UTF8, 0, errorMsg, -1, wErrorMsg, 256);
                MessageBoxW(hwnd, wErrorMsg, GetLocalizedString(NULL, L"Error"), MB_ICONWARNING);
            }
            
            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
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
    if (g_AppConfig.pomodoro.times_count == 0) {
        return FALSE;
    }
    
    current_pomodoro_time_index++;
    
    if (current_pomodoro_time_index >= g_AppConfig.pomodoro.times_count) {
        current_pomodoro_time_index = 0;
        complete_pomodoro_cycles++;
        
        if (complete_pomodoro_cycles >= g_AppConfig.pomodoro.loop_count) {
            return FALSE;
        }
    }
    
    return TRUE;
}

static void ResetPomodoroState(void) {
    current_pomodoro_phase = POMODORO_PHASE_IDLE;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;
    pomodoro_initial_times_count = 0;
    pomodoro_initial_loop_count = 0;
}

static BOOL IsActivePomodoroTimer(void) {
    if (current_pomodoro_phase == POMODORO_PHASE_IDLE) {
        return FALSE;
    }
    
    if (g_AppConfig.pomodoro.times_count == 0) {
        return FALSE;
    }
    
    if (current_pomodoro_time_index >= g_AppConfig.pomodoro.times_count) {
        return FALSE;
    }
    
    if (CLOCK_TOTAL_TIME != g_AppConfig.pomodoro.times[current_pomodoro_time_index]) {
        return FALSE;
    }
    
    return TRUE;
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
    if (g_AppConfig.display.time_format.show_milliseconds || CLOCK_COUNT_UP || CLOCK_SHOW_CURRENT_TIME || CLOCK_TOTAL_TIME == 0) {
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
                                            GetLocalizedString(NULL, L"Failed to open file"),
                                            GetLocalizedString(NULL, L"Error"),
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
                                    HINSTANCE result = ShellExecuteW(NULL, L"open", CLOCK_TIMEOUT_WEBSITE_URL, NULL, NULL, SW_NORMAL);
                                    if ((INT_PTR)result <= 32) {
                                        MessageBoxW(hwnd, 
                                            GetLocalizedString(NULL, L"Failed to open website"),
                                            GetLocalizedString(NULL, L"Error"),
                                            MB_ICONERROR);
                                    }
                                }
                                break;
                        }
}

static void FormatPomodoroTime(int seconds, wchar_t* buffer, size_t bufferSize) {
    if (seconds >= 60) {
        int minutes = seconds / 60;
        int remaining_seconds = seconds % 60;
        if (remaining_seconds > 0) {
            _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%dm%ds", minutes, remaining_seconds);
        } else {
            _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%dm", minutes);
        }
    } else {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%ds", seconds);
    }
}

static BOOL HandlePomodoroCompletion(HWND hwnd) {
    wchar_t completionMsg[256];
    wchar_t timeStr[32];
    
    int completedIndex = current_pomodoro_time_index;
    
    int times_count = (pomodoro_initial_times_count > 0) 
        ? pomodoro_initial_times_count : g_AppConfig.pomodoro.times_count;
    int loop_count = (pomodoro_initial_loop_count > 0) 
        ? pomodoro_initial_loop_count : g_AppConfig.pomodoro.loop_count;
    
    if (times_count <= 0) times_count = 1;
    if (loop_count <= 0) loop_count = 1;
    
    int currentStep = complete_pomodoro_cycles * times_count + completedIndex + 1;
    int totalSteps = times_count * loop_count;
    
    if (completedIndex < g_AppConfig.pomodoro.times_count) {
        FormatPomodoroTime(g_AppConfig.pomodoro.times[completedIndex], timeStr, sizeof(timeStr)/sizeof(wchar_t));
    } else {
        wcscpy(timeStr, L"?");
    }
    
    if (!AdvancePomodoroState()) {
        const wchar_t* completed_text = GetLocalizedString(NULL, L"Pomodoro completed");
        if (totalSteps > 1) {
            _snwprintf_s(completionMsg, sizeof(completionMsg)/sizeof(wchar_t), _TRUNCATE,
                    L"%ls %ls (%d/%d)",
                    timeStr,
                    completed_text,
                    currentStep,
                    totalSteps);
        } else {
            _snwprintf_s(completionMsg, sizeof(completionMsg)/sizeof(wchar_t), _TRUNCATE,
                    L"%ls %ls",
                    timeStr,
                    completed_text);
        }
        ShowNotification(hwnd, completionMsg);
        
        ResetTimerState(0);
        ResetPomodoroState();
        
        const wchar_t* cycle_complete_text = GetLocalizedString(NULL, L"All Pomodoro cycles completed!");
        wchar_t finalMsg[MESSAGE_BUFFER_SIZE];
        _snwprintf_s(finalMsg, sizeof(finalMsg)/sizeof(wchar_t), _TRUNCATE,
                    L"%ls (%d/%d)",
                    cycle_complete_text,
                    totalSteps,
                    totalSteps);
        ShowNotification(hwnd, finalMsg);
        if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE) {
            ReadNotificationSoundConfig();
            PlayNotificationSound(hwnd);
        }
        
        CLOCK_COUNT_UP = FALSE;
        CLOCK_SHOW_CURRENT_TIME = FALSE;
        message_shown = TRUE;
        InvalidateRect(hwnd, NULL, TRUE);
        KillTimer(hwnd, TIMER_ID_MAIN);
        return FALSE;
    }
    
    const wchar_t* completed_text = GetLocalizedString(NULL, L"Pomodoro completed");
    if (totalSteps > 1) {
        _snwprintf_s(completionMsg, sizeof(completionMsg)/sizeof(wchar_t), _TRUNCATE,
                L"%ls %ls (%d/%d)",
                timeStr,
                completed_text,
                currentStep,
                totalSteps);
    } else {
        _snwprintf_s(completionMsg, sizeof(completionMsg)/sizeof(wchar_t), _TRUNCATE,
                L"%ls %ls",
                timeStr,
                completed_text);
    }
    ShowNotification(hwnd, completionMsg);
    
    if (current_pomodoro_time_index >= g_AppConfig.pomodoro.times_count) {
        ResetPomodoroState();
        return FALSE;
    }
    
    ResetTimerState(g_AppConfig.pomodoro.times[current_pomodoro_time_index]);
    countdown_message_shown = FALSE;
    
    extern BOOL InitializeHighPrecisionTimer(void);
    InitializeHighPrecisionTimer();
    ResetMillisecondAccumulator();
    
    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

static void HandleCountdownCompletion(HWND hwnd) {
    BOOL shouldNotify = (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_OPEN_FILE &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_LOCK &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHUTDOWN &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_RESTART &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SLEEP &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHOW_TIME &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_COUNT_UP &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_OPEN_WEBSITE);
    
    if (shouldNotify) {
        ShowTimeoutNotification(hwnd, g_AppConfig.notification.messages.timeout_message, TRUE);
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
        
        if (!g_AppConfig.display.time_format.show_milliseconds && seconds_to_add > 1) {
            seconds_to_add = 1;
        }
        
        if (CLOCK_COUNT_UP) {
            countup_elapsed_time += seconds_to_add;
            } else {
            if (countdown_elapsed_time < CLOCK_TOTAL_TIME) {
                countdown_elapsed_time += seconds_to_add;
            }
            
            if (countdown_elapsed_time >= CLOCK_TOTAL_TIME) {
                if (!countdown_message_shown) {
                    countdown_message_shown = TRUE;
                    
                    TrayAnimation_RecomputeTimerDelay();
                    
                    ReadNotificationMessagesConfig();
                    ReadNotificationTypeConfig();
                    
                    BOOL pomodoro_advanced = FALSE;
                    if (IsActivePomodoroTimer()) {
                        pomodoro_advanced = HandlePomodoroCompletion(hwnd);
                    } else {
                        HandleCountdownCompletion(hwnd);
                    }
                    
                    if (pomodoro_advanced) {
                        return TRUE;
                    }
                }
                countdown_elapsed_time = CLOCK_TOTAL_TIME;
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
    
    pomodoro_initial_times_count = g_AppConfig.pomodoro.times_count;
    pomodoro_initial_loop_count = (g_AppConfig.pomodoro.loop_count > 0) 
        ? g_AppConfig.pomodoro.loop_count : 1;
    
    if (g_AppConfig.pomodoro.times_count > 0) {
        CLOCK_TOTAL_TIME = g_AppConfig.pomodoro.times[0];
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
