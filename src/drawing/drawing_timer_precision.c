/**
 * @file drawing_timer_precision.c
 * @brief High-precision timer state for sub-second display
 */

#include <windows.h>
#include "drawing/drawing_timer_precision.h"

/** High-resolution timer state (sub-second precision) */
static DWORD g_timer_start_tick = 0;
static BOOL g_timer_ms_initialized = FALSE;
static int g_paused_milliseconds = 0;

void ResetTimerMilliseconds(void) {
    g_timer_start_tick = GetTickCount();
    g_timer_ms_initialized = TRUE;
    g_paused_milliseconds = 0;
}

void PauseTimerMilliseconds(void) {
    if (g_timer_ms_initialized) {
        DWORD current_tick = GetTickCount();
        DWORD elapsed_ms = current_tick - g_timer_start_tick;
        g_paused_milliseconds = (int)(elapsed_ms % 1000);
    }
}

/** @return Elapsed centiseconds, frozen during pause to prevent visual jumps */
int GetElapsedCentiseconds(void) {
    extern BOOL CLOCK_IS_PAUSED;
    
    if (CLOCK_IS_PAUSED) {
        return g_paused_milliseconds / 10;
    }
    
    if (!g_timer_ms_initialized) {
        ResetTimerMilliseconds();
        return 0;
    }
    
    DWORD current_tick = GetTickCount();
    DWORD elapsed_ms = current_tick - g_timer_start_tick;
    return (int)((elapsed_ms % 1000) / 10);
}

int GetSystemCentiseconds(void) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return st.wMilliseconds / 10;
}

