/**
 * @file pomodoro_loop.h
 * @brief Pure state transitions for Pomodoro loop progression.
 */

#ifndef TIMER_POMODORO_LOOP_H
#define TIMER_POMODORO_LOOP_H

#include <windows.h>

/**
 * Advance an interval index and completed-cycle count.
 * A loop count of zero represents an unlimited number of cycles.
 */
BOOL PomodoroLoop_Advance(int* timeIndex, int timesCount,
                          int* completedCycles, int loopCount);

#endif /* TIMER_POMODORO_LOOP_H */
