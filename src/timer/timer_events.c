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
#include "log.h"
#include "window/window_desktop_integration.h"

/* Pomodoro and timer constants */
#define DEFAULT_POMODORO_DURATION 1500
#define MAX_RETRY_ATTEMPTS 3
#define RETRY_INTERVAL_MS 1500
#define FONT_CHECK_INTERVAL_MS 2000
#define MAX_POMODORO_TIMES 10
#define MESSAGE_BUFFER_SIZE 256

int current_pomodoro_time_index = 0;
POMODORO_PHASE current_pomodoro_phase = POMODORO_PHASE_IDLE;
int complete_pomodoro_cycles = 0;
static int pomodoro_initial_times_count = 0;
static int pomodoro_initial_loop_count = 0;
static int pomodoro_initial_times[MAX_POMODORO_TIMES] = {0};
static DWORD last_timer_tick = 0;
static int ms_accumulator = 0;
static BOOL topmost_fast_mode_active = FALSE;

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
                LOG_WARNING("System action '%s' failed (code: %d). May need administrator privileges.", 
                           SYSTEM_ACTIONS[i].command, result);
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
    if (pomodoro_initial_times_count == 0) {
        return FALSE;
    }
    
    current_pomodoro_time_index++;
    
    if (current_pomodoro_time_index >= pomodoro_initial_times_count) {
        current_pomodoro_time_index = 0;
        complete_pomodoro_cycles++;
        
        if (complete_pomodoro_cycles >= pomodoro_initial_loop_count) {
            return FALSE;
        }
    }
    
    return TRUE;
}

void ResetPomodoroState(void) {
    current_pomodoro_phase = POMODORO_PHASE_IDLE;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;
    pomodoro_initial_times_count = 0;
    pomodoro_initial_loop_count = 0;
    memset(pomodoro_initial_times, 0, sizeof(pomodoro_initial_times));
}

static BOOL IsActivePomodoroTimer(void) {
    if (current_pomodoro_phase == POMODORO_PHASE_IDLE) {
        return FALSE;
    }
    
    if (pomodoro_initial_times_count == 0) {
        return FALSE;
    }
    
    if (current_pomodoro_time_index >= pomodoro_initial_times_count) {
        return FALSE;
    }
    
    if (CLOCK_TOTAL_TIME != pomodoro_initial_times[current_pomodoro_time_index]) {
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
                    LOG_WARNING("Failed to open timeout file: %s (error: %d)", 
                               CLOCK_TIMEOUT_FILE_PATH, (int)(INT_PTR)result);
                }
            }
            break;

        case TIMEOUT_ACTION_SHOW_TIME:
            StopNotificationSound();
            CLOCK_SHOW_CURRENT_TIME = TRUE;
            CLOCK_COUNT_UP = FALSE;
            ResetMillisecondAccumulator();
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
            CLOCK_IS_PAUSED = FALSE;
            ResetMillisecondAccumulator();
            KillTimer(hwnd, TIMER_ID_MAIN);
            SetTimer(hwnd, TIMER_ID_MAIN, GetTimerInterval(), NULL);
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case TIMEOUT_ACTION_OPEN_WEBSITE:
            if (wcslen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                HINSTANCE result = ShellExecuteW(NULL, L"open", CLOCK_TIMEOUT_WEBSITE_URL, NULL, NULL, SW_NORMAL);
                if ((INT_PTR)result <= 32) {
                    char urlUtf8[MAX_PATH];
                    WideCharToMultiByte(CP_UTF8, 0, CLOCK_TIMEOUT_WEBSITE_URL, -1, urlUtf8, MAX_PATH, NULL, NULL);
                    LOG_WARNING("Failed to open timeout website: %s (error: %d)", 
                               urlUtf8, (int)(INT_PTR)result);
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
    int times_count = pomodoro_initial_times_count;
    int loop_count = pomodoro_initial_loop_count;

    if (times_count <= 0) times_count = 1;
    if (loop_count <= 0) loop_count = 1;

    /* Current step within this cycle (1-based) */
    int stepInCycle = completedIndex + 1;
    /* Current cycle number (1-based) */
    int currentCycle = complete_pomodoro_cycles + 1;

    if (completedIndex < pomodoro_initial_times_count) {
        FormatPomodoroTime(pomodoro_initial_times[completedIndex], timeStr, sizeof(timeStr)/sizeof(wchar_t));
    } else {
        wcscpy_s(timeStr, 32, L"?");
    }

    if (!AdvancePomodoroState()) {
        const wchar_t* completed_text = GetLocalizedString(NULL, L"Pomodoro completed");
        const wchar_t* cycle_text = GetLocalizedString(NULL, L"Cycle");
        const wchar_t* round_text = GetLocalizedString(NULL, L"Round");
        if (times_count > 1 || loop_count > 1) {
            _snwprintf_s(completionMsg, sizeof(completionMsg)/sizeof(wchar_t), _TRUNCATE,
                    L"%ls %ls (%ls%d/%d%ls %d/%d)",
                    timeStr,
                    completed_text,
                    cycle_text,
                    currentCycle,
                    loop_count,
                    round_text,
                    stepInCycle,
                    times_count);
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
        ShowNotification(hwnd, cycle_complete_text);
        PlayNotificationSound(hwnd);

        CLOCK_COUNT_UP = FALSE;
        CLOCK_SHOW_CURRENT_TIME = FALSE;
        message_shown = TRUE;
        InvalidateRect(hwnd, NULL, TRUE);
        KillTimer(hwnd, TIMER_ID_MAIN);
        return FALSE;
    }

    const wchar_t* completed_text = GetLocalizedString(NULL, L"Pomodoro completed");
    const wchar_t* cycle_text = GetLocalizedString(NULL, L"Cycle");
    const wchar_t* round_text = GetLocalizedString(NULL, L"Round");
    if (times_count > 1 || loop_count > 1) {
        _snwprintf_s(completionMsg, sizeof(completionMsg)/sizeof(wchar_t), _TRUNCATE,
                L"%ls %ls (%ls%d/%d%ls %d/%d)",
                timeStr,
                completed_text,
                cycle_text,
                currentCycle,
                loop_count,
                round_text,
                stepInCycle,
                times_count);
    } else {
        _snwprintf_s(completionMsg, sizeof(completionMsg)/sizeof(wchar_t), _TRUNCATE,
                L"%ls %ls",
                timeStr,
                completed_text);
    }
    ShowNotification(hwnd, completionMsg);
    PlayNotificationSound(hwnd);

    // Seamless transition: Add new duration to the existing target end time
    // This ensures no time is lost during notification processing
    int next_duration_sec = pomodoro_initial_times[current_pomodoro_time_index];
    ResetTimerState(next_duration_sec);
    
    g_target_end_time += ((int64_t)next_duration_sec * 1000);
    
    countdown_message_shown = FALSE;
    
    extern BOOL InitializeHighPrecisionTimer(void);
    InitializeHighPrecisionTimer();
    ResetMillisecondAccumulator();
    
    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

static void HandleCountdownCompletion(HWND hwnd) {
    LOG_INFO("========== Countdown timer timeout triggered ==========");
    
    const char* actionNames[] = {
        "MESSAGE", "LOCK", "SHUTDOWN", "RESTART", "SLEEP",
        "SHOW_TIME", "COUNT_UP", "OPEN_FILE", "OPEN_WEBSITE"
    };
    const char* actionName = (CLOCK_TIMEOUT_ACTION >= 0 && CLOCK_TIMEOUT_ACTION < 9) 
        ? actionNames[CLOCK_TIMEOUT_ACTION] : "UNKNOWN";
    LOG_INFO("Timeout action configured: %s", actionName);
    
    BOOL shouldNotify = (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_OPEN_FILE &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_LOCK &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHUTDOWN &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_RESTART &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SLEEP &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHOW_TIME &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_COUNT_UP &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_OPEN_WEBSITE);
    
    if (shouldNotify) {
        LOG_INFO("Displaying timeout notification and playing sound");
        ShowTimeoutNotification(hwnd, g_AppConfig.notification.messages.timeout_message, TRUE);
    }
    
    if (!IsActivePomodoroTimer()) {
        ResetPomodoroState();
    }
    
    if (ExecuteSystemAction(hwnd, CLOCK_TIMEOUT_ACTION)) {
        LOG_INFO("System action executed successfully");
        return;
    }
    
    HandleTimeoutActions(hwnd);
    
    if (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHOW_TIME &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_COUNT_UP) {
        ResetTimerState(0);
        ResetMillisecondAccumulator();
    }
    
    LOG_INFO("========== Countdown timeout handling completed ==========\n");
}

static BOOL HandleMainTimer(HWND hwnd) {
    /* Manage fast topmost enforcement based on taskbar overlap */
    BOOL overlapsTaskbar = EnforceTopmostOverTaskbar(hwnd);
    
    if (overlapsTaskbar && !topmost_fast_mode_active) {
        SetTimer(hwnd, TIMER_ID_TOPMOST_ENFORCE, 50, NULL);
        topmost_fast_mode_active = TRUE;
    } else if (!overlapsTaskbar && topmost_fast_mode_active) {
        KillTimer(hwnd, TIMER_ID_TOPMOST_ENFORCE);
        topmost_fast_mode_active = FALSE;
    }
    
    static DWORD s_lastRenderTime = 0;
    DWORD now_tick = GetTickCount();
    BOOL shouldRender = TRUE;
    
    if (CLOCK_HOLOGRAPHIC_EFFECT) {
        RECT rect;
        GetClientRect(hwnd, &rect);
        int pixels = rect.right * rect.bottom;
        DWORD minInterval = (pixels < 30000) ? 0 : (pixels < 100000) ? 50 : (pixels < 300000) ? 100 : 150;
        if (minInterval > 0 && (now_tick - s_lastRenderTime) < minInterval) shouldRender = FALSE;
    }
    
    if (CLOCK_SHOW_CURRENT_TIME) {
        extern int last_displayed_second;
        last_displayed_second = -1;
        if (shouldRender) {
            s_lastRenderTime = now_tick;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return TRUE;
    }
    
    if (shouldRender) {
        s_lastRenderTime = now_tick;
        InvalidateRect(hwnd, NULL, TRUE);
    }
    
    if (CLOCK_IS_PAUSED) {
        return TRUE;
    }
    
    /* ABSOLUTE TIME CALCULATION (MILLISECONDS) */
    extern int64_t GetAbsoluteTimeMs(void);
    int64_t current_time_ms = GetAbsoluteTimeMs();
    int current_elapsed_sec = 0;

    if (CLOCK_COUNT_UP) {
        int64_t elapsed_ms = current_time_ms - g_start_time;
        if (elapsed_ms < 0) elapsed_ms = 0;
        current_elapsed_sec = (int)(elapsed_ms / 1000);
        countup_elapsed_time = current_elapsed_sec;
    } else {
        int64_t remaining_ms = g_target_end_time - current_time_ms;
        if (remaining_ms < 0) remaining_ms = 0;
        
        /* Use ceiling division to prevent "00:00" display while time remains (e.g. 0.9s -> 1s) */
        int remaining_sec_rounded = (int)((remaining_ms + 999) / 1000);
        
        current_elapsed_sec = CLOCK_TOTAL_TIME - remaining_sec_rounded;
        if (current_elapsed_sec > CLOCK_TOTAL_TIME) current_elapsed_sec = CLOCK_TOTAL_TIME;
        if (current_elapsed_sec < 0) current_elapsed_sec = 0;
        
        countdown_elapsed_time = current_elapsed_sec;
    }
    
    if (!CLOCK_COUNT_UP && countdown_elapsed_time >= CLOCK_TOTAL_TIME) {
        if (!countdown_message_shown) {
            countdown_message_shown = TRUE;
            
            TrayAnimation_RecomputeTimerDelay();
            
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
    
    InvalidateRect(hwnd, NULL, TRUE);
    
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
    
    // Copy the entire times array to protect against config changes during run
    memset(pomodoro_initial_times, 0, sizeof(pomodoro_initial_times));
    int copy_count = (pomodoro_initial_times_count < MAX_POMODORO_TIMES) 
        ? pomodoro_initial_times_count : MAX_POMODORO_TIMES;
    for (int i = 0; i < copy_count; i++) {
        pomodoro_initial_times[i] = g_AppConfig.pomodoro.times[i];
    }
    
    if (pomodoro_initial_times_count > 0) {
        CLOCK_TOTAL_TIME = pomodoro_initial_times[0];
    } else {
        CLOCK_TOTAL_TIME = DEFAULT_POMODORO_DURATION;
    }
    
    countdown_elapsed_time = 0;
    countdown_message_shown = FALSE;
    ResetMillisecondAccumulator();
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

        case TIMER_ID_RENDER_ANIMATION:
            /* Pure render tick - decouples visual flow from logic update */
            InvalidateRect(hwnd, NULL, TRUE);
            return TRUE;

        case TIMER_ID_TOPMOST_ENFORCE:
            /* Fast topmost enforcement when overlapping taskbar (50ms) */
            EnforceTopmostOverTaskbar(hwnd);
            return TRUE;

        default:
            return FALSE;
    }
}
