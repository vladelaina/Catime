/**
 * @file pomodoro_loop.c
 * @brief Pure state transitions for Pomodoro loop progression.
 */

#include <limits.h>

#include "config/config_defaults.h"
#include "timer/pomodoro_loop.h"

BOOL PomodoroLoop_Advance(int* timeIndex, int timesCount,
                          int* completedCycles, int loopCount)
{
    if (!timeIndex || !completedCycles || timesCount <= 0 ||
        *timeIndex < 0 || *timeIndex >= timesCount ||
        !PomodoroLoopCount_IsValid(loopCount)) {
        return FALSE;
    }

    (*timeIndex)++;
    if (*timeIndex < timesCount) {
        return TRUE;
    }

    *timeIndex = 0;
    if (*completedCycles < INT_MAX) {
        (*completedCycles)++;
    }

    return PomodoroLoopCount_IsInfinite(loopCount) ||
           *completedCycles < loopCount;
}
