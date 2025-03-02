#include "../include/timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 计时器状态
BOOL CLOCK_IS_PAUSED = FALSE;
BOOL CLOCK_SHOW_CURRENT_TIME = FALSE;
BOOL CLOCK_USE_24HOUR = TRUE;
BOOL CLOCK_SHOW_SECONDS = TRUE;
BOOL CLOCK_COUNT_UP = FALSE;
char CLOCK_STARTUP_MODE[20] = "COUNTDOWN";

// 计时器时间
extern int CLOCK_TOTAL_TIME;
int countdown_elapsed_time = 0;
int countup_elapsed_time = 0;
time_t CLOCK_LAST_TIME_UPDATE = 0;

// 消息状态
BOOL countdown_message_shown = FALSE;
BOOL countup_message_shown = FALSE;

void FormatTime(int remaining_time, char* time_text) {
    if (CLOCK_SHOW_CURRENT_TIME) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        int hour = tm_info->tm_hour;
        
        if (!CLOCK_USE_24HOUR) {
            if (hour == 0) {
                hour = 12;
            } else if (hour > 12) {
                hour -= 12;
            }
        }

        if (CLOCK_SHOW_SECONDS) {
            sprintf(time_text, "%d:%02d:%02d", 
                    hour, tm_info->tm_min, tm_info->tm_sec);
        } else {
            sprintf(time_text, "%d:%02d", 
                    hour, tm_info->tm_min);
        }
        return;
    }

    if (CLOCK_COUNT_UP) {
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

    int remaining = CLOCK_TOTAL_TIME - countdown_elapsed_time;
    if (remaining <= 0) {
        time_text[0] = '\0';
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

    int hours = 0, minutes = 0, seconds = 0;
    char input_copy[256];
    strncpy(input_copy, input, sizeof(input_copy)-1);
    input_copy[sizeof(input_copy)-1] = '\0';

    char *tokens[3] = {0};
    int token_count = 0;

    char *token = strtok(input_copy, " ");
    while (token && token_count < 3) {
        tokens[token_count++] = token;
        token = strtok(NULL, " ");
    }

    if (token_count == 1) {
        char unit = tolower((unsigned char)tokens[0][strlen(tokens[0]) - 1]);
        if (unit == 'h' || unit == 'm' || unit == 's') {
            tokens[0][strlen(tokens[0]) - 1] = '\0';   
            int value = atoi(tokens[0]);
            switch (unit) {
                case 'h': hours = value; break;
                case 'm': minutes = value; break;
                case 's': seconds = value; break;
            }
        } else {
            minutes = atoi(tokens[0]);
        }
    } else if (token_count == 2) {
        char unit = tolower((unsigned char)tokens[1][strlen(tokens[1]) - 1]);
        if (unit == 'h' || unit == 'm' || unit == 's') {
            tokens[1][strlen(tokens[1]) - 1] = '\0';   
            int value1 = atoi(tokens[0]);
            int value2 = atoi(tokens[1]);
            switch (unit) {
                case 'h': 
                    minutes = value1;
                    hours = value2;
                    break;
                case 'm': 
                    hours = value1;
                    minutes = value2;
                    break;
                case 's':
                    minutes = value1;
                    seconds = value2;
                    break;
            }
        } else {
            minutes = atoi(tokens[0]);
            seconds = atoi(tokens[1]);
        }
    } else if (token_count == 3) {
        hours = atoi(tokens[0]);
        minutes = atoi(tokens[1]);
        seconds = atoi(tokens[2]);
    }

    *total_seconds = hours * 3600 + minutes * 60 + seconds;
    if (*total_seconds <= 0) return 0;

    if (hours < 0 || hours > 99 ||     
        minutes < 0 || minutes > 59 ||  
        seconds < 0 || seconds > 59) {  
        return 0;
    }

    if (hours > INT_MAX/3600 || 
        (*total_seconds) > INT_MAX) {
        return 0;
    }

    return 1;
}

int isValidInput(const char* input) {
    if (input == NULL || *input == '\0') {
        return 0;
    }

    int len = strlen(input);
    int spaceCount = 0;
    int digitCount = 0;

    for (int i = 0; i < len; i++) {
        if (isdigit(input[i])) {
            digitCount++;
        } else if (input[i] == ' ') {
            spaceCount++;
        } else if (i == len - 1 && (input[i] == 'h' || input[i] == 'm' || input[i] == 's')) {
            // 允许最后一个字符是h、m或s
        } else {
            return 0;
        }
    }

    if (digitCount == 0 || spaceCount > 2) {
        return 0;
    }

    return 1;
}