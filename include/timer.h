/**
 * @file timer.h
 * @brief Core timer system with multi-modal time tracking and flexible input parsing
 * 
 * Provides countdown, count-up, and system clock display with high-precision timing,
 * flexible input formats, and configurable timeout actions. Supports Pomodoro technique
 * and customizable timer presets.
 * 
 * @architecture
 * - State Management: Global timer state variables with mode flags
 * - Time Parsing: Multi-format parser (duration, absolute time, units)
 * - Display: Adaptive formatting based on time magnitude and display mode
 * - Precision: QueryPerformanceCounter for sub-millisecond accuracy
 */

#ifndef TIMER_H
#define TIMER_H

#include <windows.h>
#include <time.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum number of user-configurable quick timer presets */
#define MAX_TIME_OPTIONS 50

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Action executed when countdown timer expires
 * 
 * Defines system-level and application-level responses to timer completion.
 * Actions range from simple notifications to system power management.
 */
typedef enum {
    TIMEOUT_ACTION_MESSAGE = 0,      /**< Display notification message */
    TIMEOUT_ACTION_LOCK = 1,         /**< Lock Windows workstation (LockWorkStation) */
    TIMEOUT_ACTION_SHUTDOWN = 2,     /**< Shutdown system (ExitWindowsEx) */
    TIMEOUT_ACTION_RESTART = 3,      /**< Restart system (ExitWindowsEx) */
    TIMEOUT_ACTION_OPEN_FILE = 4,    /**< Execute file via ShellExecute */
    TIMEOUT_ACTION_SHOW_TIME = 5,    /**< Switch to system clock display mode */
    TIMEOUT_ACTION_COUNT_UP = 6,     /**< Transition to count-up stopwatch mode */
    TIMEOUT_ACTION_OPEN_WEBSITE = 7, /**< Open URL in default browser */
    TIMEOUT_ACTION_SLEEP = 8         /**< Suspend system (SetSuspendState) */
} TimeoutActionType;

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/** @defgroup TimerControl Timer Control State
 * @brief Flags controlling timer behavior and display mode
 * @{ */
extern BOOL CLOCK_IS_PAUSED;          /**< Timer is paused (updates frozen) */
extern BOOL CLOCK_SHOW_CURRENT_TIME;  /**< Display system clock instead of timer */
extern BOOL CLOCK_USE_24HOUR;         /**< Use 24-hour format for clock display */
extern BOOL CLOCK_SHOW_SECONDS;       /**< Include seconds in clock display */
extern BOOL CLOCK_COUNT_UP;           /**< Count-up (stopwatch) vs countdown mode */
extern char CLOCK_STARTUP_MODE[20];   /**< Application launch behavior ("COUNTDOWN", etc.) */
/** @} */

/** @defgroup TimerData Timer Data and Elapsed Time
 * @brief Core timing values and accumulation
 * @{ */
extern int CLOCK_TOTAL_TIME;          /**< Total countdown duration in seconds */
extern int countdown_elapsed_time;    /**< Elapsed seconds in countdown mode */
extern int countup_elapsed_time;      /**< Elapsed seconds in count-up mode */
extern time_t CLOCK_LAST_TIME_UPDATE; /**< Last system time update (for clock mode) */
extern int last_displayed_second;     /**< Cached second value to prevent jitter */
/** @} */

/** @defgroup NotificationState Notification Tracking
 * @brief Prevent duplicate timeout notifications
 * @{ */
extern BOOL countdown_message_shown;  /**< Countdown completion alert already shown */
extern BOOL countup_message_shown;    /**< Count-up milestone alert already shown */
extern int pomodoro_work_cycles;      /**< Completed Pomodoro work sessions */
extern int message_shown;             /**< Generic message shown flag */
extern int elapsed_time;              /**< Generic elapsed time tracker */
/** @} */

/** @defgroup InputDialog Input Dialog State
 * @brief Global state for custom input dialogs
 * @{ */
extern wchar_t inputText[256];        /**< User input text buffer */
extern HWND g_hwndInputDialog;        /**< Active input dialog handle */
/** @} */

/** @defgroup TimeoutActions Timeout Action Configuration
 * @brief Settings for timer expiration behavior
 * @{ */
extern TimeoutActionType CLOCK_TIMEOUT_ACTION; /**< Action to execute on timeout */
extern char CLOCK_TIMEOUT_TEXT[50];            /**< Custom notification message text */
extern char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH]; /**< File to open on timeout */
extern wchar_t CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH]; /**< URL to open on timeout */
/** @} */

/** @defgroup PomodoroSettings Pomodoro Technique Configuration
 * @brief Interval durations for Pomodoro workflow
 * @{ */
extern int POMODORO_WORK_TIME;        /**< Work session duration (default: 25 min) */
extern int POMODORO_SHORT_BREAK;      /**< Short break duration (default: 5 min) */
extern int POMODORO_LONG_BREAK;       /**< Long break duration (default: 15 min) */
extern int POMODORO_LOOP_COUNT;       /**< Work sessions before long break */
/** @} */

/** @defgroup QuickTimers Quick Timer Presets
 * @brief User-configured frequently-used timer durations
 * @{ */
extern int time_options[MAX_TIME_OPTIONS]; /**< Preset durations in seconds */
extern int time_options_count;             /**< Number of active presets */
/** @} */

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief Format time for display based on current timer mode
 * @param remaining_time Unused legacy parameter (kept for API stability)
 * @param time_text Output buffer for formatted string (minimum 64 bytes recommended)
 * 
 * @details Behavior varies by mode:
 * - CLOCK_SHOW_CURRENT_TIME: System time with H:MM or H:MM:SS format
 * - CLOCK_COUNT_UP: Adaptive stopwatch (S, M:SS, or H:MM:SS based on magnitude)
 * - Default: Countdown timer with visual alignment using leading spaces
 * 
 * @performance Uses cached last_displayed_second to minimize system calls
 * @threadsafety Not thread-safe (modifies global state)
 */
void FormatTime(int remaining_time, char* time_text);

/**
 * @brief Parse flexible time input formats into total seconds
 * @param input User input string (null-terminated, max 255 chars)
 * @param total_seconds Output parameter for parsed duration
 * @return 1 on successful parse, 0 on invalid input
 * 
 * @details Supported input formats:
 * - Duration units: "25m", "1h 30m", "90s", "1h 30m 15s"
 * - Numeric shorthand: "25" (minutes), "1 30" (1:30), "1 30 15" (1:30:15)
 * - Absolute time: "14:30t" or "14 30 45T" (countdown to that time today/tomorrow)
 * 
 * @examples
 * - "25" → 1500 seconds (25 minutes)
 * - "1h 30m" → 5400 seconds (90 minutes)
 * - "14:30t" → seconds until next 2:30 PM
 * 
 * @validation Calls isValidInput() first; rejects empty, negative, or malformed input
 * @limits Maximum value: INT_MAX seconds (~68 years)
 */
int ParseInput(const char* input, int* total_seconds);

/**
 * @brief Validate time input string before parsing
 * @param input String to validate (null-terminated)
 * @return 1 if valid format, 0 if invalid
 * 
 * @details Accepts:
 * - Digits (0-9)
 * - Spaces (for separation)
 * - Trailing unit suffixes: h, m, s, t (case-insensitive, only at end)
 * 
 * @note Must contain at least one digit; empty strings rejected
 */
int isValidInput(const char* input);

/**
 * @brief Persist default timer start duration to config.ini
 * @param seconds Duration in seconds (must be > 0)
 * 
 * @details Writes to [Timer] section as CLOCK_DEFAULT_START_TIME
 * @sideeffects Triggers config file write (may cause disk I/O)
 * @note This is a convenience wrapper; actual implementation in config.c
 */
void WriteConfigDefaultStartTime(int seconds);

/**
 * @brief Reset timer to initial state based on current mode
 * 
 * @details Actions performed:
 * - Clears elapsed time (countdown or count-up based on CLOCK_COUNT_UP)
 * - Ensures valid CLOCK_TOTAL_TIME (fallback to 60 seconds)
 * - Unpauses timer (CLOCK_IS_PAUSED = FALSE)
 * - Clears notification flags
 * - Reinitializes high-precision timer baseline
 * 
 * @sideeffects Modifies global timer state, resets millisecond accumulator
 */
void ResetTimer(void);

/**
 * @brief Toggle timer between paused and running states
 * 
 * @details Behavior on pause:
 * - Freezes millisecond display via PauseTimerMilliseconds()
 * - Stops elapsed time accumulation
 * 
 * Behavior on resume:
 * - Resets precision timer baseline to prevent time jumps
 * - Resets millisecond accumulator for smooth display
 * 
 * @sideeffects Modifies CLOCK_IS_PAUSED and reinitializes timing subsystems
 */
void TogglePauseTimer(void);

/**
 * @brief Initialize high-precision performance counter
 * @return TRUE on success, FALSE if hardware doesn't support QueryPerformanceCounter
 * 
 * @details Call when starting timer or resuming from pause to establish
 * timing baseline. Internal state managed automatically.
 * 
 * @performance Uses Windows QueryPerformanceCounter for sub-millisecond accuracy
 */
BOOL InitializeHighPrecisionTimer(void);

#endif /* TIMER_H */