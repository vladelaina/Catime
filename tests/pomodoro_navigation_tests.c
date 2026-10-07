#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "config/config_defaults.h"
#include "log.h"
#include "pomodoro.h"
#include "timer/pomodoro_navigation.h"
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

static int64_t g_now = 100000;
static BOOL g_startTimerSucceeds = TRUE;
static int g_startTimerCalls = 0;
static int g_closeNotificationCalls = 0;
static int g_stopSoundCalls = 0;

int64_t GetAbsoluteTimeMs(void)
{
    return g_now;
}

BOOL TimerEvents_IsActivePomodoroTimer(void)
{
    return current_pomodoro_phase != POMODORO_PHASE_IDLE &&
           current_pomodoro_time_index >= 0 &&
           current_pomodoro_time_index < pomodoro_initial_times_count &&
           !CLOCK_SHOW_CURRENT_TIME && !CLOCK_COUNT_UP &&
           CLOCK_TOTAL_TIME == pomodoro_initial_times[current_pomodoro_time_index];
}

void ResetTimer(void)
{
    CLOCK_IS_PAUSED = false;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;
    countdown_message_shown = false;
    g_pause_start_time = 0;
    g_target_end_time = g_now + (int64_t)CLOCK_TOTAL_TIME * 1000;
}

BOOL MainTimer_Start(HWND hwnd, UINT intervalMs)
{
    (void)hwnd;
    (void)intervalMs;
    g_startTimerCalls++;
    return g_startTimerSucceeds;
}

UINT GetTimerInterval(void)
{
    return 100;
}

void StopNotificationSound(void)
{
    g_stopSoundCalls++;
}

void CloseAllNotifications(void)
{
    g_closeNotificationCalls++;
}

void WriteLog(LogLevel level, const char* format, ...)
{
    (void)level;
    (void)format;
}

static void SetActivePomodoro(void)
{
    current_pomodoro_phase = POMODORO_PHASE_WORK;
    current_pomodoro_time_index = 1;
    complete_pomodoro_cycles = 3;
    pomodoro_initial_times_count = 3;
    pomodoro_initial_loop_count = 4;
    pomodoro_initial_times[0] = 1500;
    pomodoro_initial_times[1] = 300;
    pomodoro_initial_times[2] = 900;
    CLOCK_TOTAL_TIME = 300;
    CLOCK_IS_PAUSED = false;
    CLOCK_SHOW_CURRENT_TIME = false;
    CLOCK_COUNT_UP = false;
    countdown_elapsed_time = 42;
    g_target_end_time = 101000;
    g_startTimerSucceeds = TRUE;
    g_startTimerCalls = 0;
    g_closeNotificationCalls = 0;
    g_stopSoundCalls = 0;
}

static void TestJumpRestartsSelectedInterval(void)
{
    SetActivePomodoro();

    assert(PomodoroNavigation_CanJumpToTimeIndex(2));
    assert(PomodoroNavigation_GetActiveTimeCount() == 3);
    assert(PomodoroNavigation_GetActiveTimeSeconds(2) == 900);
    assert(PomodoroNavigation_JumpToTimeIndex(NULL, 2));
    assert(current_pomodoro_time_index == 2);
    assert(complete_pomodoro_cycles == 3);
    assert(CLOCK_TOTAL_TIME == 900);
    assert(countdown_elapsed_time == 0);
    assert(g_target_end_time == g_now + 900000);
    assert(!CLOCK_IS_PAUSED);
    assert(g_startTimerCalls == 1);
    assert(g_closeNotificationCalls == 1);
    assert(g_stopSoundCalls == 1);
}

static void TestPausedJumpResumes(void)
{
    SetActivePomodoro();
    CLOCK_IS_PAUSED = true;
    g_pause_start_time = g_now - 500;

    assert(PomodoroNavigation_JumpToTimeIndex(NULL, 0));
    assert(current_pomodoro_time_index == 0);
    assert(CLOCK_TOTAL_TIME == 1500);
    assert(!CLOCK_IS_PAUSED);
    assert(g_pause_start_time == 0);
}

static void TestCurrentIntervalRestarts(void)
{
    SetActivePomodoro();

    assert(PomodoroNavigation_JumpToTimeIndex(NULL, 1));
    assert(current_pomodoro_time_index == 1);
    assert(CLOCK_TOTAL_TIME == 300);
    assert(countdown_elapsed_time == 0);
    assert(g_target_end_time == g_now + 300000);
}

static void TestInvalidJumpDoesNotChangeState(void)
{
    SetActivePomodoro();
    CLOCK_TOTAL_TIME = 123;

    assert(!PomodoroNavigation_CanJumpToTimeIndex(1));
    assert(PomodoroNavigation_GetActiveTimeCount() == 0);
    assert(PomodoroNavigation_GetActiveTimeSeconds(1) == 0);
    assert(!PomodoroNavigation_JumpToTimeIndex(NULL, 1));
    assert(current_pomodoro_time_index == 1);
    assert(CLOCK_TOTAL_TIME == 123);
    assert(g_startTimerCalls == 0);
    assert(g_closeNotificationCalls == 0);
    assert(g_stopSoundCalls == 0);
}

static void TestStartFailureLeavesTargetPaused(void)
{
    SetActivePomodoro();
    g_startTimerSucceeds = FALSE;

    assert(!PomodoroNavigation_JumpToTimeIndex(NULL, 0));
    assert(current_pomodoro_time_index == 0);
    assert(CLOCK_TOTAL_TIME == 1500);
    assert(CLOCK_IS_PAUSED);
    assert(g_pause_start_time == g_now);
}

int main(void)
{
    TestJumpRestartsSelectedInterval();
    TestPausedJumpResumes();
    TestCurrentIntervalRestarts();
    TestInvalidJumpDoesNotChangeState();
    TestStartFailureLeavesTargetPaused();
    puts("pomodoro navigation tests passed");
    return 0;
}
