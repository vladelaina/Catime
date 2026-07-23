/**
 * @file timer_events_pomodoro.c
 * @brief Pomodoro session state and interval completion handling.
 */

#include <string.h>
#include <wchar.h>

#include "timer_events_internal.h"

BOOL TimerEvents_AdvancePomodoroState(void) {
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

BOOL TimerEvents_IsActivePomodoroTimer(void) {
    if (current_pomodoro_phase == POMODORO_PHASE_IDLE ||
        pomodoro_initial_times_count == 0 ||
        current_pomodoro_time_index >= pomodoro_initial_times_count) {
        return FALSE;
    }

    return CLOCK_TOTAL_TIME ==
           pomodoro_initial_times[current_pomodoro_time_index];
}

void TimerEvents_FormatPomodoroTime(int seconds,
                                    wchar_t* buffer,
                                    size_t bufferSize) {
    if (seconds < 60) {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%ds", seconds);
        return;
    }

    int minutes = seconds / 60;
    int remainingSeconds = seconds % 60;
    if (remainingSeconds > 0) {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE,
                     L"%dm%ds", minutes, remainingSeconds);
    } else {
        _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%dm", minutes);
    }
}

static void BuildCompletionMessage(wchar_t* completionMsg,
                                   size_t completionMsgSize,
                                   int completedIndex,
                                   int timesCount,
                                   int loopCount,
                                   int currentCycle,
                                   int stepInCycle) {
    wchar_t timeStr[32];
    if (completedIndex < pomodoro_initial_times_count) {
        TimerEvents_FormatPomodoroTime(pomodoro_initial_times[completedIndex],
                                       timeStr, _countof(timeStr));
    } else {
        wcscpy_s(timeStr, _countof(timeStr), L"?");
    }

    const wchar_t* completedText =
        GetLocalizedString(NULL, L"Pomodoro completed");
    if (timesCount <= 1 && loopCount <= 1) {
        _snwprintf_s(completionMsg, completionMsgSize, _TRUNCATE,
                     L"%ls %ls", timeStr, completedText);
        return;
    }

    const wchar_t* cycleText = GetLocalizedString(NULL, L"Cycle");
    const wchar_t* roundText = GetLocalizedString(NULL, L"Round");
    _snwprintf_s(completionMsg, completionMsgSize, _TRUNCATE,
                 L"%ls %ls (%ls%d/%d%ls %d/%d)",
                 timeStr, completedText, cycleText, currentCycle, loopCount,
                 roundText, stepInCycle, timesCount);
}

BOOL TimerEvents_HandlePomodoroCompletion(HWND hwnd) {
    wchar_t completionMsg[256];
    int completedIndex = current_pomodoro_time_index;
    int timesCount = pomodoro_initial_times_count;
    int loopCount = pomodoro_initial_loop_count;

    if (timesCount <= 0) timesCount = 1;
    if (loopCount <= 0) loopCount = 1;

    int stepInCycle = completedIndex + 1;
    int currentCycle = complete_pomodoro_cycles + 1;
    BuildCompletionMessage(completionMsg, _countof(completionMsg),
                           completedIndex, timesCount, loopCount,
                           currentCycle, stepInCycle);

    if (!TimerEvents_AdvancePomodoroState()) {
        ShowNotification(hwnd, completionMsg);
        TimerEvents_ResetTimerState(0);
        ResetPomodoroState();

        const wchar_t* allCompleted =
            GetLocalizedString(NULL, L"All Pomodoro cycles completed!");
        ShowNotification(hwnd, allCompleted);
        PlayNotificationSound(hwnd);

        CLOCK_COUNT_UP = false;
        CLOCK_SHOW_CURRENT_TIME = false;
        message_shown = TRUE;
        InvalidateRect(hwnd, NULL, TRUE);
        MainTimer_Stop();
        return FALSE;
    }

    ShowNotification(hwnd, completionMsg);
    PlayNotificationSound(hwnd);

    /* Preserve the absolute deadline so notification work introduces no drift. */
    int nextDurationSec =
        pomodoro_initial_times[current_pomodoro_time_index];
    TimerEvents_ResetTimerState(nextDurationSec);
    g_target_end_time += (int64_t)nextDurationSec * 1000;
    countdown_message_shown = false;

    InitializeHighPrecisionTimer();
    TimerEvents_ResetMillisecondAccumulator();
    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
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

    memset(pomodoro_initial_times, 0, sizeof(pomodoro_initial_times));
    for (int i = 0; i < pomodoro_initial_times_count; i++) {
        pomodoro_initial_times[i] = g_AppConfig.pomodoro.times[i];
    }

    CLOCK_TOTAL_TIME = pomodoro_initial_times_count > 0
        ? pomodoro_initial_times[0]
        : DEFAULT_POMODORO_DURATION;
    countdown_elapsed_time = 0;
    countdown_message_shown = false;
    TimerEvents_ResetMillisecondAccumulator();
}
