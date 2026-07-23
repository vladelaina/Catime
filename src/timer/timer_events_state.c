/**
 * @file timer_events_state.c
 * @brief Storage for timer event state shared by the implementation modules.
 */

#include "timer_events_internal.h"

int current_pomodoro_time_index = 0;
POMODORO_PHASE current_pomodoro_phase = POMODORO_PHASE_IDLE;
int complete_pomodoro_cycles = 0;

int pomodoro_initial_times_count = 0;
int pomodoro_initial_loop_count = 0;
int pomodoro_initial_times[MAX_POMODORO_TIMES] = {0};

DWORD last_timer_tick = 0;
int ms_accumulator = 0;
wchar_t g_visibleTimerCurrentText[TIME_TEXT_MAX_LEN] = {0};
wchar_t g_lastPaintedTimerText[TIME_TEXT_MAX_LEN] = {0};
BOOL g_hasLastPaintedTimerText = FALSE;
TimeoutActionType g_armedTimeoutSystemAction = TIMEOUT_ACTION_MESSAGE;
