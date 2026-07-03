/**
 * @file timer_events.c
 * @brief Timer event dispatch with callback-based retry
 */

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <powrprof.h>

#include "timer/timer_events.h"
#include "timer/timer.h"
#include "timer/main_timer.h"
#include "language.h"
#include "notification.h"
#include "pomodoro.h"
#include "config.h"
#include "config/config_defaults.h"
#include "window.h"
#include "drawing.h"
#include "menu_preview.h"
#include "audio_player.h"
#include "drag_scale.h"
#include "font/font_manager.h"
#include "tray/tray_animation_core.h"
#include "utils/string_convert.h"
#include "utils/url_safety.h"
#include "log.h"
#include "window/window_desktop_integration.h"

/* External function from timer.c */
extern int64_t GetAbsoluteTimeMs(void);

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
static wchar_t g_visibleTimerCurrentText[TIME_TEXT_MAX_LEN] = {0};
static TimeoutActionType g_armedTimeoutSystemAction = TIMEOUT_ACTION_MESSAGE;

static inline void ForceWindowRedraw(HWND hwnd) {
    InvalidateRect(hwnd, NULL, TRUE);
}

static inline void RequestWindowRepaint(HWND hwnd) {
    if (CLOCK_IS_DRAGGING) {
        return;
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static BOOL StartMainTimerForTimeoutAction(HWND hwnd, const char* actionName) {
    UINT interval = GetTimerInterval();
    if (MainTimer_Start(hwnd, interval)) {
        return TRUE;
    }

    LOG_WARNING("Failed to start main timer for timeout action %s (interval=%u)",
                actionName ? actionName : "unknown", interval);
    return FALSE;
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
    const wchar_t* messageW = SafeUtf8ToWide(messageUtf8, messageBuffer, MESSAGE_BUFFER_SIZE);
    
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

static BOOL IsSystemTimeoutAction(TimeoutActionType action) {
    return action == TIMEOUT_ACTION_SHUTDOWN ||
           action == TIMEOUT_ACTION_RESTART ||
           action == TIMEOUT_ACTION_SLEEP;
}

void Timer_ClearTimeoutSystemActionArm(void) {
    g_armedTimeoutSystemAction = TIMEOUT_ACTION_MESSAGE;
}

void Timer_ArmTimeoutSystemAction(TimeoutActionType action) {
    if (IsSystemTimeoutAction(action)) {
        g_armedTimeoutSystemAction = action;
    } else {
        Timer_ClearTimeoutSystemActionArm();
    }
}

static BOOL IsSystemTimeoutActionArmed(TimeoutActionType action) {
    return IsSystemTimeoutAction(action) &&
           g_armedTimeoutSystemAction == action;
}

static BOOL IsSystemTimeoutExecutionContextSafe(void) {
    if (!MainTimer_IsRunning() ||
        CLOCK_SHOW_CURRENT_TIME ||
        CLOCK_COUNT_UP ||
        CLOCK_IS_PAUSED ||
        CLOCK_TOTAL_TIME <= 0 ||
        countdown_elapsed_time < CLOCK_TOTAL_TIME ||
        g_target_end_time <= 0) {
        return FALSE;
    }

    return GetAbsoluteTimeMs() >= g_target_end_time;
}

static void ConsumeBlockedSystemTimeoutAction(HWND hwnd) {
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    Timer_ClearTimeoutSystemActionArm();
    ResetTimerState(0);
    MainTimer_Stop();
    ForceWindowRedraw(hwnd);
}

static const char* GetSystemActionName(TimeoutActionType action) {
    switch (action) {
        case TIMEOUT_ACTION_SHUTDOWN: return "shutdown";
        case TIMEOUT_ACTION_RESTART: return "restart";
        case TIMEOUT_ACTION_SLEEP: return "sleep";
        default: return "unknown";
    }
}

static BOOL EnableShutdownPrivilege(void) {
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &token)) {
        LOG_WARNING("Failed to open process token for system action (error: %lu)",
                    GetLastError());
        return FALSE;
    }

    TOKEN_PRIVILEGES privileges;
    ZeroMemory(&privileges, sizeof(privileges));
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(NULL,
                               SE_SHUTDOWN_NAME,
                               &privileges.Privileges[0].Luid)) {
        DWORD error = GetLastError();
        CloseHandle(token);
        LOG_WARNING("Failed to look up shutdown privilege (error: %lu)", error);
        return FALSE;
    }

    SetLastError(ERROR_SUCCESS);
    if (!AdjustTokenPrivileges(token, FALSE, &privileges, 0, NULL, NULL)) {
        DWORD error = GetLastError();
        CloseHandle(token);
        LOG_WARNING("Failed to enable shutdown privilege (error: %lu)", error);
        return FALSE;
    }

    DWORD adjustError = GetLastError();
    CloseHandle(token);

    if (adjustError == ERROR_NOT_ALL_ASSIGNED) {
        LOG_WARNING("Shutdown privilege is not assigned to this process token");
        return FALSE;
    }

    return TRUE;
}

static BOOL ExecuteSystemPowerAction(TimeoutActionType action) {
    switch (action) {
        case TIMEOUT_ACTION_SHUTDOWN:
            if (!EnableShutdownPrivilege()) {
                return FALSE;
            }
            return ExitWindowsEx(EWX_POWEROFF | EWX_FORCEIFHUNG,
                                 SHTDN_REASON_MAJOR_APPLICATION |
                                 SHTDN_REASON_MINOR_MAINTENANCE |
                                 SHTDN_REASON_FLAG_PLANNED);

        case TIMEOUT_ACTION_RESTART:
            if (!EnableShutdownPrivilege()) {
                return FALSE;
            }
            return ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG,
                                 SHTDN_REASON_MAJOR_APPLICATION |
                                 SHTDN_REASON_MINOR_MAINTENANCE |
                                 SHTDN_REASON_FLAG_PLANNED);

        case TIMEOUT_ACTION_SLEEP:
            EnableShutdownPrivilege();
            return SetSuspendState(FALSE, FALSE, FALSE);

        default:
            return FALSE;
    }
}

static BOOL ExecuteSystemAction(HWND hwnd, TimeoutActionType action) {
    if (!IsSystemTimeoutAction(action)) {
        return FALSE;
    }

    if (!IsSystemTimeoutActionArmed(action)) {
        LOG_WARNING("Blocked unarmed timeout system action: %s",
                    GetSystemActionName(action));
        ConsumeBlockedSystemTimeoutAction(hwnd);
        return TRUE;
    }

    if (!IsSystemTimeoutExecutionContextSafe()) {
        LOG_WARNING("Blocked timeout system action outside completed countdown: %s",
                    GetSystemActionName(action));
        ConsumeBlockedSystemTimeoutAction(hwnd);
        return TRUE;
    }

    ResetTimerState(0);
    MainTimer_Stop();
    ForceWindowRedraw(hwnd);
    Timer_ClearTimeoutSystemActionArm();

    if (!ExecuteSystemPowerAction(action)) {
        LOG_WARNING("Timeout system action failed: %s (error: %lu)",
                    GetSystemActionName(action),
                    GetLastError());
    }

    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    return TRUE;
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
        if (!SetTimer(hwnd, timerId, RETRY_INTERVAL_MS, NULL)) {
            LOG_WARNING("Retry timer %u failed to reschedule (error: %lu)",
                        timerId, GetLastError());
        }
    } else {
        KillTimer(hwnd, timerId);
    }
    
    return TRUE;
}

static void SetupTopmostWindow(HWND hwnd) {
    if (CLOCK_WINDOW_TOPMOST) {
        EnsureWindowVisibleWithTopmostState(hwnd);
    }
}

static void SetupVisibilityWindow(HWND hwnd) {
    if (!CLOCK_WINDOW_TOPMOST) {
        EnsureWindowVisibleWithTopmostState(hwnd);
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
    if (CheckAndReloadCurrentFontPath()) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
    
    if (!SetTimer(hwnd, TIMER_ID_FONT_VALIDATION, FONT_CHECK_INTERVAL_MS, NULL)) {
        LOG_WARNING("Font validation timer failed to reschedule (error: %lu)",
                    GetLastError());
    }
    return TRUE;
}

static BOOL HandleForceRedraw(HWND hwnd) {
    KillTimer(hwnd, TIMER_ID_FORCE_REDRAW);
    EnsureWindowVisibleWithTopmostState(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
    RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
    return TRUE;
}

static void HandleTimeoutActions(HWND hwnd) {
    switch (CLOCK_TIMEOUT_ACTION) {
        case TIMEOUT_ACTION_MESSAGE:
            break;

        case TIMEOUT_ACTION_LOCK:
            if (!LockWorkStation()) {
                LOG_WARNING("Failed to lock workstation (error: %lu)", GetLastError());
            }
            break;

        case TIMEOUT_ACTION_OPEN_FILE:
            if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
                wchar_t wPath[MAX_PATH];
                if (MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_FILE_PATH, -1, wPath, MAX_PATH) <= 0) {
                    LOG_WARNING("Failed to convert timeout file path: %s", CLOCK_TIMEOUT_FILE_PATH);
                    break;
                }
                HINSTANCE result = ShellExecuteW(NULL, L"open", wPath, NULL, NULL, SW_SHOWNORMAL);
                if ((INT_PTR)result <= 32) {
                    LOG_WARNING("Failed to open timeout file: %s (error: %d)", 
                               CLOCK_TIMEOUT_FILE_PATH, (int)(INT_PTR)result);
                }
            }
            break;

        case TIMEOUT_ACTION_SHOW_TIME:
            StopNotificationSound();
            CLOCK_SHOW_CURRENT_TIME = true;
            CLOCK_COUNT_UP = false;
            /* Reset countdown state to prevent accidental completion trigger */
            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            ResetMillisecondAccumulator();
            MainTimer_Stop();
            StartMainTimerForTimeoutAction(hwnd, "show_time");
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case TIMEOUT_ACTION_COUNT_UP:
            StopNotificationSound();
            CLOCK_COUNT_UP = true;
            CLOCK_SHOW_CURRENT_TIME = false;
            countup_elapsed_time = 0;
            elapsed_time = 0;
            g_start_time = GetAbsoluteTimeMs();
            message_shown = FALSE;
            countdown_message_shown = false;
            CLOCK_IS_PAUSED = false;
            ResetMillisecondAccumulator();
            MainTimer_Stop();
            if (!StartMainTimerForTimeoutAction(hwnd, "count_up")) {
                CLOCK_IS_PAUSED = true;
            }
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case TIMEOUT_ACTION_OPEN_WEBSITE:
            if (strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                wchar_t wUrl[MAX_PATH];
                if (MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_WEBSITE_URL, -1, wUrl, MAX_PATH) <= 0) {
                    LOG_WARNING("Failed to convert timeout website URL: %s", CLOCK_TIMEOUT_WEBSITE_URL);
                    break;
                }
                if (!IsSafeOpenUrlW(wUrl)) {
                    LOG_WARNING("Blocked unsafe timeout website URL: %s", CLOCK_TIMEOUT_WEBSITE_URL);
                    break;
                }
                HINSTANCE result = ShellExecuteW(NULL, L"open", wUrl, NULL, NULL, SW_NORMAL);
                if ((INT_PTR)result <= 32) {
                    LOG_WARNING("Failed to open timeout website: %s (error: %d)", 
                               CLOCK_TIMEOUT_WEBSITE_URL, (int)(INT_PTR)result);
                }
            }
            break;

        case TIMEOUT_ACTION_SHUTDOWN:
        case TIMEOUT_ACTION_RESTART:
        case TIMEOUT_ACTION_SLEEP:
            /* These system actions are handled earlier by ExecuteSystemAction(). */
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

static BOOL HasVisibleTimerTextChanged(wchar_t* lastText, size_t lastTextSize, BOOL* hasLastText) {
    if (!lastText || lastTextSize == 0 || !hasLastText) return TRUE;

    wchar_t* currentText = g_visibleTimerCurrentText;
    currentText[0] = L'\0';
    GetTimeText(currentText, TIME_TEXT_MAX_LEN);

    if (*hasLastText && wcscmp(lastText, currentText) == 0) {
        return FALSE;
    }

    wcsncpy(lastText, currentText, lastTextSize - 1);
    lastText[lastTextSize - 1] = L'\0';
    *hasLastText = TRUE;
    return TRUE;
}

static BOOL ShouldRenderMainTimer(wchar_t* lastText, size_t lastTextSize, BOOL* hasLastText) {
    return HasVisibleTimerTextChanged(lastText, lastTextSize, hasLastText);
}

static BOOL ShouldCheckActiveTimerRender(int currentElapsedSecond,
                                         int* lastCheckedSecond,
                                         BOOL* hasLastCheckedSecond) {
    if (!lastCheckedSecond || !hasLastCheckedSecond) return TRUE;

    if (GetActiveShowMilliseconds()) {
        *hasLastCheckedSecond = FALSE;
        return TRUE;
    }

    if (*hasLastCheckedSecond && *lastCheckedSecond == currentElapsedSecond) {
        return FALSE;
    }

    *lastCheckedSecond = currentElapsedSecond;
    *hasLastCheckedSecond = TRUE;
    return TRUE;
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

        CLOCK_COUNT_UP = false;
        CLOCK_SHOW_CURRENT_TIME = false;
        message_shown = TRUE;
        InvalidateRect(hwnd, NULL, TRUE);
        MainTimer_Stop();
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

    countdown_message_shown = false;
    
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
    
    if (ExecuteSystemAction(hwnd, CLOCK_TIMEOUT_ACTION)) return;
    
    HandleTimeoutActions(hwnd);
    
    if (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHOW_TIME &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_COUNT_UP) {
        ResetTimerState(0);
        ResetMillisecondAccumulator();
    }
}

static BOOL HandleMainTimer(HWND hwnd) {
    static DWORD s_lastTopmostCheck = 0;
    static UINT s_lastDesiredInterval = 0;
    static wchar_t s_lastRenderedText[TIME_TEXT_MAX_LEN] = {0};
    static BOOL s_hasLastRenderedText = FALSE;
    static int s_lastActiveRenderCheckSecond = 0;
    static BOOL s_hasLastActiveRenderCheckSecond = FALSE;
    DWORD now_tick = GetTickCount();
    UINT desiredInterval = GetTimerInterval();

    if (s_lastDesiredInterval != desiredInterval) {
        s_lastDesiredInterval = desiredInterval;
        MainTimer_SetInterval(desiredInterval);
    }

    /* Throttle expensive topmost/taskbar overlap checks. */
    if (!CLOCK_IS_DRAGGING &&
        (s_lastTopmostCheck == 0 || (now_tick - s_lastTopmostCheck) >= 500)) {
        EnforceTopmostOverTaskbar(hwnd);
        s_lastTopmostCheck = now_tick;
    }
    
    if (CLOCK_SHOW_CURRENT_TIME) {
        last_displayed_second = -1;
        s_hasLastActiveRenderCheckSecond = FALSE;
        if (ShouldRenderMainTimer(s_lastRenderedText,
                                  sizeof(s_lastRenderedText) / sizeof(s_lastRenderedText[0]),
                                  &s_hasLastRenderedText)) {
            RequestWindowRepaint(hwnd);
        }
        return TRUE;
    }

    if (CLOCK_IS_PAUSED) {
        s_hasLastActiveRenderCheckSecond = FALSE;
        if (ShouldRenderMainTimer(s_lastRenderedText,
                                  sizeof(s_lastRenderedText) / sizeof(s_lastRenderedText[0]),
                                  &s_hasLastRenderedText)) {
            RequestWindowRepaint(hwnd);
        }
        return TRUE;
    }
    
    /* ABSOLUTE TIME CALCULATION (MILLISECONDS) */
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
    
    if (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time >= CLOCK_TOTAL_TIME) {
        if (!countdown_message_shown) {
            countdown_message_shown = true;
            
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

    if (ShouldCheckActiveTimerRender(current_elapsed_sec,
                                     &s_lastActiveRenderCheckSecond,
                                     &s_hasLastActiveRenderCheckSecond)) {
        if (ShouldRenderMainTimer(s_lastRenderedText,
                                  sizeof(s_lastRenderedText) / sizeof(s_lastRenderedText[0]),
                                  &s_hasLastRenderedText)) {
            RequestWindowRepaint(hwnd);
        }
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
    if (pomodoro_initial_times_count < 0) {
        pomodoro_initial_times_count = 0;
    }
    if (pomodoro_initial_times_count > MAX_POMODORO_TIMES) {
        pomodoro_initial_times_count = MAX_POMODORO_TIMES;
    }
    pomodoro_initial_loop_count = g_AppConfig.pomodoro.loop_count;
    if (pomodoro_initial_loop_count < MIN_POMODORO_LOOP_COUNT) {
        pomodoro_initial_loop_count = MIN_POMODORO_LOOP_COUNT;
    }
    if (pomodoro_initial_loop_count > MAX_POMODORO_LOOP_COUNT) {
        pomodoro_initial_loop_count = MAX_POMODORO_LOOP_COUNT;
    }
    
    // Copy the entire times array to protect against config changes during run
    memset(pomodoro_initial_times, 0, sizeof(pomodoro_initial_times));
    for (int i = 0; i < pomodoro_initial_times_count; i++) {
        pomodoro_initial_times[i] = g_AppConfig.pomodoro.times[i];
    }
    
    if (pomodoro_initial_times_count > 0) {
        CLOCK_TOTAL_TIME = pomodoro_initial_times[0];
    } else {
        CLOCK_TOTAL_TIME = DEFAULT_POMODORO_DURATION;
    }
    
    countdown_elapsed_time = 0;
    countdown_message_shown = false;
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

        case TIMER_ID_TOPMOST_APPLY_RETRY:
            return HandleTopmostApplyRetry(hwnd);

        case TIMER_ID_TOPMOST_VISIBILITY_RESTORE:
            if (CLOCK_IS_DRAGGING) {
                SetTimer(hwnd, TIMER_ID_TOPMOST_VISIBILITY_RESTORE, 100, NULL);
                return TRUE;
            }
            KillTimer(hwnd, TIMER_ID_TOPMOST_VISIBILITY_RESTORE);
            return HandleTopmostVisibilityChange(hwnd, NULL);

        case TIMER_ID_FORCE_REDRAW:
            if (CLOCK_IS_DRAGGING) {
                SetTimer(hwnd, TIMER_ID_FORCE_REDRAW, 100, NULL);
                return TRUE;
            }
            return HandleForceRedraw(hwnd);

        case TIMER_ID_EDIT_MODE_REFRESH:
            if (CLOCK_IS_DRAGGING) {
                SetTimer(hwnd, TIMER_ID_EDIT_MODE_REFRESH, 100, NULL);
                return TRUE;
            }
            KillTimer(hwnd, TIMER_ID_EDIT_MODE_REFRESH);
            return HandleForceRedraw(hwnd);

        case TIMER_ID_FONT_VALIDATION:
            return HandleFontValidation(hwnd);

        case TIMER_ID_MAIN:
            if (!MainTimer_IsRunning()) {
                return TRUE;
            }
            return HandleMainTimer(hwnd);

        case TIMER_ID_RENDER_ANIMATION:
            /* Pure render tick - decouples visual flow from logic update */
            RequestWindowRepaint(hwnd);
            return TRUE;

        default:
            return FALSE;
    }
}
