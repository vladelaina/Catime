/**
 * @file pomodoro_suspend.h
 * @brief Temporary-mode suspension for a paused Pomodoro session.
 */

#ifndef TIMER_POMODORO_SUSPEND_H
#define TIMER_POMODORO_SUSPEND_H

#include <windows.h>

/**
 * Save the currently paused Pomodoro session and clear its active state.
 *
 * @return TRUE when a Pomodoro session was saved, or when one was already
 *         saved for the current temporary mode.
 */
BOOL PomodoroSuspend_BeginTemporaryMode(void);

/** Return TRUE while a paused Pomodoro session can be resumed. */
BOOL PomodoroSuspend_HasSnapshot(void);

/** Update the loop count stored in a paused, suspended Pomodoro session. */
BOOL PomodoroSuspend_SetLoopCount(int loopCount);

/** Get the loop count stored in a paused, suspended Pomodoro session. */
BOOL PomodoroSuspend_GetLoopCount(int* loopCount);

/**
 * Restore the saved Pomodoro session in its paused state.
 *
 * The caller starts the main timer after it has completed any required UI
 * transition. The snapshot is retained until PomodoroSuspend_Discard().
 */
BOOL PomodoroSuspend_Restore(void);

/** Discard a saved session after a successful resume or explicit replacement. */
void PomodoroSuspend_Discard(void);

#endif /* TIMER_POMODORO_SUSPEND_H */
