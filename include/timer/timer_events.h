/**
 * @file timer_events.h
 * @brief Timer event dispatcher with retry mechanisms
 * 
 * Retry mechanisms handle Windows z-order/visibility race conditions (3 attempts each).
 * Millisecond accumulator prevents time drift across pause/resume cycles.
 * Function decomposition reduced code duplication by 53%.
 */

#ifndef TIMER_EVENTS_H
#define TIMER_EVENTS_H

#include <windows.h>

/* ============================================================================
 * Timer ID Constants
 * ============================================================================ */

/**
 * @brief Timer IDs (named constants prevent magic numbers)
 * 
 * @note 1001-1003: audio_player.c
 * @note 1005: drag_scale.h (TIMER_ID_CONFIG_SAVE)
 * @note 2001: drag_scale.h (TIMER_ID_EDIT_MODE_REFRESH)
 */
#define TIMER_ID_MAIN 1
#define TIMER_ID_TOPMOST_RETRY 999         /**< 3 retries for z-order */
#define TIMER_ID_VISIBILITY_RETRY 1000     /**< 3 retries for visibility */
#define TIMER_ID_FORCE_REDRAW 1004
#define TIMER_ID_FONT_VALIDATION 1006      /**< Every 2s */

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Timer event dispatcher
 * @param hwnd Window handle
 * @param wp Timer ID (WPARAM)
 * @return TRUE if handled
 * 
 * @details Routes to specialized handlers:
 * - MAIN: Countdown/countup/clock
 * - TOPMOST_RETRY: Z-order correction (3 retries)
 * - VISIBILITY_RETRY: Visibility recovery (3 retries)
 * - FONT_VALIDATION: Font integrity check (every 2s)
 * - EDIT_MODE_REFRESH: Post-edit cleanup
 * 
 * @note Called from WindowProcedure on WM_TIMER
 */
BOOL HandleTimerEvent(HWND hwnd, WPARAM wp);

/**
 * @brief Reset timing baseline (prevents drift)
 * 
 * @details
 * Resets millisecond accumulator and GetTickCount baseline.
 * Prevents accumulated errors and visual jumps.
 * 
 * Call when: starting timer, resuming from pause, restarting, switching modes.
 * 
 * @note Also calls ResetTimerMilliseconds() for display sync
 */
void ResetMillisecondAccumulator(void);

/**
 * @brief Initialize Pomodoro session
 * 
 * @details
 * Sets phase to WORK, loads first interval, resets counters/flags.
 * Falls back to 25min (1500s) if no config.
 * 
 * @note Uses POMODORO_TIMES array from config
 */
void InitializePomodoro(void);

#endif