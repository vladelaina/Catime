#include "../include/timer.h"
#include "../include/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <shellapi.h>

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

LARGE_INTEGER timer_frequency;
LARGE_INTEGER timer_last_count;
BOOL high_precision_timer_initialized = FALSE;

BOOL countdown_message_shown = FALSE;
BOOL countup_message_shown = FALSE;
int pomodoro_work_cycles = 0;

TimeoutActionType CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
char CLOCK_TIMEOUT_TEXT[50] = "";
char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH] = "";

int POMODORO_WORK_TIME = 25 * 60;
int POMODORO_SHORT_BREAK = 5 * 60;
int POMODORO_LONG_BREAK = 15 * 60;
int POMODORO_LOOP_COUNT = 1;

int time_options[MAX_TIME_OPTIONS];
int time_options_count = 0;

int last_displayed_second = -1;

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
    
    timer_last_count = current_count;
    
    return elapsed;
}

void UpdateElapsedTime(void) {
    if (CLOCK_IS_PAUSED) {
        return;
    }
    
    double elapsed_ms = GetElapsedMilliseconds();
    
    if (CLOCK_COUNT_UP) {
        countup_elapsed_time += (int)(elapsed_ms / 1000.0);
    } else {
        countdown_elapsed_time += (int)(elapsed_ms / 1000.0);
        
        if (countdown_elapsed_time > CLOCK_TOTAL_TIME) {
            countdown_elapsed_time = CLOCK_TOTAL_TIME;
        }
    }
}

void FormatTime(int remaining_time, char* time_text) {
    if (CLOCK_SHOW_CURRENT_TIME) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        
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

    UpdateElapsedTime();
    
    int remaining = CLOCK_TOTAL_TIME - countdown_elapsed_time;
    if (remaining <= 0) {
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

int ParseInput(const char* input, int* total_seconds) {
    if (!isValidInput(input)) return 0;

    int total = 0;
    char input_copy[256];
    strncpy(input_copy, input, sizeof(input_copy)-1);
    input_copy[sizeof(input_copy)-1] = '\0';

    int len = strlen(input_copy);
    if (len > 0 && (input_copy[len-1] == 't' || input_copy[len-1] == 'T')) {
        input_copy[len-1] = '\0';
        
        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);
        
        struct tm tm_target = *tm_now;
        
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
        
        if (target_time <= now) {
            tm_target.tm_mday += 1;
            target_time = mktime(&tm_target);
        }
        
        total = (int)difftime(target_time, now);
    } else {
        BOOL hasUnits = FALSE;
        for (int i = 0; input_copy[i]; i++) {
            char c = tolower((unsigned char)input_copy[i]);
            if (c == 'h' || c == 'm' || c == 's') {
                hasUnits = TRUE;
                break;
            }
        }
        
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

    *total_seconds = total;
    if (*total_seconds <= 0) return 0;

    if (*total_seconds > INT_MAX) {
        return 0;
    }

    return 1;
}

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
        } else if (i == len - 1 && (input[i] == 'h' || input[i] == 'm' || input[i] == 's' || 
                                   input[i] == 't' || input[i] == 'T' || 
                                   input[i] == 'H' || input[i] == 'M' || input[i] == 'S')) {
        } else {
            return 0;
        }
    }

    if (digitCount == 0) {
        return 0;
    }

    return 1;
}

void ResetTimer(void) {
    if (CLOCK_COUNT_UP) {
        countup_elapsed_time = 0;
    } else {
        countdown_elapsed_time = 0;
        
        if (CLOCK_TOTAL_TIME <= 0) {
            CLOCK_TOTAL_TIME = 60;
        }
    }
    
    CLOCK_IS_PAUSED = FALSE;
    
    countdown_message_shown = FALSE;
    countup_message_shown = FALSE;
    
    InitializeHighPrecisionTimer();
}

void TogglePauseTimer(void) {
    CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
    
    if (!CLOCK_IS_PAUSED) {
        InitializeHighPrecisionTimer();
    }
}

void WriteConfigDefaultStartTime(int seconds) {
    char config_path[MAX_PATH];
    
    GetConfigPath(config_path, MAX_PATH);
    
    WriteIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", seconds, config_path);
}