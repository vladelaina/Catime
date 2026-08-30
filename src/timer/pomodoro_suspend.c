/**
 * @file pomodoro_suspend.c
 * @brief Saves a paused Pomodoro session while another timer mode is shown.
 */

#include <string.h>

#include "timer_events_internal.h"
#include "timer/pomodoro_suspend.h"

typedef struct {
    BOOL valid;
    POMODORO_PHASE phase;
    int timeIndex;
    int completeCycles;
    int timesCount;
    int loopCount;
    int times[MAX_POMODORO_TIMES];
    int32_t totalTime;
    int32_t countdownElapsed;
    int32_t countupElapsed;
    int32_t savedElapsed;
    BOOL countdownMessageShown;
    int messageShown;
    int64_t targetEndTime;
    int64_t startTime;
    int64_t pauseStartTime;
    DWORD lastTimerTick;
    int millisecondAccumulator;
    int32_t lastDisplayedSecond;
} SuspendedPomodoroSnapshot;

static SuspendedPomodoroSnapshot g_suspendedPomodoro = {0};

static BOOL IsPausedActivePomodoro(void) {
    return CLOCK_IS_PAUSED && TimerEvents_IsActivePomodoroTimer();
}

BOOL PomodoroSuspend_HasSnapshot(void) {
    return g_suspendedPomodoro.valid;
}

BOOL PomodoroSuspend_BeginTemporaryMode(void) {
    if (g_suspendedPomodoro.valid) {
        return TRUE;
    }
    if (!IsPausedActivePomodoro()) {
        return FALSE;
    }

    g_suspendedPomodoro.phase = current_pomodoro_phase;
    g_suspendedPomodoro.timeIndex = current_pomodoro_time_index;
    g_suspendedPomodoro.completeCycles = complete_pomodoro_cycles;
    g_suspendedPomodoro.timesCount = pomodoro_initial_times_count;
    g_suspendedPomodoro.loopCount = pomodoro_initial_loop_count;
    memcpy(g_suspendedPomodoro.times, pomodoro_initial_times,
           sizeof(g_suspendedPomodoro.times));
    g_suspendedPomodoro.totalTime = CLOCK_TOTAL_TIME;
    g_suspendedPomodoro.countdownElapsed = countdown_elapsed_time;
    g_suspendedPomodoro.countupElapsed = countup_elapsed_time;
    g_suspendedPomodoro.savedElapsed = elapsed_time;
    g_suspendedPomodoro.countdownMessageShown = countdown_message_shown;
    g_suspendedPomodoro.messageShown = message_shown;
    g_suspendedPomodoro.targetEndTime = g_target_end_time;
    g_suspendedPomodoro.startTime = g_start_time;
    g_suspendedPomodoro.pauseStartTime = g_pause_start_time;
    g_suspendedPomodoro.lastTimerTick = last_timer_tick;
    g_suspendedPomodoro.millisecondAccumulator = ms_accumulator;
    g_suspendedPomodoro.lastDisplayedSecond = last_displayed_second;
    g_suspendedPomodoro.valid = TRUE;

    ResetPomodoroState();
    return TRUE;
}

BOOL PomodoroSuspend_Restore(void) {
    if (!g_suspendedPomodoro.valid) {
        return FALSE;
    }

    current_pomodoro_phase = g_suspendedPomodoro.phase;
    current_pomodoro_time_index = g_suspendedPomodoro.timeIndex;
    complete_pomodoro_cycles = g_suspendedPomodoro.completeCycles;
    pomodoro_initial_times_count = g_suspendedPomodoro.timesCount;
    pomodoro_initial_loop_count = g_suspendedPomodoro.loopCount;
    memcpy(pomodoro_initial_times, g_suspendedPomodoro.times,
           sizeof(pomodoro_initial_times));
    CLOCK_SHOW_CURRENT_TIME = false;
    CLOCK_COUNT_UP = false;
    CLOCK_TOTAL_TIME = g_suspendedPomodoro.totalTime;
    countdown_elapsed_time = g_suspendedPomodoro.countdownElapsed;
    countup_elapsed_time = g_suspendedPomodoro.countupElapsed;
    elapsed_time = g_suspendedPomodoro.savedElapsed;
    countdown_message_shown = g_suspendedPomodoro.countdownMessageShown;
    message_shown = g_suspendedPomodoro.messageShown;
    g_target_end_time = g_suspendedPomodoro.targetEndTime;
    g_start_time = g_suspendedPomodoro.startTime;
    last_timer_tick = g_suspendedPomodoro.lastTimerTick;
    ms_accumulator = g_suspendedPomodoro.millisecondAccumulator;
    last_displayed_second = g_suspendedPomodoro.lastDisplayedSecond;

    /* Time spent in the temporary mode must not be charged to Pomodoro. */
    CLOCK_IS_PAUSED = true;
    g_pause_start_time = GetAbsoluteTimeMs();
    return TRUE;
}

void PomodoroSuspend_Discard(void) {
    ZeroMemory(&g_suspendedPomodoro, sizeof(g_suspendedPomodoro));
}
