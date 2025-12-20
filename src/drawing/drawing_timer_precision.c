/**
 * @file drawing_timer_precision.c
 * @brief High-precision timer state for sub-second display
 */

#include <windows.h>
#include "drawing/drawing_timer_precision.h"

/**
 * @file drawing_timer_precision.c
 * @brief High-precision timer state for sub-second display
 */

#include <windows.h>
#include <stdint.h>
#include "drawing/drawing_timer_precision.h"

/* Legacy stubs for compatibility */
void ResetTimerMilliseconds(void) {}
void PauseTimerMilliseconds(void) {}

/** @return Elapsed centiseconds (0-99) synchronized with main timer */
int GetElapsedCentiseconds(void) {
    extern BOOL CLOCK_IS_PAUSED;
    extern BOOL CLOCK_COUNT_UP;
    extern BOOL CLOCK_SHOW_CURRENT_TIME;
    extern int64_t g_target_end_time;
    extern int64_t g_start_time;
    extern int64_t g_pause_start_time;
    extern int64_t GetAbsoluteTimeMs(void);
    
    int64_t calc_time_point;
    
    if (CLOCK_IS_PAUSED) {
        calc_time_point = g_pause_start_time;
    } else {
        calc_time_point = GetAbsoluteTimeMs();
    }
    
    if (CLOCK_SHOW_CURRENT_TIME) {
        return (int)((calc_time_point % 1000) / 10);
    }
    
    if (CLOCK_COUNT_UP) {
        int64_t elapsed_ms = calc_time_point - g_start_time;
        if (elapsed_ms < 0) elapsed_ms = 0;
        return (int)((elapsed_ms % 1000) / 10);
    } else {
        int64_t remaining_ms = g_target_end_time - calc_time_point;
        if (remaining_ms < 0) remaining_ms = 0;
        return (int)((remaining_ms % 1000) / 10);
    }
}

int GetSystemCentiseconds(void) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return st.wMilliseconds / 10;
}

