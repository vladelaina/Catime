/**
 * @file timer.c
 * @brief Core timer functionality with flexible input parsing and high-precision timing
 * Handles time display formatting, countdown/countup modes, and system clock integration
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
#include <shellapi.h>

/** @brief Timer state control flags */
BOOL CLOCK_IS_PAUSED = FALSE;
BOOL CLOCK_SHOW_CURRENT_TIME = FALSE;
BOOL CLOCK_USE_24HOUR = TRUE;
BOOL CLOCK_SHOW_SECONDS = TRUE;
BOOL CLOCK_COUNT_UP = FALSE;
char CLOCK_STARTUP_MODE[20] = "COUNTDOWN";

/** @brief Timer duration and elapsed time tracking */
int CLOCK_TOTAL_TIME = 0;
int countdown_elapsed_time = 0;
int countup_elapsed_time = 0;
time_t CLOCK_LAST_TIME_UPDATE = 0;

/** @brief High-precision timer state for smooth updates */
LARGE_INTEGER timer_frequency;
LARGE_INTEGER timer_last_count;
BOOL high_precision_timer_initialized = FALSE;

/** @brief Notification state tracking to prevent duplicates */
BOOL countdown_message_shown = FALSE;
BOOL countup_message_shown = FALSE;
int pomodoro_work_cycles = 0;

/** @brief Timeout action configuration */
TimeoutActionType CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
char CLOCK_TIMEOUT_TEXT[50] = "";
char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH] = "";

/** @brief Pomodoro technique default time intervals (in seconds) */
int POMODORO_WORK_TIME = 25 * 60;      /**< 25 minutes work session */
int POMODORO_SHORT_BREAK = 5 * 60;     /**< 5 minutes short break */
int POMODORO_LONG_BREAK = 15 * 60;     /**< 15 minutes long break */
int POMODORO_LOOP_COUNT = 1;

/** @brief User-defined time preset options */
int time_options[MAX_TIME_OPTIONS];
int time_options_count = 0;

/** @brief Cache for smooth second display updates */
int last_displayed_second = -1;

/**
 * @brief Initialize Windows high-precision performance counter for smooth timing
 * @return TRUE if initialization succeeded, FALSE on hardware failure
 * Uses QueryPerformanceFrequency/Counter for sub-millisecond accuracy
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
 * @brief Calculate elapsed milliseconds since last call using performance counter
 * @return Elapsed time in milliseconds as floating-point value
 * Automatically initializes timer on first call
 */
double GetElapsedMilliseconds(void) {
    if (!high_precision_timer_initialized) {
        if (!InitializeHighPrecisionTimer()) {
            return 0.0;
        }
    }
    
    LARGE_INTEGER current_count;
    if (!QueryPerformanceCounter(&current_count)) {
        return 0.0;
    }
    
    double elapsed = (double)(current_count.QuadPart - timer_last_count.QuadPart) * 1000.0 / (double)timer_frequency.QuadPart;
    
    /** Update baseline for next calculation */
    timer_last_count = current_count;
    
    return elapsed;
}

/**
 * @brief Update elapsed time counters using high-precision timing
 * Respects pause state and enforces countdown limits
 */
void UpdateElapsedTime(void) {
    if (CLOCK_IS_PAUSED) {
        return;
    }
    
    double elapsed_ms = GetElapsedMilliseconds();
    
    if (CLOCK_COUNT_UP) {
        countup_elapsed_time += (int)(elapsed_ms / 1000.0);
    } else {
        countdown_elapsed_time += (int)(elapsed_ms / 1000.0);
        
        /** Clamp countdown to total time limit */
        if (countdown_elapsed_time > CLOCK_TOTAL_TIME) {
            countdown_elapsed_time = CLOCK_TOTAL_TIME;
        }
    }
}

/**
 * @brief Format time for display based on current mode (clock/countdown/countup)
 * @param remaining_time Unused parameter (kept for API compatibility)
 * @param time_text Output buffer for formatted time string
 * Handles system clock display, countdown, and countup modes with smart formatting
 */
void FormatTime(int remaining_time, char* time_text) {
    /** System clock mode: display current time with smooth second updates */
    if (CLOCK_SHOW_CURRENT_TIME) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        /** Smooth second transition detection to prevent jittery updates */
        if (last_displayed_second != -1) {
            if (st.wSecond != (last_displayed_second + 1) % 60 && 
                !(last_displayed_second == 59 && st.wSecond == 0)) {
                if (st.wSecond != last_displayed_second) {
                    last_displayed_second = st.wSecond;
                }
            } else {
                last_displayed_second = st.wSecond;
            }
        } else {
            last_displayed_second = st.wSecond;
        }
        
        int hour = st.wHour;
        
        /** Convert to 12-hour format if needed */
        if (!CLOCK_USE_24HOUR) {
            if (hour == 0) {
                hour = 12;          /**< Midnight becomes 12 AM */
            } else if (hour > 12) {
                hour -= 12;         /**< PM hours */
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

    /** Count-up mode: stopwatch with adaptive formatting */
    if (CLOCK_COUNT_UP) {
        UpdateElapsedTime();
        
        int hours = countup_elapsed_time / 3600;
        int minutes = (countup_elapsed_time % 3600) / 60;
        int seconds = countup_elapsed_time % 60;

        /** Progressive display complexity based on elapsed time */
        if (hours > 0) {
            sprintf(time_text, "%d:%02d:%02d", hours, minutes, seconds);
        } else if (minutes > 0) {
            sprintf(time_text, "    %d:%02d", minutes, seconds);
        } else {
            sprintf(time_text, "        %d", seconds);
        }
        return;
    }

    /** Countdown mode: remaining time with visual alignment */
    UpdateElapsedTime();
    
    int remaining = CLOCK_TOTAL_TIME - countdown_elapsed_time;
    if (remaining <= 0) {
        sprintf(time_text, "    0:00");
        return;
    }

    int hours = remaining / 3600;
    int minutes = (remaining % 3600) / 60;
    int seconds = remaining % 60;

    /** Format with consistent visual alignment using spaces */
    if (hours > 0) {
        sprintf(time_text, "%d:%02d:%02d", hours, minutes, seconds);
    } else if (minutes > 0) {
        if (minutes >= 10) {
            sprintf(time_text, "    %d:%02d", minutes, seconds);
        } else {
            sprintf(time_text, "    %d:%02d", minutes, seconds);
        }
    } else {
        /** Right-align single seconds for visual consistency */
        if (seconds < 10) {
            sprintf(time_text, "          %d", seconds);
        } else {
            sprintf(time_text, "        %d", seconds);
        }
    }
}

/**
 * @brief Parse flexible time input formats into seconds
 * @param input Time input string (various formats supported)
 * @param total_seconds Output parameter for parsed time in seconds
 * @return 1 if parsing succeeded, 0 if invalid input
 * Supports: duration (5m 30s), time-to (14:30t), and numeric formats
 */
int ParseInput(const char* input, int* total_seconds) {
    if (!isValidInput(input)) return 0;

    int total = 0;
    char input_copy[256];
    strncpy(input_copy, input, sizeof(input_copy)-1);
    input_copy[sizeof(input_copy)-1] = '\0';

    int len = strlen(input_copy);
    /** Time-to mode: countdown to specific time (suffix 't' or 'T') */
    if (len > 0 && (input_copy[len-1] == 't' || input_copy[len-1] == 'T')) {
        input_copy[len-1] = '\0';
        
        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);
        
        struct tm tm_target = *tm_now;
        
        /** Parse target time components */
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
        
        /** Build target time structure */
        if (hour >= 0) {
            tm_target.tm_hour = hour;
            
            if (minute < 0) {
                tm_target.tm_min = 0;
                tm_target.tm_sec = 0;
            } else {
                tm_target.tm_min = minute;
                
                if (second < 0) {
                    tm_target.tm_sec = 0;
                } else {
                    tm_target.tm_sec = second;
                }
            }
        }
        
        time_t target_time = mktime(&tm_target);
        
        /** If target time is in the past, assume next day */
        if (target_time <= now) {
            tm_target.tm_mday += 1;
            target_time = mktime(&tm_target);
        }
        
        total = (int)difftime(target_time, now);
    } else {
        /** Duration mode: parse explicit units (5h 30m 10s) or numeric formats */
        BOOL hasUnits = FALSE;
        for (int i = 0; input_copy[i]; i++) {
            char c = tolower((unsigned char)input_copy[i]);
            if (c == 'h' || c == 'm' || c == 's') {
                hasUnits = TRUE;
                break;
            }
        }
        
        /** Unit-based parsing (e.g., "5h 30m", "25m", "90s") */
        if (hasUnits) {
            char* parts[10] = {0};
            int part_count = 0;
            
            char* token = strtok(input_copy, " ");
            while (token && part_count < 10) {
                parts[part_count++] = token;
                token = strtok(NULL, " ");
            }
            
            for (int i = 0; i < part_count; i++) {
                char* part = parts[i];
                int part_len = strlen(part);
                BOOL has_unit = FALSE;
                
                for (int j = 0; j < part_len; j++) {
                    char c = tolower((unsigned char)part[j]);
                    if (c == 'h' || c == 'm' || c == 's') {
                        has_unit = TRUE;
                        break;
                    }
                }
                
                if (has_unit) {
                    char unit = tolower((unsigned char)part[part_len-1]);
                    part[part_len-1] = '\0';
                    int value = atoi(part);
                    
                    switch (unit) {
                        case 'h': total += value * 3600; break;
                        case 'm': total += value * 60; break;
                        case 's': total += value; break;
                    }
                } else if (i < part_count - 1 && 
                          strlen(parts[i+1]) > 0 && 
                          tolower((unsigned char)parts[i+1][strlen(parts[i+1])-1]) == 'h') {
                    total += atoi(part) * 3600;
                } else if (i < part_count - 1 && 
                          strlen(parts[i+1]) > 0 && 
                          tolower((unsigned char)parts[i+1][strlen(parts[i+1])-1]) == 'm') {
                    total += atoi(part) * 3600;
                } else {
                    if (part_count == 2) {
                        if (i == 0) total += atoi(part) * 60;
                        else total += atoi(part);
                    } else if (part_count == 3) {
                        if (i == 0) total += atoi(part) * 3600;
                        else if (i == 1) total += atoi(part) * 60;
                        else total += atoi(part);
                    } else {
                        total += atoi(part) * 60;
                    }
                }
            }
        } else {
            char* parts[3] = {0};
            int part_count = 0;
            
            char* token = strtok(input_copy, " ");
            while (token && part_count < 3) {
                parts[part_count++] = token;
                token = strtok(NULL, " ");
            }
            
            if (part_count == 1) {
                total = atoi(parts[0]) * 60;
            } else if (part_count == 2) {
                total = atoi(parts[0]) * 60 + atoi(parts[1]);
            } else if (part_count == 3) {
                total = atoi(parts[0]) * 3600 + atoi(parts[1]) * 60 + atoi(parts[2]);
            }
        }
    }

    /** Finalize and validate parsed result */
    *total_seconds = total;
    if (*total_seconds <= 0) return 0;

    if (*total_seconds > INT_MAX) {
        return 0;
    }

    return 1;
}

/**
 * @brief Validate input string for timer parsing
 * @param input Input string to validate
 * @return 1 if input is valid for parsing, 0 otherwise
 * Accepts digits, spaces, and trailing unit letters (h/m/s/t)
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
            /** Spaces are allowed for separation */
        } else if (i == len - 1 && (input[i] == 'h' || input[i] == 'm' || input[i] == 's' || 
                                   input[i] == 't' || input[i] == 'T' || 
                                   input[i] == 'H' || input[i] == 'M' || input[i] == 'S')) {
            /** Unit suffixes allowed only at end */
        } else {
            return 0;
        }
    }

    /** Must contain at least one digit */
    if (digitCount == 0) {
        return 0;
    }

    return 1;
}

/**
 * @brief Reset timer to initial state based on current mode
 * Clears elapsed time, ensures valid total time, and reinitializes precision timer
 */
void ResetTimer(void) {
    if (CLOCK_COUNT_UP) {
        countup_elapsed_time = 0;
    } else {
        countdown_elapsed_time = 0;
        
        /** Ensure valid countdown duration with 1-minute fallback */
        if (CLOCK_TOTAL_TIME <= 0) {
            CLOCK_TOTAL_TIME = 60;
        }
    }
    
    CLOCK_IS_PAUSED = FALSE;
    
    /** Clear notification flags to allow new alerts */
    countdown_message_shown = FALSE;
    countup_message_shown = FALSE;
    
    InitializeHighPrecisionTimer();
    ResetMillisecondAccumulator();  /** Reset millisecond timing on reset */
}

/**
 * @brief Toggle timer pause state
 * Reinitializes precision timer baseline when resuming for accurate timing
 */
void TogglePauseTimer(void) {
    BOOL was_paused = CLOCK_IS_PAUSED;
    CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
    
    if (CLOCK_IS_PAUSED && !was_paused) {
        /** Just paused: save current milliseconds for frozen display */
        PauseTimerMilliseconds();
    } else if (!CLOCK_IS_PAUSED && was_paused) {
        /** Just resumed: reset timing baseline to prevent time jumps */
        InitializeHighPrecisionTimer();
        ResetMillisecondAccumulator();  /** Reset millisecond timing on resume */
    }
}

/**
 * @brief Save default start time to configuration file
 * @param seconds Default timer duration in seconds
 * Updates persistent configuration for application startup
 */
void WriteConfigDefaultStartTime(int seconds) {
    char config_path[MAX_PATH];
    
    GetConfigPath(config_path, MAX_PATH);
    
    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", seconds, config_path);
}