/**
 * @file timer.c
 * @brief Implementation of core timer functionality
 * 
 * This file contains the implementation of the core timer logic, including time format conversion, 
 * input parsing, configuration saving, and other functionalities,
 * while maintaining various timer states and configuration parameters.
 */

#include "../include/timer.h"
#include "../include/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <shellapi.h>

/** @name Timer status flags
 *  @{ */
BOOL CLOCK_IS_PAUSED = FALSE;          ///< Timer pause status flag
BOOL CLOCK_SHOW_CURRENT_TIME = FALSE;  ///< Show current time mode flag
BOOL CLOCK_USE_24HOUR = TRUE;          ///< Use 24-hour format flag
BOOL CLOCK_SHOW_SECONDS = TRUE;        ///< Show seconds flag
BOOL CLOCK_COUNT_UP = FALSE;           ///< Countdown/count-up mode flag
char CLOCK_STARTUP_MODE[20] = "COUNTDOWN"; ///< Startup mode (countdown/count-up)
/** @} */

/** @name Timer time parameters 
 *  @{ */
int CLOCK_TOTAL_TIME = 0;              ///< Total timer time (seconds)
int countdown_elapsed_time = 0;        ///< Countdown elapsed time (seconds)
int countup_elapsed_time = 0;          ///< Count-up accumulated time (seconds)
time_t CLOCK_LAST_TIME_UPDATE = 0;     ///< Last update timestamp

// High-precision timer related variables
LARGE_INTEGER timer_frequency;         ///< High-precision timer frequency
LARGE_INTEGER timer_last_count;        ///< Last timing point
BOOL high_precision_timer_initialized = FALSE; ///< High-precision timer initialization flag
/** @} */

/** @name Message status flags
 *  @{ */
BOOL countdown_message_shown = FALSE;  ///< Countdown completion message display status
BOOL countup_message_shown = FALSE;    ///< Count-up completion message display status
int pomodoro_work_cycles = 0;          ///< Pomodoro work cycle count
/** @} */

/** @name Timeout action configuration
 *  @{ */
TimeoutActionType CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE; ///< Timeout action type
char CLOCK_TIMEOUT_TEXT[50] = "";      ///< Timeout display text
char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH] = ""; ///< Timeout executable file path
/** @} */

/** @name Pomodoro time settings
 *  @{ */
int POMODORO_WORK_TIME = 25 * 60;    ///< Pomodoro work time (25 minutes)
int POMODORO_SHORT_BREAK = 5 * 60;   ///< Pomodoro short break time (5 minutes)
int POMODORO_LONG_BREAK = 15 * 60;   ///< Pomodoro long break time (15 minutes)
int POMODORO_LOOP_COUNT = 1;         ///< Pomodoro loop count (default 1 time)
/** @} */

/** @name Preset time options
 *  @{ */
int time_options[MAX_TIME_OPTIONS];    ///< Preset time options array
int time_options_count = 0;            ///< Number of valid preset times
/** @} */

/** Last displayed time (seconds), used to prevent time display jumping phenomenon */
int last_displayed_second = -1;

/**
 * @brief Initialize high-precision timer
 * 
 * Get system timer frequency and record initial timing point
 * @return BOOL Whether initialization was successful
 */
BOOL InitializeHighPrecisionTimer(void) {
    if (!QueryPerformanceFrequency(&timer_frequency)) {
        return FALSE;  // System does not support high-precision timer
    }
    
    if (!QueryPerformanceCounter(&timer_last_count)) {
        return FALSE;  // Failed to get current count
    }
    
    high_precision_timer_initialized = TRUE;
    return TRUE;
}

/**
 * @brief Calculate milliseconds elapsed since last call
 * 
 * Use high-precision timer to calculate exact time interval
 * @return double Elapsed milliseconds
 */
double GetElapsedMilliseconds(void) {
    if (!high_precision_timer_initialized) {
        if (!InitializeHighPrecisionTimer()) {
            return 0.0;  // Initialization failed, return 0
        }
    }
    
    LARGE_INTEGER current_count;
    if (!QueryPerformanceCounter(&current_count)) {
        return 0.0;  // Failed to get current count
    }
    
    // Calculate time difference (convert to milliseconds)
    double elapsed = (double)(current_count.QuadPart - timer_last_count.QuadPart) * 1000.0 / (double)timer_frequency.QuadPart;
    
    // Update last timing point
    timer_last_count = current_count;
    
    return elapsed;
}

/**
 * @brief Update elapsed time for countdown/count-up
 * 
 * Use high-precision timer to calculate time elapsed since last call, and update
 * countup_elapsed_time (count-up mode) or countdown_elapsed_time (countdown mode) accordingly.
 * No updates are made in pause state.
 */
void UpdateElapsedTime(void) {
    if (CLOCK_IS_PAUSED) {
        return;  // Do not update in pause state
    }
    
    double elapsed_ms = GetElapsedMilliseconds();
    
    if (CLOCK_COUNT_UP) {
        // Count-up mode
        countup_elapsed_time += (int)(elapsed_ms / 1000.0);
    } else {
        // Countdown mode
        countdown_elapsed_time += (int)(elapsed_ms / 1000.0);
        
        // Ensure does not exceed total time
        if (countdown_elapsed_time > CLOCK_TOTAL_TIME) {
            countdown_elapsed_time = CLOCK_TOTAL_TIME;
        }
    }
}

/**
 * @brief Format display time
 * @param remaining_time Remaining time (seconds)
 * @param[out] time_text Formatted time string output buffer
 * 
 * Format time as a string according to current configuration (12/24 hour format, whether to show seconds, countdown/count-up mode).
 * Supports three display modes: current system time, countdown remaining time, count-up accumulated time.
 */
void FormatTime(int remaining_time, char* time_text) {
    if (CLOCK_SHOW_CURRENT_TIME) {
        // Get local time
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        // Check time continuity, prevent second jumping display
        if (last_displayed_second != -1) {
            // If not consecutive seconds, and not a cross-minute situation
            if (st.wSecond != (last_displayed_second + 1) % 60 && 
                !(last_displayed_second == 59 && st.wSecond == 0)) {
                // Relax conditions, allow larger differences to sync, ensure not lagging behind for long
                if (st.wSecond != last_displayed_second) {
                    // Directly use system time seconds
                    last_displayed_second = st.wSecond;
                }
            } else {
                // Time is consecutive, normal update
                last_displayed_second = st.wSecond;
            }
        } else {
            // First display, initialize record
            last_displayed_second = st.wSecond;
        }
        
        int hour = st.wHour;
        
        if (!CLOCK_USE_24HOUR) {
            if (hour == 0) {
                hour = 12;
            } else if (hour > 12) {
                hour -= 12;
            }
        }

        if (CLOCK_SHOW_SECONDS) {
            sprintf(time_text, "%d:%02d:%02d", 
                    hour, st.wMinute, last_displayed_second);
        } else {
            sprintf(time_text, "%d:%02d", 
                    hour, st.wMinute);
        }
        return;
    }

    if (CLOCK_COUNT_UP) {
        // Update elapsed time before display
        UpdateElapsedTime();
        
        int hours = countup_elapsed_time / 3600;
        int minutes = (countup_elapsed_time % 3600) / 60;
        int seconds = countup_elapsed_time % 60;

        if (hours > 0) {
            sprintf(time_text, "%d:%02d:%02d", hours, minutes, seconds);
        } else if (minutes > 0) {
            sprintf(time_text, "    %d:%02d", minutes, seconds);
        } else {
            sprintf(time_text, "        %d", seconds);
        }
        return;
    }

    // Update elapsed time before display
    UpdateElapsedTime();
    
    int remaining = CLOCK_TOTAL_TIME - countdown_elapsed_time;
    if (remaining <= 0) {
        // Do not return empty string, show 0:00 instead
        sprintf(time_text, "    0:00");
        return;
    }

    int hours = remaining / 3600;
    int minutes = (remaining % 3600) / 60;
    int seconds = remaining % 60;

    if (hours > 0) {
        sprintf(time_text, "%d:%02d:%02d", hours, minutes, seconds);
    } else if (minutes > 0) {
        if (minutes >= 10) {
            sprintf(time_text, "    %d:%02d", minutes, seconds);
        } else {
            sprintf(time_text, "    %d:%02d", minutes, seconds);
        }
    } else {
        if (seconds < 10) {
            sprintf(time_text, "          %d", seconds);
        } else {
            sprintf(time_text, "        %d", seconds);
        }
    }
}

/**
 * @brief Parse user input time string
 * @param input User input time string
 * @param[out] total_seconds Total seconds parsed
 * @return int Returns 1 if parsing successful, 0 if failed
 * 
 * Supports multiple input formats:
 * - Single number (default minutes): "25" → 25 minutes
 * - With units: "1h30m" → 1 hour 30 minutes
 * - Two-segment format: "25 3" → 25 minutes 3 seconds
 * - Three-segment format: "1 30 15" → 1 hour 30 minutes 15 seconds
 * - Mixed format: "25 30m" → 25 hours 30 minutes
 * - Target time: "17 30t" or "17 30T" → Countdown to 17:30
 */

// Detailed explanation of parsing logic and boundary handling
/**
 * @brief Parse user input time string
 * @param input User input time string
 * @param[out] total_seconds Total seconds parsed
 * @return int Returns 1 if parsing successful, 0 if failed
 * 
 * Supports multiple input formats:
 * - Single number (default minutes): "25" → 25 minutes
 * - With units: "1h30m" → 1 hour 30 minutes
 * - Two-segment format: "25 3" → 25 minutes 3 seconds
 * - Three-segment format: "1 30 15" → 1 hour 30 minutes 15 seconds
 * - Mixed format: "25 30m" → 25 hours 30 minutes
 * - Target time: "17 30t" or "17 30T" → Countdown to 17:30
 * 
 * Parsing process:
 * 1. First check the validity of the input
 * 2. Detect if it's a target time format (ending with 't' or 'T')
 *    - If so, calculate seconds from current to target time, if time has passed set to same time tomorrow
 * 3. Otherwise, check if it contains unit identifiers (h/m/s)
 *    - If it does, process according to units
 *    - If not, decide processing method based on number of space-separated numbers
 * 
 * Boundary handling:
 * - Invalid input returns 0
 * - Negative or zero value returns 0
 * - Value exceeding INT_MAX returns 0
 */
int ParseInput(const char* input, int* total_seconds) {
    if (!isValidInput(input)) return 0;

    int total = 0;
    char input_copy[256];
    strncpy(input_copy, input, sizeof(input_copy)-1);
    input_copy[sizeof(input_copy)-1] = '\0';

    // Check if it's a target time format (ending with 't' or 'T')
    int len = strlen(input_copy);
    if (len > 0 && (input_copy[len-1] == 't' || input_copy[len-1] == 'T')) {
        // Remove 't' or 'T' suffix
        input_copy[len-1] = '\0';
        
        // Get current time
        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);
        
        // Target time, initialize to current date
        struct tm tm_target = *tm_now;
        
        // Parse target time
        int hour = -1, minute = -1, second = -1;
        int count = 0;
        char *token = strtok(input_copy, " ");
        
        while (token && count < 3) {
            int value = atoi(token);
            if (count == 0) hour = value;
            else if (count == 1) minute = value;
            else if (count == 2) second = value;
            count++;
            token = strtok(NULL, " ");
        }
        
        // Set target time, set according to provided values, defaults to 0 if not provided
        if (hour >= 0) {
            tm_target.tm_hour = hour;
            
            // If only hour provided, set minute and second to 0
            if (minute < 0) {
                tm_target.tm_min = 0;
                tm_target.tm_sec = 0;
            } else {
                tm_target.tm_min = minute;
                
                // If second not provided, set to 0
                if (second < 0) {
                    tm_target.tm_sec = 0;
                } else {
                    tm_target.tm_sec = second;
                }
            }
        }
        
        // Calculate time difference (seconds)
        time_t target_time = mktime(&tm_target);
        
        // If target time has passed, set to same time tomorrow
        if (target_time <= now) {
            tm_target.tm_mday += 1;
            target_time = mktime(&tm_target);
        }
        
        total = (int)difftime(target_time, now);
    } else {
        // Check if it contains unit identifiers
        BOOL hasUnits = FALSE;
        for (int i = 0; input_copy[i]; i++) {
            char c = tolower((unsigned char)input_copy[i]);
            if (c == 'h' || c == 'm' || c == 's') {
                hasUnits = TRUE;
                break;
            }
        }
        
        if (hasUnits) {
            // For input with units, merge all parts with unit markings
            char* parts[10] = {0}; // Store up to 10 parts
            int part_count = 0;
            
            // Split input string
            char* token = strtok(input_copy, " ");
            while (token && part_count < 10) {
                parts[part_count++] = token;
                token = strtok(NULL, " ");
            }
            
            // Process each part
            for (int i = 0; i < part_count; i++) {
                char* part = parts[i];
                int part_len = strlen(part);
                BOOL has_unit = FALSE;
                
                // Check if this part has a unit
                for (int j = 0; j < part_len; j++) {
                    char c = tolower((unsigned char)part[j]);
                    if (c == 'h' || c == 'm' || c == 's') {
                        has_unit = TRUE;
                        break;
                    }
                }
                
                if (has_unit) {
                    // If it has a unit, process according to unit
                    char unit = tolower((unsigned char)part[part_len-1]);
                    part[part_len-1] = '\0'; // Remove unit
                    int value = atoi(part);
                    
                    switch (unit) {
                        case 'h': total += value * 3600; break;
                        case 'm': total += value * 60; break;
                        case 's': total += value; break;
                    }
                } else if (i < part_count - 1 && 
                          strlen(parts[i+1]) > 0 && 
                          tolower((unsigned char)parts[i+1][strlen(parts[i+1])-1]) == 'h') {
                    // If next item has h unit, current item is hours
                    total += atoi(part) * 3600;
                } else if (i < part_count - 1 && 
                          strlen(parts[i+1]) > 0 && 
                          tolower((unsigned char)parts[i+1][strlen(parts[i+1])-1]) == 'm') {
                    // If next item has m unit, current item is hours
                    total += atoi(part) * 3600;
                } else {
                    // Default process as two-segment or three-segment format
                    if (part_count == 2) {
                        // Two-segment format: first segment is minutes, second segment is seconds
                        if (i == 0) total += atoi(part) * 60;
                        else total += atoi(part);
                    } else if (part_count == 3) {
                        // Three-segment format: hour:minute:second
                        if (i == 0) total += atoi(part) * 3600;
                        else if (i == 1) total += atoi(part) * 60;
                        else total += atoi(part);
                    } else {
                        // Other cases treated as minutes
                        total += atoi(part) * 60;
                    }
                }
            }
        } else {
            // Processing without units
            char* parts[3] = {0}; // Store up to 3 parts (hour, minute, second)
            int part_count = 0;
            
            // Split input string
            char* token = strtok(input_copy, " ");
            while (token && part_count < 3) {
                parts[part_count++] = token;
                token = strtok(NULL, " ");
            }
            
            if (part_count == 1) {
                // Single number, calculate as minutes
                total = atoi(parts[0]) * 60;
            } else if (part_count == 2) {
                // Two numbers: minute:second
                total = atoi(parts[0]) * 60 + atoi(parts[1]);
            } else if (part_count == 3) {
                // Three numbers: hour:minute:second
                total = atoi(parts[0]) * 3600 + atoi(parts[1]) * 60 + atoi(parts[2]);
            }
        }
    }

    *total_seconds = total;
    if (*total_seconds <= 0) return 0;

    if (*total_seconds > INT_MAX) {
        return 0;
    }

    return 1;
}

/**
 * @brief Validate if input string is legal
 * @param input Input string to validate
 * @return int Returns 1 if legal, 0 if illegal
 * 
 * Valid character check rules:
 * - Only allows digits, spaces, and h/m/s/t unit identifiers at the end (case insensitive)
 * - Must contain at least one digit
 * - Maximum of two space separators
 */
int isValidInput(const char* input) {
    if (input == NULL || *input == '\0') {
        return 0;
    }

    int len = strlen(input);
    int digitCount = 0;

    for (int i = 0; i < len; i++) {
        if (isdigit(input[i])) {
            digitCount++;
        } else if (input[i] == ' ') {
            // Allow any number of spaces
        } else if (i == len - 1 && (input[i] == 'h' || input[i] == 'm' || input[i] == 's' || 
                                   input[i] == 't' || input[i] == 'T' || 
                                   input[i] == 'H' || input[i] == 'M' || input[i] == 'S')) {
            // Allow last character to be h/m/s/t or their uppercase forms
        } else {
            return 0;
        }
    }

    if (digitCount == 0) {
        return 0;
    }

    return 1;
}

/**
 * @brief Reset timer
 * 
 * Reset timer state, including pause flag, elapsed time, etc.
 */
void ResetTimer(void) {
    // Reset timing state
    if (CLOCK_COUNT_UP) {
        countup_elapsed_time = 0;
    } else {
        countdown_elapsed_time = 0;
        
        // Ensure countdown total time is not zero, zero would cause timer not to display
        if (CLOCK_TOTAL_TIME <= 0) {
            // If total time is invalid, use default value
            CLOCK_TOTAL_TIME = 60; // Default set to 1 minute
        }
    }
    
    // Cancel pause state
    CLOCK_IS_PAUSED = FALSE;
    
    // Reset message display flags
    countdown_message_shown = FALSE;
    countup_message_shown = FALSE;
    
    // Reinitialize high-precision timer
    InitializeHighPrecisionTimer();
}

/**
 * @brief Toggle timer pause state
 * 
 * Toggle timer between pause and continue states
 */
void TogglePauseTimer(void) {
    CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
    
    // If resuming from pause state, reinitialize high-precision timer
    if (!CLOCK_IS_PAUSED) {
        InitializeHighPrecisionTimer();
    }
}

/**
 * @brief Write default startup time to configuration file
 * @param seconds Default startup time (seconds)
 * 
 * Use the general path retrieval method in the configuration management module to write to INI format configuration file
 */
void WriteConfigDefaultStartTime(int seconds) {
    char config_path[MAX_PATH];
    
    // Get configuration file path
    GetConfigPath(config_path, MAX_PATH);
    
    // Write using INI format
    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", seconds, config_path);
}
