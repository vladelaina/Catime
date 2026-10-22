/**
 * @file pomodoro_navigation.c
 * @brief Active Pomodoro interval navigation.
 */

#include "timer_events_internal.h"
#include "timer/pomodoro_navigation.h"

BOOL PomodoroNavigation_CanJumpToTimeIndex(int timeIndex)
{
    return TimerEvents_IsActivePomodoroTimer() &&
           timeIndex >= 0 &&
           timeIndex < pomodoro_initial_times_count &&
           pomodoro_initial_times[timeIndex] > 0;
}

int PomodoroNavigation_GetActiveTimeCount(void)
{
    if (!TimerEvents_IsActivePomodoroTimer()) {
        return 0;
    }
    return pomodoro_initial_times_count;
}

int PomodoroNavigation_GetActiveTimeSeconds(int timeIndex)
{
    if (!PomodoroNavigation_CanJumpToTimeIndex(timeIndex)) {
        return 0;
    }
    return pomodoro_initial_times[timeIndex];
}

BOOL PomodoroNavigation_JumpToTimeIndex(HWND hwnd, int timeIndex)
{
    if (!PomodoroNavigation_CanJumpToTimeIndex(timeIndex)) {
        return FALSE;
    }

    StopNotificationSound();
    CloseAllNotifications();

    current_pomodoro_time_index = timeIndex;
    CLOCK_SHOW_CURRENT_TIME = false;
    CLOCK_COUNT_UP = false;
    CLOCK_TOTAL_TIME = pomodoro_initial_times[timeIndex];
    ResetTimer();

    if (!MainTimer_Start(hwnd, GetTimerInterval())) {
        CLOCK_IS_PAUSED = true;
        g_pause_start_time = GetAbsoluteTimeMs();
        LOG_WARNING("Failed to start Pomodoro interval after jump; keeping it paused");
        InvalidateRect(hwnd, NULL, TRUE);
        return FALSE;
    }

    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}
