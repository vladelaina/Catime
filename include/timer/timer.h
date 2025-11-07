/**
 * @file timer.h
 * @brief Multi-modal timer with flexible input parsing
 * 
 * QueryPerformanceCounter provides sub-millisecond accuracy (prevents drift).
 * Multi-format parser accepts duration, absolute time, and unit combinations.
 * Adaptive formatting prevents visual jumps (aligned spacing, magnitude-based format).
 */

#ifndef TIMER_H
#define TIMER_H

#include <windows.h>
#include <time.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_TIME_OPTIONS 50

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Timeout action types
 */
typedef enum {
    TIMEOUT_ACTION_MESSAGE = 0,
    TIMEOUT_ACTION_LOCK = 1,
    TIMEOUT_ACTION_SHUTDOWN = 2,
    TIMEOUT_ACTION_RESTART = 3,
    TIMEOUT_ACTION_OPEN_FILE = 4,
    TIMEOUT_ACTION_SHOW_TIME = 5,
    TIMEOUT_ACTION_COUNT_UP = 6,
    TIMEOUT_ACTION_OPEN_WEBSITE = 7,
    TIMEOUT_ACTION_SLEEP = 8
} TimeoutActionType;

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/* Timer control */
extern BOOL CLOCK_IS_PAUSED;
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_USE_24HOUR;
extern BOOL CLOCK_SHOW_SECONDS;
extern BOOL CLOCK_COUNT_UP;
extern char CLOCK_STARTUP_MODE[20];

/* Timer data */
extern int CLOCK_TOTAL_TIME;
extern int countdown_elapsed_time;
extern int countup_elapsed_time;
extern time_t CLOCK_LAST_TIME_UPDATE;
extern int last_displayed_second;

/* Notification state (prevent duplicates) */
extern BOOL countdown_message_shown;
extern BOOL countup_message_shown;
extern int pomodoro_work_cycles;
extern int message_shown;
extern int elapsed_time;

/* Input dialog */
extern wchar_t inputText[256];
extern HWND g_hwndInputDialog;

/* Timeout actions */
extern TimeoutActionType CLOCK_TIMEOUT_ACTION;
extern char CLOCK_TIMEOUT_TEXT[50];
extern char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH];
extern wchar_t CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH];

/* Pomodoro settings */
extern int POMODORO_WORK_TIME;
extern int POMODORO_SHORT_BREAK;
extern int POMODORO_LONG_BREAK;
extern int POMODORO_LOOP_COUNT;

/* Quick presets */
extern int time_options[MAX_TIME_OPTIONS];
extern int time_options_count;

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief Format time by mode (adaptive alignment)
 * @param remaining_time Unused (legacy API compatibility)
 * @param time_text Output buffer (min 64 bytes)
 * 
 * @details Modes:
 * - SHOW_CURRENT_TIME: H:MM or H:MM:SS
 * - COUNT_UP: S, M:SS, or H:MM:SS (magnitude-based)
 * - Default: Countdown with leading spaces (visual alignment)
 * 
 * @note Uses cached last_displayed_second (reduces system calls)
 * @warning Not thread-safe
 */
void FormatTime(int remaining_time, char* time_text);

/**
 * @brief Parse flexible time input
 * @param input Input string
 * @param total_seconds Output
 * @return 1 on success, 0 on invalid
 * 
 * @details Formats:
 * - Units: "25m", "1h 30m", "90s"
 * - Shorthand: "25" (min), "1 30" (1:30), "1 30 15" (1:30:15)
 * - Absolute: "14:30t" (countdown to 2:30 PM today/tomorrow)
 * 
 * @note Validates via isValidInput() first
 */
int ParseInput(const char* input, int* total_seconds);

/**
 * @brief Validate input format
 * @param input String
 * @return 1 if valid
 * 
 * @details Accepts digits, spaces, trailing h/m/s/t (case-insensitive)
 */
int isValidInput(const char* input);

/**
 * @brief Write default start time to config
 * @param seconds Duration (>0)
 */
void WriteConfigDefaultStartTime(int seconds);

/**
 * @brief Reset timer to initial state
 * 
 * @details
 * Clears elapsed time, ensures valid CLOCK_TOTAL_TIME (fallback 60s),
 * unpauses, clears notification flags, reinitializes baseline.
 */
void ResetTimer(void);

/**
 * @brief Toggle pause state
 * 
 * @details
 * Pause: Freezes millisecond display, stops accumulation.
 * Resume: Resets baseline to prevent time jumps.
 */
void TogglePauseTimer(void);

/**
 * @brief Initialize high-precision counter
 * @return TRUE on success, FALSE if unsupported
 * 
 * @details
 * Establishes timing baseline via QueryPerformanceCounter.
 * Call when starting/resuming timer.
 */
BOOL InitializeHighPrecisionTimer(void);

#endif /* TIMER_H */