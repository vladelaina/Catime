/**
 * @file timer_events_main.c
 * @brief Main timer tick, completion dispatch, and paint-cache coordination.
 */

#include "timer_events_internal.h"
#include "timer/pomodoro_suspend.h"

BOOL TimerEvents_ShouldRenderMainTimer(void) {
    g_visibleTimerCurrentText[0] = L'\0';
    GetTimeText(g_visibleTimerCurrentText, TIME_TEXT_MAX_LEN);
    return TimerRenderCache_NeedsRepaint(g_lastPaintedTimerText,
                                         g_hasLastPaintedTimerText,
                                         g_visibleTimerCurrentText);
}

BOOL TimerEvents_ShouldCheckActiveTimerRender(int currentElapsedSecond,
                                              int* lastCheckedSecond,
                                              BOOL* hasLastCheckedSecond) {
    if (!lastCheckedSecond || !hasLastCheckedSecond) {
        return TRUE;
    }
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

void Timer_NotifyMainWindowPainted(const wchar_t* timerText) {
    TimerRenderCache_CommitPaint(g_lastPaintedTimerText,
                                 _countof(g_lastPaintedTimerText),
                                 &g_hasLastPaintedTimerText,
                                 timerText);
}

BOOL Timer_HasMainWindowPainted(void) {
    return g_hasLastPaintedTimerText && g_lastPaintedTimerText[0] != L'\0';
}

BOOL Timer_HasPresentedMainWindowFrame(void) {
    return g_hasLastPaintedTimerText;
}

void TimerEvents_HandleCountdownCompletion(HWND hwnd) {
    if (PomodoroSuspend_HasSnapshot()) {
        TimerEvents_ShowTimeoutNotification(
            hwnd, g_AppConfig.notification.messages.timeout_message, TRUE);
        TimerEvents_ResetTimerState(0);
        TimerEvents_ResetMillisecondAccumulator();
        MainTimer_Stop();
        return;
    }

    BOOL shouldNotify = CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_OPEN_FILE &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_LOCK &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHUTDOWN &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_RESTART &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SLEEP &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHOW_TIME &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_COUNT_UP &&
                        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_OPEN_WEBSITE;

    if (shouldNotify) {
        TimerEvents_ShowTimeoutNotification(
            hwnd, g_AppConfig.notification.messages.timeout_message, TRUE);
    }

    if (!TimerEvents_IsActivePomodoroTimer()) {
        ResetPomodoroState();
    }

    if (TimerEvents_ExecuteSystemAction(hwnd, CLOCK_TIMEOUT_ACTION)) {
        return;
    }

    TimerEvents_HandleTimeoutActions(hwnd);
    if (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHOW_TIME &&
        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_COUNT_UP) {
        TimerEvents_ResetTimerState(0);
        TimerEvents_ResetMillisecondAccumulator();
    }
}

static BOOL HandleMainTimer(HWND hwnd) {
    static DWORD s_lastTopmostCheck = 0;
    static UINT s_lastDesiredInterval = 0;
    static int s_lastActiveRenderCheckSecond = 0;
    static BOOL s_hasLastActiveRenderCheckSecond = FALSE;
    DWORD nowTick = GetTickCount();
    UINT desiredInterval = GetTimerInterval();

    if (s_lastDesiredInterval != desiredInterval) {
        s_lastDesiredInterval = desiredInterval;
        MainTimer_SetInterval(desiredInterval);
    }

    if (!CLOCK_IS_DRAGGING &&
        (s_lastTopmostCheck == 0 ||
         (nowTick - s_lastTopmostCheck) >= 500)) {
        TryRestorePendingWindowPosition(hwnd);
        EnforceTopmostOverTaskbar(hwnd);
        s_lastTopmostCheck = nowTick;
    }

    if (CLOCK_SHOW_CURRENT_TIME) {
        last_displayed_second = -1;
        s_hasLastActiveRenderCheckSecond = FALSE;
        if (TimerEvents_ShouldRenderMainTimer()) {
            TimerEvents_RequestWindowRepaint(hwnd);
        }
        return TRUE;
    }

    if (CLOCK_IS_PAUSED) {
        s_hasLastActiveRenderCheckSecond = FALSE;
        if (TimerEvents_ShouldRenderMainTimer()) {
            TimerEvents_RequestWindowRepaint(hwnd);
        }
        return TRUE;
    }

    int64_t currentTimeMs = GetAbsoluteTimeMs();
    int currentElapsedSec = 0;
    if (CLOCK_COUNT_UP) {
        int64_t elapsedMs = currentTimeMs - g_start_time;
        if (elapsedMs < 0) elapsedMs = 0;
        currentElapsedSec = (int)(elapsedMs / 1000);
        countup_elapsed_time = currentElapsedSec;
    } else {
        int64_t remainingMs = g_target_end_time - currentTimeMs;
        if (remainingMs < 0) remainingMs = 0;
        int remainingSecRounded = (int)((remainingMs + 999) / 1000);
        currentElapsedSec = CLOCK_TOTAL_TIME - remainingSecRounded;
        if (currentElapsedSec > CLOCK_TOTAL_TIME) {
            currentElapsedSec = CLOCK_TOTAL_TIME;
        }
        if (currentElapsedSec < 0) currentElapsedSec = 0;
        countdown_elapsed_time = currentElapsedSec;
    }

    if (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0 &&
        countdown_elapsed_time >= CLOCK_TOTAL_TIME) {
        if (!countdown_message_shown) {
            countdown_message_shown = true;
            TrayAnimation_RecomputeTimerDelay();

            BOOL pomodoroAdvanced = FALSE;
            if (TimerEvents_IsActivePomodoroTimer()) {
                pomodoroAdvanced = TimerEvents_HandlePomodoroCompletion(hwnd);
            } else {
                TimerEvents_HandleCountdownCompletion(hwnd);
            }
            if (pomodoroAdvanced) {
                return TRUE;
            }
        }
        countdown_elapsed_time = CLOCK_TOTAL_TIME;
    }

    if (TimerEvents_ShouldCheckActiveTimerRender(currentElapsedSec,
                                                 &s_lastActiveRenderCheckSecond,
                                                 &s_hasLastActiveRenderCheckSecond) &&
        TimerEvents_ShouldRenderMainTimer()) {
        TimerEvents_RequestWindowRepaint(hwnd);
    }

    return TRUE;
}

BOOL TimerEvents_HandleMainTimer(HWND hwnd) {
    return HandleMainTimer(hwnd);
}
