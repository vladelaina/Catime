/**
 * @file timer.h
 * @brief Timer core functionality definition
 * 
 * This header file defines timer-related data structures, status enumerations, and global variables,
 * including countdown/count-up mode switching, timeout action handling, and other core functionality declarations.
 */

#ifndef TIMER_H
#define TIMER_H

#include <windows.h>
#include <time.h>

/// Maximum number of preset time options
#define MAX_TIME_OPTIONS 10

/// Pomodoro time variables (in seconds)
extern int POMODORO_WORK_TIME;    ///< Pomodoro work time (default 25 minutes)
extern int POMODORO_SHORT_BREAK;  ///< Pomodoro short break time (default 5 minutes)
extern int POMODORO_LONG_BREAK;   ///< Pomodoro long break time (default 15 minutes)
extern int POMODORO_LOOP_COUNT;   ///< Pomodoro loop count (default 1 cycle)

/**
 * @brief Timeout action type enumeration
 * 
 * Defines different operation types executed after timer timeout:
 * - MESSAGE: Display notification message
 * - LOCK: Lock computer
 * - SHUTDOWN: Shut down system
 * - RESTART: Restart system
 * - OPEN_FILE: Open specified file
 * - SHOW_TIME: Display current time
 * - COUNT_UP: Start count-up timer
 * - OPEN_WEBSITE: Open website
 * - SLEEP: Sleep mode
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

// Timer status --------------------------------------------------
extern BOOL CLOCK_IS_PAUSED;         ///< Pause state flag
extern BOOL CLOCK_SHOW_CURRENT_TIME; ///< Real-time clock display mode
extern BOOL CLOCK_USE_24HOUR;        ///< 24-hour format display
extern BOOL CLOCK_SHOW_SECONDS;      ///< Display seconds
extern BOOL CLOCK_COUNT_UP;          ///< Count-up mode switch
extern char CLOCK_STARTUP_MODE[20];  ///< Startup mode (COUNTDOWN/COUNTUP)

// Time related --------------------------------------------------
extern int CLOCK_TOTAL_TIME;         ///< Total timer time (seconds)
extern int countdown_elapsed_time;    ///< Countdown elapsed time
extern int countup_elapsed_time;     ///< Count-up accumulated time
extern time_t CLOCK_LAST_TIME_UPDATE;///< Last update timestamp
extern int last_displayed_second;    ///< Last displayed second, used for time display synchronization

// Message status ------------------------------------------------
extern BOOL countdown_message_shown; ///< Countdown notification display status
extern BOOL countup_message_shown;   ///< Count-up notification display status
extern int pomodoro_work_cycles;     ///< Pomodoro work cycle count

// Timeout action configuration ----------------------------------
extern TimeoutActionType CLOCK_TIMEOUT_ACTION; ///< Current timeout action type
extern char CLOCK_TIMEOUT_TEXT[50];            ///< Timeout notification text content
extern char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH];  ///< Timeout file path to open
extern char CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH]; ///< Timeout website URL to open

// Time options configuration ------------------------------------
extern int time_options[MAX_TIME_OPTIONS]; ///< Preset time options array
extern int time_options_count;             ///< Number of valid options

/**
 * @brief Format time display
 * @param remaining_time Remaining time (seconds)
 * @param time_text Output buffer (at least 9 bytes)
 * 
 * Converts seconds to "HH:MM:SS" format based on current timer mode (24-hour/display seconds),
 * automatically handles the display differences between countdown/count-up.
 */
void FormatTime(int remaining_time, char* time_text);

/**
 * @brief Parse user input time
 * @param input User input string
 * @param total_seconds Output parsed total seconds
 * @return int Returns 1 if parsing successful, 0 if failed
 * 
 * Supports "MM:SS", "HH:MM:SS", and pure numeric seconds format,
 * maximum support for 99 hours 59 minutes 59 seconds (359999 seconds).
 */
int ParseInput(const char* input, int* total_seconds);

/**
 * @brief Validate time input format
 * @param input String to validate
 * @return int Returns 1 if valid, 0 if invalid
 * 
 * Checks if input conforms to the following formats:
 * - Pure numeric (seconds)
 * - MM:SS (1-59 minutes or 00-59 seconds)
 * - HH:MM:SS (00-99 hours 00-59 minutes 00-59 seconds)
 */
int isValidInput(const char* input);

/**
 * @brief Write default startup time configuration
 * @param seconds Default startup time (seconds)
 * 
 * Updates the CLOCK_DEFAULT_START_TIME item in the configuration file,
 * affects timer initial value and reset operation.
 */
void WriteConfigDefaultStartTime(int seconds);

/**
 * @brief Write startup mode configuration
 * @param mode Startup mode string ("COUNTDOWN"/"COUNTUP")
 * 
 * Modifies the STARTUP_MODE item in the configuration file,
 * controls the default timer mode when the program starts.
 */
void WriteConfigStartupMode(const char* mode);

#endif // TIMER_H