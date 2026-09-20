/**
 * @file timer_events_pomodoro.c
 * @brief Pomodoro session state and interval completion handling.
 */

#include <limits.h>
#include <string.h>
#include <wchar.h>

#include "timer_events_internal.h"
#include "timer/pomodoro_loop.h"
#include "timer/pomodoro_suspend.h"

BOOL TimerEvents_AdvancePomodoroState(void) {
    if (pomodoro_initial_times_count == 0) {
        return FALSE;
    }

    return PomodoroLoop_Advance(&current_pomodoro_time_index,
                                pomodoro_initial_times_count,
                                &complete_pomodoro_cycles,
                                pomodoro_initial_loop_count);
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

BOOL TimerEvents_SetActivePomodoroLoopCount(int loopCount) {
    if (!PomodoroLoopCount_IsValid(loopCount)) {
        return FALSE;
    }

    if (TimerEvents_IsActivePomodoroTimer()) {
        pomodoro_initial_loop_count = loopCount;
        return TRUE;
    }

    return PomodoroSuspend_SetLoopCount(loopCount);
}

BOOL TimerEvents_GetActivePomodoroLoopCount(int* loopCount) {
    if (!loopCount) {
        return FALSE;
    }

    if (TimerEvents_IsActivePomodoroTimer()) {
        *loopCount = pomodoro_initial_loop_count;
        return TRUE;
    }

    return PomodoroSuspend_GetLoopCount(loopCount);
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
                                   const wchar_t* loopCountText,
                                   int loopCount,
                                   BOOL isInfinite,
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
    if (timesCount <= 1 && !isInfinite && loopCount == 1) {
        _snwprintf_s(completionMsg, completionMsgSize, _TRUNCATE,
                     L"%ls %ls", timeStr, completedText);
        return;
    }

    const wchar_t* cycleText = GetLocalizedString(NULL, L"Cycle");
    const wchar_t* roundText = GetLocalizedString(NULL, L"Round");
    _snwprintf_s(completionMsg, completionMsgSize, _TRUNCATE,
                 L"%ls %ls (%ls%d/%ls%ls %d/%d)",
                 timeStr, completedText, cycleText, currentCycle, loopCountText,
                 roundText, stepInCycle, timesCount);
}

static void ReplacePomodoroVariable(const wchar_t* source, const wchar_t* token,
                                    const wchar_t* value, wchar_t* output,
                                    size_t outputSize) {
    size_t outputIndex = 0;
    size_t tokenLength = wcslen(token);
    for (size_t sourceIndex = 0; source[sourceIndex] && outputIndex + 1 < outputSize;) {
        const wchar_t* replacement = wcsncmp(source + sourceIndex, token,
                                              tokenLength) == 0 ? value : NULL;
        if (replacement) {
            for (size_t i = 0; replacement[i] && outputIndex + 1 < outputSize; ++i) {
                output[outputIndex++] = replacement[i];
            }
            sourceIndex += tokenLength;
        } else {
            output[outputIndex++] = source[sourceIndex++];
        }
    }
    output[outputIndex] = L'\0';
}

static BOOL BuildConfiguredPomodoroMessage(wchar_t* completionMsg,
                                           size_t completionMsgSize,
                                           const wchar_t* loopCountText,
                                           int currentCycle,
                                           int stepInCycle,
                                           int timesCount) {
    wchar_t configuredMessage[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE] = {0};
    if (!Utf8ToWide(g_AppConfig.notification.messages.timeout_message,
                    configuredMessage, _countof(configuredMessage)) ||
        configuredMessage[0] == L'\0') {
        return FALSE;
    }

    wchar_t cycleValue[32] = {0};
    wchar_t roundValue[32] = {0};
    wchar_t intermediate[NOTIFICATION_MESSAGE_CHAR_BUFFER_SIZE] = {0};
    _snwprintf_s(cycleValue, _countof(cycleValue), _TRUNCATE, L"%d/%ls",
                 currentCycle, loopCountText);
    _snwprintf_s(roundValue, _countof(roundValue), _TRUNCATE, L"%d/%d",
                 stepInCycle, timesCount);
    ReplacePomodoroVariable(configuredMessage, L"{Cycle}", cycleValue,
                            intermediate, _countof(intermediate));
    ReplacePomodoroVariable(intermediate, L"{Round}", roundValue,
                            completionMsg, completionMsgSize);
    return TRUE;
}

BOOL TimerEvents_HandlePomodoroCompletion(HWND hwnd) {
    wchar_t completionMsg[256];
    int completedIndex = current_pomodoro_time_index;
    int timesCount = pomodoro_initial_times_count;
    int loopCount = pomodoro_initial_loop_count;

    if (timesCount <= 0) timesCount = 1;
    if (!PomodoroLoopCount_IsValid(loopCount)) {
        loopCount = DEFAULT_POMODORO_LOOP_COUNT;
    }

    int stepInCycle = completedIndex + 1;
    BOOL isInfinite = PomodoroLoopCount_IsInfinite(loopCount);
    wchar_t loopCountText[16];
    if (isInfinite) {
        wcscpy_s(loopCountText, _countof(loopCountText), L"Inf");
    } else {
        _snwprintf_s(loopCountText, _countof(loopCountText), _TRUNCATE,
                     L"%d", loopCount);
    }
    int currentCycle = complete_pomodoro_cycles < INT_MAX
        ? complete_pomodoro_cycles + 1 : INT_MAX;
    BOOL useConfiguredMessage =
        g_AppConfig.notification.messages.use_for_pomodoro &&
        BuildConfiguredPomodoroMessage(completionMsg, _countof(completionMsg),
                                       loopCountText, currentCycle,
                                       stepInCycle, timesCount);
    if (!useConfiguredMessage) {
        BuildCompletionMessage(completionMsg, _countof(completionMsg),
                               completedIndex, timesCount, loopCountText,
                               loopCount, isInfinite, currentCycle,
                               stepInCycle);
    }

    if (!TimerEvents_AdvancePomodoroState()) {
        ShowNotification(hwnd, completionMsg);
        TimerEvents_ResetTimerState(0);
        ResetPomodoroState();

        if (!useConfiguredMessage) {
            const wchar_t* allCompleted =
                GetLocalizedString(NULL, L"All Pomodoro cycles completed!");
            ShowNotification(hwnd, allCompleted);
        }
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

    pomodoro_initial_loop_count = PomodoroLoopCount_Normalize(
        g_AppConfig.pomodoro.loop_count);

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
