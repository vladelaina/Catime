/**
 * @file timer.c
 * @brief Multi-modal timer with high-precision tracking
 * 
 * QueryPerformanceCounter prevents drift in long-running timers.
 * Adaptive display formatting reduces visual jitter during transitions.
 */

#include "timer/timer.h"
#include "config.h"
#include "timer/timer_events.h"
#include "drawing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600
#define MINUTES_PER_HOUR 60
#define MILLISECONDS_PER_SECOND 1000.0
#define DEFAULT_FALLBACK_TIME 60

BOOL CLOCK_IS_PAUSED = FALSE;
BOOL CLOCK_SHOW_CURRENT_TIME = FALSE;
BOOL CLOCK_USE_24HOUR = TRUE;
BOOL CLOCK_SHOW_SECONDS = TRUE;
BOOL CLOCK_COUNT_UP = FALSE;
char CLOCK_STARTUP_MODE[20] = "COUNTDOWN";

int CLOCK_TOTAL_TIME = 0;
int countdown_elapsed_time = 0;
int countup_elapsed_time = 0;
time_t CLOCK_LAST_TIME_UPDATE = 0;
int last_displayed_second = -1;

static LARGE_INTEGER timer_frequency = {0};
static LARGE_INTEGER timer_last_count = {0};
static BOOL high_precision_timer_initialized = FALSE;

BOOL countdown_message_shown = FALSE;
BOOL countup_message_shown = FALSE;
int pomodoro_work_cycles = 0;

TimeoutActionType CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
char CLOCK_TIMEOUT_TEXT[50] = "";
char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH] = "";

int time_options[MAX_TIME_OPTIONS] = {0};
int time_options_count = 0;

/** Reset QPC baseline to prevent time jumps after pause/resume */
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

/** Delta-based measurement, auto-updates baseline to avoid accumulation errors */
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

/** Accumulate elapsed time, clamp countdown to prevent negative display */
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

/** Leading spaces stabilize width when hours/minutes disappear during countdown */
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

static int ConvertTo12HourFormat(int hour24) {
    if (hour24 == 0) return 12;
    if (hour24 > 12) return hour24 - 12;
    return hour24;
}

/** Cache displayed second to filter jitter from misaligned queries and refresh cycles */
static void FormatSystemClock(char* time_text) {
    SYSTEMTIME st;
    GetLocalTime(&st);
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

static void FormatCountUpTime(char* time_text) {
    UpdateElapsedTime();
    
    int hours = countup_elapsed_time / SECONDS_PER_HOUR;
    int minutes = (countup_elapsed_time % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
    int seconds = countup_elapsed_time % SECONDS_PER_MINUTE;
    
    FormatTimeComponents(hours, minutes, seconds, time_text, 64);
}

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

/** Dispatch to formatter based on current mode */
void FormatTime(int remaining_time, char* time_text) {
    (void)remaining_time;
    
    if (CLOCK_SHOW_CURRENT_TIME) {
        FormatSystemClock(time_text);
    } else if (CLOCK_COUNT_UP) {
        FormatCountUpTime(time_text);
    } else {
        FormatCountdownTime(time_text);
    }
}

/** Reject empty or pure non-numeric input to prevent parser errors */
int isValidInput(const char* input) {
    if (!input || !*input) return 0;
    
    int len = strlen(input);
    int digit_count = 0;
    
    for (int i = 0; i < len; i++) {
        if (isdigit(input[i])) {
            digit_count++;
        } else if (input[i] == ' ') {
            continue;
        } else if (i == len - 1) {
            char c = tolower((unsigned char)input[i]);
            if (c != 'h' && c != 'm' && c != 's' && c != 't') {
                return 0;
            }
        } else {
            return 0;
        }
    }
    
    return digit_count > 0;
}

typedef struct {
    char unit;
    int multiplier;
} TimeUnit;

static const TimeUnit TIME_UNITS[] = {
    {'h', SECONDS_PER_HOUR},
    {'m', SECONDS_PER_MINUTE},
    {'s', 1}
};

/** Parse "1h 30m 15s" or unitless "1 30 15" (infer by position if no suffix) */
static int ParseDurationWithUnits(char* input) {
    int total = 0;
    char* parts[10];
    int part_count = 0;
    char* token = strtok(input, " ");
    while (token && part_count < 10) {
        parts[part_count++] = token;
        token = strtok(NULL, " ");
    }
    
    BOOL has_any_unit = FALSE;
    for (int i = 0; i < part_count; i++) {
        int part_len = strlen(parts[i]);
        char unit = tolower((unsigned char)parts[i][part_len - 1]);
        if (unit == 'h' || unit == 'm' || unit == 's') {
            has_any_unit = TRUE;
            break;
        }
    }
    
    int unitless_index = 0;
    for (int i = 0; i < part_count; i++) {
        char* part = parts[i];
        int part_len = strlen(part);
        char unit = tolower((unsigned char)part[part_len - 1]);
        BOOL has_unit = (unit == 'h' || unit == 'm' || unit == 's');
        
        if (has_unit) {
            part[part_len - 1] = '\0';
            int value = atoi(part);
            for (size_t j = 0; j < sizeof(TIME_UNITS) / sizeof(TIME_UNITS[0]); j++) {
                if (TIME_UNITS[j].unit == unit) {
                    total += value * TIME_UNITS[j].multiplier;
                    break;
                }
            }
        } else {
            int value = atoi(part);
            if (has_any_unit) {
                int multipliers[] = {SECONDS_PER_HOUR, SECONDS_PER_MINUTE, 1};
                total += value * multipliers[unitless_index];
                unitless_index++;
            } else {
                if (part_count == 2) {
                    total += (i == 0) ? value * SECONDS_PER_MINUTE : value;
                } else if (part_count == 3) {
                    int multipliers[] = {SECONDS_PER_HOUR, SECONDS_PER_MINUTE, 1};
                    total += value * multipliers[i];
                } else {
                    total += value * SECONDS_PER_MINUTE;
                }
            }
        }
    }
    
    return total;
}

/** Infer units from part count: 1→minutes, 2→MM:SS, 3→HH:MM:SS */
static int ParseNumericShorthand(char* input) {
    char* parts[3];
    int part_count = 0;
    
    char* token = strtok(input, " ");
    while (token && part_count < 3) {
        parts[part_count++] = token;
        token = strtok(NULL, " ");
    }
    
    if (part_count == 1) {
        return atoi(parts[0]) * SECONDS_PER_MINUTE;
    } else if (part_count == 2) {
        return atoi(parts[0]) * SECONDS_PER_MINUTE + atoi(parts[1]);
    } else if (part_count == 3) {
        return atoi(parts[0]) * SECONDS_PER_HOUR + atoi(parts[1]) * SECONDS_PER_MINUTE + atoi(parts[2]);
    }
    
    return 0;
}

/** Parse "14 30t" → countdown to target time (assumes next day if in past) */
static int ParseAbsoluteTime(char* input) {
    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    struct tm tm_target = *tm_now;
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
    
    if (hour < 0 || hour > 23) {
        return 0;
    }
    if (minute < -1 || minute > 59) {
        return 0;
    }
    if (second < -1 || second > 59) {
        return 0;
    }
    
    tm_target.tm_hour = hour;
    tm_target.tm_min = (minute >= 0) ? minute : 0;
    tm_target.tm_sec = (second >= 0) ? second : 0;
    
    time_t target_time = mktime(&tm_target);
    if (target_time <= now) {
        tm_target.tm_mday += 1;
        target_time = mktime(&tm_target);
    }
    
    return (int)difftime(target_time, now);
}

static BOOL HasTimeUnits(const char* input) {
    for (const char* p = input; *p; p++) {
        char c = tolower((unsigned char)*p);
        if (c == 'h' || c == 'm' || c == 's') {
            return TRUE;
        }
    }
    return FALSE;
}

/** Parse: "14 30t" (absolute), "1h 30m" (units), "25" or "1 30" (shorthand) */
int ParseInput(const char* input, int* total_seconds) {
    if (!isValidInput(input)) return 0;
    
    char input_copy[256];
    strncpy(input_copy, input, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';
    
    int len = strlen(input_copy);
    int result = 0;
    
    if (len > 0 && (input_copy[len - 1] == 't' || input_copy[len - 1] == 'T')) {
        input_copy[len - 1] = '\0';
        result = ParseAbsoluteTime(input_copy);
    } else if (HasTimeUnits(input_copy)) {
        result = ParseDurationWithUnits(input_copy);
    } else {
        result = ParseNumericShorthand(input_copy);
    }
    
    if (result <= 0 || result > INT_MAX) {
        return 0;
    }
    
    *total_seconds = result;
    return 1;
}

void WriteConfigDefaultStartTime(int seconds) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", seconds, config_path);
}

/** Fallback to DEFAULT_FALLBACK_TIME if countdown has invalid total time */
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

/** Reinitialize timing baseline on resume to prevent time jumps */
void TogglePauseTimer(void) {
    BOOL was_paused = CLOCK_IS_PAUSED;
    CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
    
    if (CLOCK_IS_PAUSED && !was_paused) {
        PauseTimerMilliseconds();
    } else if (!CLOCK_IS_PAUSED && was_paused) {
        InitializeHighPrecisionTimer();
        ResetMillisecondAccumulator();
    }
}
