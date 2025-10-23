/**
 * @file timer_events.h
 * @brief High-precision timer event handling with modular architecture
 * 
 * Provides comprehensive timer event processing including:
 * - Main countdown/countup timer management
 * - Pomodoro technique implementation with cycle tracking
 * - Window positioning retry mechanisms
 * - Font validation and auto-recovery
 * - Timeout action execution (system commands, notifications, mode transitions)
 * 
 * Refactored for:
 * - Reduced code duplication (53% reduction)
 * - Improved maintainability through function decomposition
 * - Type-safe timer ID management
 * - Clear separation of concerns
 * 
 * @version 2.0 - Major refactoring for code quality and modularity
 */

#ifndef TIMER_EVENTS_H
#define TIMER_EVENTS_H

#include <windows.h>

/* ============================================================================
 * Timer ID Constants
 * ============================================================================ */

/**
 * @brief Well-defined timer identifiers for clarity and maintainability
 * 
 * Each timer serves a specific purpose in the application lifecycle.
 * Using named constants instead of magic numbers improves code readability.
 * 
 * @note Timer IDs 1001-1003 are used by audio_player.c
 * @note Timer ID 1005 is used by drag_scale.h (TIMER_ID_CONFIG_SAVE)
 * @note Timer ID 2001 is defined in drag_scale.h (TIMER_ID_EDIT_MODE_REFRESH)
 */
#define TIMER_ID_MAIN 1                    /**< Main countdown/countup/clock timer */
#define TIMER_ID_TOPMOST_RETRY 999         /**< Retry mechanism for topmost window positioning */
#define TIMER_ID_VISIBILITY_RETRY 1000     /**< Retry mechanism for normal window visibility */
#define TIMER_ID_FORCE_REDRAW 1004         /**< Force window redraw after mode change */
#define TIMER_ID_FONT_VALIDATION 1006      /**< Periodic font path validation and auto-fix */

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Main timer event dispatcher for all application timers
 * 
 * Central hub for processing timer messages from the Windows message loop.
 * Routes timer events to specialized handlers based on timer ID.
 * 
 * Handles multiple timer types:
 * - TIMER_ID_MAIN: Core application timing (countdown/countup/clock)
 * - TIMER_ID_TOPMOST_RETRY: Window z-order correction (3 retries)
 * - TIMER_ID_VISIBILITY_RETRY: Window visibility recovery (3 retries)
 * - TIMER_ID_FONT_VALIDATION: Font integrity checking (every 2 seconds)
 * - TIMER_ID_EDIT_MODE_REFRESH: Post-edit mode cleanup
 * 
 * @param hwnd Window handle receiving timer event
 * @param wp Timer ID parameter (WPARAM)
 * @return TRUE if event was handled, FALSE if unrecognized timer
 * 
 * @note This function is called from WindowProcedure on WM_TIMER messages
 */
BOOL HandleTimerEvent(HWND hwnd, WPARAM wp);

/**
 * @brief Reset high-precision timing baseline to prevent time drift
 * 
 * Resets both the millisecond accumulator and the GetTickCount baseline
 * to ensure accurate timing after state changes. Must be called when:
 * - Starting a new timer
 * - Resuming from pause
 * - Restarting current timer
 * - Switching timer modes
 * 
 * This prevents accumulated timing errors and visual jumps in the display.
 * 
 * @note Also resets display milliseconds through ResetTimerMilliseconds()
 * @see ResetTimerMilliseconds in drawing.h
 */
void ResetMillisecondAccumulator(void);

/**
 * @brief Initialize Pomodoro technique timer session
 * 
 * Sets up a new Pomodoro session with configured time intervals.
 * Initializes state tracking for work/break cycles and resets all counters.
 * 
 * Initialization includes:
 * - Setting phase to POMODORO_PHASE_WORK
 * - Loading first time interval from configuration
 * - Resetting cycle and index counters
 * - Clearing notification flags
 * 
 * @note Uses POMODORO_TIMES array from configuration
 * @note Falls back to 25 minutes (1500s) if no configuration exists
 * 
 * @see POMODORO_TIMES, POMODORO_TIMES_COUNT in config
 */
void InitializePomodoro(void);

#endif