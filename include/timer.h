/**
 * @file timer.h
 * @brief Timer functionality and timeout actions
 * 
 * Core timer system with countdown, count-up, and Pomodoro functionality
 */

#ifndef TIMER_H
#define TIMER_H

#include <windows.h>
#include <time.h>

/** @brief Maximum number of configurable time options */
#define MAX_TIME_OPTIONS 50

/** @brief Pomodoro work phase duration in seconds */
extern int POMODORO_WORK_TIME;

/** @brief Pomodoro short break duration in seconds */
extern int POMODORO_SHORT_BREAK;

/** @brief Pomodoro long break duration in seconds */
extern int POMODORO_LONG_BREAK;

/** @brief Number of Pomodoro cycles before long break */
extern int POMODORO_LOOP_COUNT;

/**
 * @brief Actions to execute when timer expires
 */
typedef enum {
    TIMEOUT_ACTION_MESSAGE = 0,      /**< Show notification message */
    TIMEOUT_ACTION_LOCK = 1,         /**< Lock workstation */
    TIMEOUT_ACTION_SHUTDOWN = 2,     /**< Shutdown system */
    TIMEOUT_ACTION_RESTART = 3,      /**< Restart system */
    TIMEOUT_ACTION_OPEN_FILE = 4,    /**< Open specified file */
    TIMEOUT_ACTION_SHOW_TIME = 5,    /**< Switch to current time display */
    TIMEOUT_ACTION_COUNT_UP = 6,     /**< Start count-up timer */
    TIMEOUT_ACTION_OPEN_WEBSITE = 7, /**< Open website URL */
    TIMEOUT_ACTION_SLEEP = 8         /**< Put system to sleep */
} TimeoutActionType;

/** @brief Timer paused state */
extern BOOL CLOCK_IS_PAUSED;

/** @brief Current time display mode active */
extern BOOL CLOCK_SHOW_CURRENT_TIME;

/** @brief Use 24-hour time format */
extern BOOL CLOCK_USE_24HOUR;

/** @brief Show seconds in time display */
extern BOOL CLOCK_SHOW_SECONDS;

/** @brief Count-up timer mode active */
extern BOOL CLOCK_COUNT_UP;

/** @brief Application startup mode configuration */
extern char CLOCK_STARTUP_MODE[20];

/** @brief Total timer duration in seconds */
extern int CLOCK_TOTAL_TIME;

/** @brief Elapsed time for countdown timer */
extern int countdown_elapsed_time;

/** @brief Elapsed time for count-up timer */
extern int countup_elapsed_time;

/** @brief Last time update timestamp */
extern time_t CLOCK_LAST_TIME_UPDATE;

/** @brief Last displayed second for update optimization */
extern int last_displayed_second;

/** @brief Countdown completion message shown flag */
extern BOOL countdown_message_shown;

/** @brief Count-up completion message shown flag */
extern BOOL countup_message_shown;

/** @brief Completed Pomodoro work cycles count */
extern int pomodoro_work_cycles;

/** @brief Current timeout action type */
extern TimeoutActionType CLOCK_TIMEOUT_ACTION;

/** @brief Timeout notification text */
extern char CLOCK_TIMEOUT_TEXT[50];

/** @brief File path for timeout file action */
extern char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH];

/** @brief Website URL for timeout web action */
extern wchar_t CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH];

/** @brief Array of configurable time options */
extern int time_options[MAX_TIME_OPTIONS];

/** @brief Count of configured time options */
extern int time_options_count;

/**
 * @brief Format time in seconds to display string
 * @param remaining_time Time in seconds to format
 * @param time_text Buffer to store formatted time string
 */
void FormatTime(int remaining_time, char* time_text);

/**
 * @brief Parse user time input string to seconds
 * @param input Input string (e.g., "25", "1h 30m", "90s")
 * @param total_seconds Output buffer for parsed seconds
 * @return 1 if parsing successful, 0 otherwise
 */
int ParseInput(const char* input, int* total_seconds);

/**
 * @brief Validate time input string format
 * @param input Input string to validate
 * @return 1 if valid format, 0 otherwise
 */
int isValidInput(const char* input);

/**
 * @brief Write default startup time to configuration
 * @param seconds Default time in seconds
 */
void WriteConfigDefaultStartTime(int seconds);

/**
 * @brief Write startup mode to configuration
 * @param mode Startup mode string
 */
void WriteConfigStartupMode(const char* mode);

#endif