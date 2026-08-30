#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config/config_defaults.h"
#include "pomodoro.h"
#include "timer/pomodoro_suspend.h"
#include "timer/timer.h"

bool CLOCK_IS_PAUSED = false;
bool CLOCK_SHOW_CURRENT_TIME = false;
bool CLOCK_COUNT_UP = false;
int32_t CLOCK_TOTAL_TIME = 0;
int32_t countdown_elapsed_time = 0;
int32_t countup_elapsed_time = 0;
int32_t elapsed_time = 0;
int32_t last_displayed_second = -1;
bool countdown_message_shown = false;
int message_shown = 0;
int64_t g_target_end_time = 0;
int64_t g_start_time = 0;
int64_t g_pause_start_time = 0;

POMODORO_PHASE current_pomodoro_phase = POMODORO_PHASE_IDLE;
int current_pomodoro_time_index = 0;
int complete_pomodoro_cycles = 0;
int pomodoro_initial_times_count = 0;
int pomodoro_initial_loop_count = 0;
int pomodoro_initial_times[MAX_POMODORO_TIMES] = {0};
DWORD last_timer_tick = 0;
int ms_accumulator = 0;

static int64_t g_now = 0;

int64_t GetAbsoluteTimeMs(void) {
    return g_now;
}

BOOL TimerEvents_IsActivePomodoroTimer(void) {
    return current_pomodoro_phase != POMODORO_PHASE_IDLE &&
           current_pomodoro_time_index >= 0 &&
           current_pomodoro_time_index < pomodoro_initial_times_count &&
           CLOCK_TOTAL_TIME == pomodoro_initial_times[current_pomodoro_time_index];
}

void ResetPomodoroState(void) {
    current_pomodoro_phase = POMODORO_PHASE_IDLE;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;
    pomodoro_initial_times_count = 0;
    pomodoro_initial_loop_count = 0;
    memset(pomodoro_initial_times, 0, sizeof(pomodoro_initial_times));
}

static void SetPausedPomodoro(void) {
    current_pomodoro_phase = POMODORO_PHASE_WORK;
    current_pomodoro_time_index = 1;
    complete_pomodoro_cycles = 2;
    pomodoro_initial_times_count = 3;
    pomodoro_initial_loop_count = 4;
    pomodoro_initial_times[0] = 1500;
    pomodoro_initial_times[1] = 300;
    pomodoro_initial_times[2] = 900;
    CLOCK_TOTAL_TIME = 300;
    countdown_elapsed_time = 42;
    countup_elapsed_time = 17;
    elapsed_time = 42;
    countdown_message_shown = false;
    message_shown = 0;
    CLOCK_IS_PAUSED = true;
    CLOCK_SHOW_CURRENT_TIME = false;
    CLOCK_COUNT_UP = false;
    g_target_end_time = 123456;
    g_start_time = 120000;
    g_pause_start_time = 121000;
    last_timer_tick = 88;
    ms_accumulator = 23;
    last_displayed_second = 42;
}

static void TestSuspendAndRestore(void) {
    PomodoroSuspend_Discard();
    SetPausedPomodoro();
    assert(PomodoroSuspend_BeginTemporaryMode());
    assert(PomodoroSuspend_HasSnapshot());
    assert(current_pomodoro_phase == POMODORO_PHASE_IDLE);

    CLOCK_SHOW_CURRENT_TIME = true;
    CLOCK_TOTAL_TIME = 0;
    g_now = 200000;
    assert(PomodoroSuspend_Restore());

    assert(current_pomodoro_phase == POMODORO_PHASE_WORK);
    assert(current_pomodoro_time_index == 1);
    assert(complete_pomodoro_cycles == 2);
    assert(pomodoro_initial_times_count == 3);
    assert(pomodoro_initial_loop_count == 4);
    assert(pomodoro_initial_times[1] == 300);
    assert(CLOCK_TOTAL_TIME == 300);
    assert(countdown_elapsed_time == 42);
    assert(g_target_end_time == 123456);
    assert(CLOCK_IS_PAUSED);
    assert(!CLOCK_SHOW_CURRENT_TIME);
    assert(g_pause_start_time == 200000);
    assert(PomodoroSuspend_HasSnapshot());

    PomodoroSuspend_Discard();
    assert(!PomodoroSuspend_HasSnapshot());
}

static void TestOnlyPausedPomodoroCanBeSuspended(void) {
    PomodoroSuspend_Discard();
    SetPausedPomodoro();
    CLOCK_IS_PAUSED = false;
    assert(!PomodoroSuspend_BeginTemporaryMode());
    assert(current_pomodoro_phase == POMODORO_PHASE_WORK);
    assert(!PomodoroSuspend_HasSnapshot());
}

int main(void) {
    TestSuspendAndRestore();
    TestOnlyPausedPomodoroCanBeSuspended();
    puts("pomodoro suspend tests passed");
    return 0;
}
