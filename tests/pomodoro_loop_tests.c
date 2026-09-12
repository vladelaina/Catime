#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "config/config_defaults.h"
#include "timer/pomodoro_loop.h"

static void TestLoopCountValues(void)
{
    assert(PomodoroLoopCount_IsInfinite(POMODORO_LOOP_COUNT_INFINITE));
    assert(PomodoroLoopCount_IsValid(POMODORO_LOOP_COUNT_INFINITE));
    assert(PomodoroLoopCount_IsValid(1));
    assert(PomodoroLoopCount_IsValid(MAX_POMODORO_LOOP_COUNT));
    assert(!PomodoroLoopCount_IsValid(-1));
    assert(!PomodoroLoopCount_IsValid(MAX_POMODORO_LOOP_COUNT + 1));
    assert(PomodoroLoopCount_Normalize(-1) == DEFAULT_POMODORO_LOOP_COUNT);
    assert(PomodoroLoopCount_Normalize(0) == POMODORO_LOOP_COUNT_INFINITE);
    assert(PomodoroLoopCount_Normalize(101) == MAX_POMODORO_LOOP_COUNT);
}

static void TestFiniteLoopStopsAtConfiguredCount(void)
{
    int index = 0;
    int cycles = 0;

    assert(PomodoroLoop_Advance(&index, 2, &cycles, 2));
    assert(index == 1 && cycles == 0);
    assert(PomodoroLoop_Advance(&index, 2, &cycles, 2));
    assert(index == 0 && cycles == 1);
    assert(PomodoroLoop_Advance(&index, 2, &cycles, 2));
    assert(!PomodoroLoop_Advance(&index, 2, &cycles, 2));
    assert(index == 0 && cycles == 2);
}

static void TestInfiniteLoopNeverStopsOrOverflows(void)
{
    int index = 1;
    int cycles = INT_MAX;

    assert(PomodoroLoop_Advance(&index, 2, &cycles,
                                POMODORO_LOOP_COUNT_INFINITE));
    assert(index == 0 && cycles == INT_MAX);
}

static void TestInvalidStateIsRejected(void)
{
    int index = 0;
    int cycles = 0;

    assert(!PomodoroLoop_Advance(&index, 2, &cycles, -1));
    assert(index == 0 && cycles == 0);
}

int main(void)
{
    TestLoopCountValues();
    TestFiniteLoopStopsAtConfiguredCount();
    TestInfiniteLoopNeverStopsOrOverflows();
    TestInvalidStateIsRejected();
    puts("pomodoro loop tests passed");
    return 0;
}
