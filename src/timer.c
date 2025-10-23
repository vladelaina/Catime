/**
 * @file timer.c
 * @brief Core timer system with precision timing and flexible input parsing
 * 
 * Implements multi-modal time tracking (countdown, count-up, system clock),
 * high-precision elapsed time calculation using QueryPerformanceCounter,
 * and a powerful multi-format time input parser.
 * 
 * @architecture
 * - State: Global variables for timer state (pause, mode, elapsed time)
 * - Precision: High-resolution performance counter for sub-millisecond accuracy
 * - Parsing: Recursive descent parser for duration and absolute time formats
 * - Formatting: Adaptive display with visual alignment based on time magnitude
 */

#include "../include/timer.h"
#include "../include/config.h"
#include "../include/timer_events.h"
#include "../include/drawing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Time conversion constants */
#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600
#define MINUTES_PER_HOUR 60
#define MILLISECONDS_PER_SECOND 1000.0
#define DEFAULT_FALLBACK_TIME 60  /* 1 minute fallback for invalid timer */

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/* Timer Control Flags */
BOOL CLOCK_IS_PAUSED = FALSE;
BOOL CLOCK_SHOW_CURRENT_TIME = FALSE;
BOOL CLOCK_USE_24HOUR = TRUE;
BOOL CLOCK_SHOW_SECONDS = TRUE;
BOOL CLOCK_COUNT_UP = FALSE;
char CLOCK_STARTUP_MODE[20] = "COUNTDOWN";

/* Timer Data */
int CLOCK_TOTAL_TIME = 0;
int countdown_elapsed_time = 0;
int countup_elapsed_time = 0;
time_t CLOCK_LAST_TIME_UPDATE = 0;
int last_displayed_second = -1;

/* High-Precision Timing State */
static LARGE_INTEGER timer_frequency = {0};
static LARGE_INTEGER timer_last_count = {0};
static BOOL high_precision_timer_initialized = FALSE;

/* Notification State */
BOOL countdown_message_shown = FALSE;
BOOL countup_message_shown = FALSE;
int pomodoro_work_cycles = 0;

/* Timeout Configuration */
TimeoutActionType CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
char CLOCK_TIMEOUT_TEXT[50] = "";
char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH] = "";

/* Pomodoro Configuration */
int POMODORO_WORK_TIME = 25 * SECONDS_PER_MINUTE;
int POMODORO_SHORT_BREAK = 5 * SECONDS_PER_MINUTE;
int POMODORO_LONG_BREAK = 15 * SECONDS_PER_MINUTE;
int POMODORO_LOOP_COUNT = 1;

/* Quick Timer Presets */
int time_options[MAX_TIME_OPTIONS] = {0};
int time_options_count = 0;

/* ============================================================================
 * High-Precision Timing - Performance Counter Management
 * ============================================================================ */

/**
 * @brief Initialize Windows performance counter for high-precision timing
 * @return TRUE on success, FALSE if hardware doesn't support QPC
 * 
 * @details Sets up frequency and baseline count for elapsed time calculation.
 * Call once at application start or when resetting timer baseline.
 */
BOOL InitializeHighPrecisionTimer(void) {
    if (!QueryPerformanceFrequency(&timer_frequency)) {
        return FALSE;
    }
    if (!QueryPerformanceCounter(&timer_last_count)) {
        return FALSE;
    }
    high_precision_timer_initialized = TRUE;
    return TRUE;
}

/**
 * @brief Calculate elapsed milliseconds since last query
 * @return Elapsed time in milliseconds (floating-point for sub-ms precision)
 * 
 * @details Auto-initializes on first call. Updates internal baseline for next call.
 * Uses double precision to avoid integer overflow on long-running timers.
 */
static double GetElapsedMilliseconds(void) {
    if (!high_precision_timer_initialized) {
        if (!InitializeHighPrecisionTimer()) {
            return 0.0;
        }
    }
    
    LARGE_INTEGER current_count;
    if (!QueryPerformanceCounter(&current_count)) {
        return 0.0;
    }
    
    double elapsed = (double)(current_count.QuadPart - timer_last_count.QuadPart) 
                     * MILLISECONDS_PER_SECOND / (double)timer_frequency.QuadPart;
    timer_last_count = current_count;
    
    return elapsed;
}

/**
 * @brief Accumulate elapsed time to countdown/count-up counters
 * 
 * @details Respects CLOCK_IS_PAUSED flag. Clamps countdown to CLOCK_TOTAL_TIME.
 * Called periodically from FormatTime() during active timer updates.
 */
static void UpdateElapsedTime(void) {
    if (CLOCK_IS_PAUSED) {
        return;
    }
    
    double elapsed_ms = GetElapsedMilliseconds();
    int elapsed_sec = (int)(elapsed_ms / MILLISECONDS_PER_SECOND);
    
    if (CLOCK_COUNT_UP) {
        countup_elapsed_time += elapsed_sec;
    } else {
        countdown_elapsed_time += elapsed_sec;
        if (countdown_elapsed_time > CLOCK_TOTAL_TIME) {
            countdown_elapsed_time = CLOCK_TOTAL_TIME;
        }
    }
}

/* ============================================================================
 * Time Formatting - Display String Generation
 * ============================================================================ */

/**
 * @brief Format time components into aligned display string
 * @param hours Hours component
 * @param minutes Minutes component (0-59)
 * @param seconds Seconds component (0-59)
 * @param buffer Output buffer for formatted string
 * @param buffer_size Size of output buffer
 * 
 * @details Adaptive formatting:
 * - Hours present: "H:MM:SS"
 * - Minutes only: "    M:SS" or "   MM:SS"
 * - Seconds only: "        S" or "       SS"
 * 
 * Leading spaces maintain visual alignment across different time magnitudes.
 */
static void FormatTimeComponents(int hours, int minutes, int seconds, 
                                 char* buffer, size_t buffer_size) {
    if (hours > 0) {
        snprintf(buffer, buffer_size, "%d:%02d:%02d", hours, minutes, seconds);
    } else if (minutes > 0) {
        snprintf(buffer, buffer_size, "    %d:%02d", minutes, seconds);
    } else {
        if (seconds < 10) {
            snprintf(buffer, buffer_size, "          %d", seconds);
        } else {
            snprintf(buffer, buffer_size, "        %d", seconds);
        }
    }
}

/**
 * @brief Convert 24-hour to 12-hour format
 * @param hour24 Hour in 24-hour format (0-23)
 * @return Hour in 12-hour format (1-12)
 */
static int ConvertTo12HourFormat(int hour24) {
    if (hour24 == 0) return 12;      /* Midnight */
    if (hour24 > 12) return hour24 - 12;
    return hour24;
}

/**
 * @brief Format system clock time with optional seconds
 * @param time_text Output buffer for formatted time string
 * 
 * @details Uses last_displayed_second cache to prevent jittery updates.
 * Validates second transitions for smooth visual experience.
 */
static void FormatSystemClock(char* time_text) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    /* Smooth second transition detection */
    if (last_displayed_second != -1) {
        int expected_next = (last_displayed_second + 1) % SECONDS_PER_MINUTE;
        BOOL is_rollover = (last_displayed_second == (SECONDS_PER_MINUTE - 1) && st.wSecond == 0);
        
        if (st.wSecond != expected_next && !is_rollover) {
            if (st.wSecond != last_displayed_second) {
                last_displayed_second = st.wSecond;
            }
        } else {
            last_displayed_second = st.wSecond;
        }
    } else {
        last_displayed_second = st.wSecond;
    }
    
    int display_hour = CLOCK_USE_24HOUR ? st.wHour : ConvertTo12HourFormat(st.wHour);
    
    if (CLOCK_SHOW_SECONDS) {
        snprintf(time_text, 64, "%d:%02d:%02d", display_hour, st.wMinute, last_displayed_second);
    } else {
        snprintf(time_text, 64, "%d:%02d", display_hour, st.wMinute);
    }
}

/**
 * @brief Format count-up stopwatch time
 * @param time_text Output buffer for formatted time string
 * 
 * @details Updates elapsed time and formats adaptively based on magnitude.
 */
static void FormatCountUpTime(char* time_text) {
    UpdateElapsedTime();
    
    int hours = countup_elapsed_time / SECONDS_PER_HOUR;
    int minutes = (countup_elapsed_time % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
    int seconds = countup_elapsed_time % SECONDS_PER_MINUTE;
    
    FormatTimeComponents(hours, minutes, seconds, time_text, 64);
}

/**
 * @brief Format countdown timer with remaining time
 * @param time_text Output buffer for formatted time string
 * 
 * @details Updates elapsed time, calculates remaining, and formats with alignment.
 */
static void FormatCountdownTime(char* time_text) {
    UpdateElapsedTime();
    
    int remaining = CLOCK_TOTAL_TIME - countdown_elapsed_time;
    if (remaining <= 0) {
        snprintf(time_text, 64, "    0:00");
        return;
    }
    
    int hours = remaining / SECONDS_PER_HOUR;
    int minutes = (remaining % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
    int seconds = remaining % SECONDS_PER_MINUTE;
    
    FormatTimeComponents(hours, minutes, seconds, time_text, 64);
}

/**
 * @brief Format time for display based on current mode
 * @param remaining_time Unused legacy parameter (kept for API compatibility)
 * @param time_text Output buffer (minimum 64 bytes recommended)
 * 
 * @details Dispatches to mode-specific formatters:
 * - System clock (CLOCK_SHOW_CURRENT_TIME)
 * - Count-up stopwatch (CLOCK_COUNT_UP)
 * - Countdown timer (default)
 */
void FormatTime(int remaining_time, char* time_text) {
    (void)remaining_time;  /* Unused, kept for API stability */
    
    if (CLOCK_SHOW_CURRENT_TIME) {
        FormatSystemClock(time_text);
    } else if (CLOCK_COUNT_UP) {
        FormatCountUpTime(time_text);
    } else {
        FormatCountdownTime(time_text);
    }
}

/* ============================================================================
 * Input Validation
 * ============================================================================ */

/**
 * @brief Validate time input string format
 * @param input String to validate (null-terminated)
 * @return 1 if valid, 0 if invalid
 * 
 * @details Accepts digits, spaces, and trailing unit suffixes (h/m/s/t).
 * Must contain at least one digit. Suffixes only allowed at end.
 */
int isValidInput(const char* input) {
    if (!input || !*input) return 0;
    
    int len = strlen(input);
    int digit_count = 0;
    
    for (int i = 0; i < len; i++) {
        if (isdigit(input[i])) {
            digit_count++;
        } else if (input[i] == ' ') {
            /* Spaces allowed for separation */
        } else if (i == len - 1) {
            /* Unit suffixes only at end */
            char c = tolower((unsigned char)input[i]);
            if (c != 'h' && c != 'm' && c != 's' && c != 't') {
                return 0;
            }
        } else {
            return 0;  /* Invalid character in middle */
        }
    }
    
    return digit_count > 0;
}

/* ============================================================================
 * Time Parsing - Multi-Format Input Parser
 * ============================================================================ */

/**
 * @brief Time unit multipliers for conversion to seconds
 */
typedef struct {
    char unit;
    int multiplier;
} TimeUnit;

static const TimeUnit TIME_UNITS[] = {
    {'h', SECONDS_PER_HOUR},
    {'m', SECONDS_PER_MINUTE},
    {'s', 1}
};

/**
 * @brief Parse duration with explicit units (e.g., "1h 30m 15s")
 * @param input Modified input string (strtok will mutate)
 * @return Total seconds, or -1 on parse error
 * 
 * @details Tokenizes on spaces, extracts value+unit pairs.
 * Handles mixed formats: "5h 30m", "25m", "90s"
 */
static int ParseDurationWithUnits(char* input) {
    int total = 0;
    char* parts[10];
    int part_count = 0;
    
    /* Tokenize input */
    char* token = strtok(input, " ");
    while (token && part_count < 10) {
        parts[part_count++] = token;
        token = strtok(NULL, " ");
    }
    
    /* Process each part */
    for (int i = 0; i < part_count; i++) {
        char* part = parts[i];
        int part_len = strlen(part);
        
        /* Check if part has unit suffix */
        char unit = tolower((unsigned char)part[part_len - 1]);
        BOOL has_unit = (unit == 'h' || unit == 'm' || unit == 's');
        
        if (has_unit) {
            part[part_len - 1] = '\0';  /* Remove unit */
            int value = atoi(part);
            
            /* Apply multiplier */
            for (size_t j = 0; j < sizeof(TIME_UNITS) / sizeof(TIME_UNITS[0]); j++) {
                if (TIME_UNITS[j].unit == unit) {
                    total += value * TIME_UNITS[j].multiplier;
                    break;
                }
            }
        } else {
            /* No unit: infer from position in multi-part input */
            int value = atoi(part);
            if (part_count == 2) {
                total += (i == 0) ? value * SECONDS_PER_MINUTE : value;  /* MM SS */
            } else if (part_count == 3) {
                int multipliers[] = {SECONDS_PER_HOUR, SECONDS_PER_MINUTE, 1};  /* HH MM SS */
                total += value * multipliers[i];
            } else {
                total += value * SECONDS_PER_MINUTE;  /* Default to minutes */
            }
        }
    }
    
    return total;
}

/**
 * @brief Parse numeric shorthand without units (e.g., "25", "1 30", "1 30 15")
 * @param input Modified input string (strtok will mutate)
 * @return Total seconds
 * 
 * @details Infers units from part count:
 * - 1 part: minutes
 * - 2 parts: minutes seconds
 * - 3 parts: hours minutes seconds
 */
static int ParseNumericShorthand(char* input) {
    char* parts[3];
    int part_count = 0;
    
    char* token = strtok(input, " ");
    while (token && part_count < 3) {
        parts[part_count++] = token;
        token = strtok(NULL, " ");
    }
    
    if (part_count == 1) {
        return atoi(parts[0]) * SECONDS_PER_MINUTE;  /* Minutes */
    } else if (part_count == 2) {
        return atoi(parts[0]) * SECONDS_PER_MINUTE + atoi(parts[1]);  /* MM:SS */
    } else if (part_count == 3) {
        return atoi(parts[0]) * SECONDS_PER_HOUR + atoi(parts[1]) * SECONDS_PER_MINUTE + atoi(parts[2]);  /* HH:MM:SS */
    }
    
    return 0;
}

/**
 * @brief Parse absolute target time (e.g., "14:30t" → countdown to 2:30 PM)
 * @param input Modified input string (trailing 't' already removed)
 * @return Seconds until target time (may span into next day)
 * 
 * @details If target time is in the past, assumes next occurrence (tomorrow).
 */
static int ParseAbsoluteTime(char* input) {
    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    struct tm tm_target = *tm_now;
    
    /* Parse time components */
    int hour = -1, minute = -1, second = -1;
    int count = 0;
    char* token = strtok(input, " ");
    
    while (token && count < 3) {
        int value = atoi(token);
        if (count == 0) hour = value;
        else if (count == 1) minute = value;
        else if (count == 2) second = value;
        count++;
        token = strtok(NULL, " ");
    }
    
    /* Build target time */
    if (hour >= 0) {
        tm_target.tm_hour = hour;
        tm_target.tm_min = (minute >= 0) ? minute : 0;
        tm_target.tm_sec = (second >= 0) ? second : 0;
    }
    
    time_t target_time = mktime(&tm_target);
    
    /* If target is in past, assume next day */
    if (target_time <= now) {
        tm_target.tm_mday += 1;
        target_time = mktime(&tm_target);
    }
    
    return (int)difftime(target_time, now);
}

/**
 * @brief Check if input contains time unit suffixes
 * @param input Input string to check
 * @return TRUE if any h/m/s suffixes found, FALSE otherwise
 */
static BOOL HasTimeUnits(const char* input) {
    for (const char* p = input; *p; p++) {
        char c = tolower((unsigned char)*p);
        if (c == 'h' || c == 'm' || c == 's') {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief Parse flexible time input into total seconds
 * @param input User input string (max 255 chars)
 * @param total_seconds Output parameter for parsed duration
 * @return 1 on success, 0 on invalid input
 * 
 * @details Supports three input modes:
 * 1. Absolute time: "14:30t" (suffix t/T)
 * 2. Duration with units: "1h 30m 15s"
 * 3. Numeric shorthand: "25" (minutes), "1 30" (MM:SS)
 */
int ParseInput(const char* input, int* total_seconds) {
    if (!isValidInput(input)) return 0;
    
    char input_copy[256];
    strncpy(input_copy, input, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';
    
    int len = strlen(input_copy);
    int result = 0;
    
    /* Check for absolute time mode (trailing 't' or 'T') */
    if (len > 0 && (input_copy[len - 1] == 't' || input_copy[len - 1] == 'T')) {
        input_copy[len - 1] = '\0';
        result = ParseAbsoluteTime(input_copy);
    } 
    /* Check for duration with units */
    else if (HasTimeUnits(input_copy)) {
        result = ParseDurationWithUnits(input_copy);
    } 
    /* Numeric shorthand */
    else {
        result = ParseNumericShorthand(input_copy);
    }
    
    /* Validate result */
    if (result <= 0 || result > INT_MAX) {
        return 0;
    }
    
    *total_seconds = result;
    return 1;
}

/* ============================================================================
 * Configuration Persistence
 * ============================================================================ */

/**
 * @brief Write default timer start duration to config.ini
 * @param seconds Duration in seconds
 * 
 * @details Persists to [Timer] section as CLOCK_DEFAULT_START_TIME
 */
void WriteConfigDefaultStartTime(int seconds) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", seconds, config_path);
}

/* ============================================================================
 * Timer Control - State Management
 * ============================================================================ */

/**
 * @brief Reset timer to initial state based on current mode
 * 
 * @details Clears elapsed time, ensures valid total time, unpauses,
 * clears notification flags, and reinitializes precision timer.
 */
void ResetTimer(void) {
    if (CLOCK_COUNT_UP) {
        countup_elapsed_time = 0;
    } else {
        countdown_elapsed_time = 0;
        if (CLOCK_TOTAL_TIME <= 0) {
            CLOCK_TOTAL_TIME = DEFAULT_FALLBACK_TIME;
        }
    }
    
    CLOCK_IS_PAUSED = FALSE;
    countdown_message_shown = FALSE;
    countup_message_shown = FALSE;
    
    InitializeHighPrecisionTimer();
    ResetMillisecondAccumulator();
}

/**
 * @brief Toggle timer between paused and running states
 * 
 * @details Manages pause state transitions, freezing/resuming display
 * and reinitializing timing to prevent time jumps.
 */
void TogglePauseTimer(void) {
    BOOL was_paused = CLOCK_IS_PAUSED;
    CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
    
    if (CLOCK_IS_PAUSED && !was_paused) {
        /* Just paused: freeze millisecond display */
        PauseTimerMilliseconds();
    } else if (!CLOCK_IS_PAUSED && was_paused) {
        /* Just resumed: reset timing baseline */
        InitializeHighPrecisionTimer();
        ResetMillisecondAccumulator();
    }
}
