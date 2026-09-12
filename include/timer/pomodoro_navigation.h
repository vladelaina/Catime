/**
 * @file pomodoro_navigation.h
 * @brief Navigation between intervals of the active Pomodoro session.
 */

#ifndef TIMER_POMODORO_NAVIGATION_H
#define TIMER_POMODORO_NAVIGATION_H

#include <windows.h>

/**
 * Return whether an interval of the active Pomodoro session can be selected.
 * Paused sessions are selectable and will resume when selected.
 */
BOOL PomodoroNavigation_CanJumpToTimeIndex(int timeIndex);

/** Return the interval count captured by the active Pomodoro session, or 0. */
int PomodoroNavigation_GetActiveTimeCount(void);

/** Return an active-session interval duration, or 0 when the index is invalid. */
int PomodoroNavigation_GetActiveTimeSeconds(int timeIndex);

/**
 * Restart the selected interval of the active Pomodoro session.
 * The current completed-cycle count is preserved.
 */
BOOL PomodoroNavigation_JumpToTimeIndex(HWND hwnd, int timeIndex);

#endif /* TIMER_POMODORO_NAVIGATION_H */
