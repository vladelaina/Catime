#include "../include/timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <shellapi.h>

// 计时器状态
BOOL CLOCK_IS_PAUSED = FALSE;
BOOL CLOCK_SHOW_CURRENT_TIME = FALSE;
BOOL CLOCK_USE_24HOUR = TRUE;
BOOL CLOCK_SHOW_SECONDS = TRUE;
BOOL CLOCK_COUNT_UP = FALSE;
char CLOCK_STARTUP_MODE[20] = "COUNTDOWN";

// 计时器时间
int CLOCK_TOTAL_TIME = 0;
int countdown_elapsed_time = 0;
int countup_elapsed_time = 0;
time_t CLOCK_LAST_TIME_UPDATE = 0;

// 消息状态
BOOL countdown_message_shown = FALSE;
BOOL countup_message_shown = FALSE;

// 超时动作相关
TimeoutActionType CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
char CLOCK_TIMEOUT_TEXT[50] = "";
char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH] = "";

// 时间选项
int time_options[MAX_TIME_OPTIONS];
int time_options_count = 0;

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

void WriteConfigDefaultStartTime(int seconds) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    
    // 获取配置文件路径
    char* appdata_path = getenv("LOCALAPPDATA");
    if (appdata_path) {
        snprintf(config_path, MAX_PATH, "%s\\Catime\\config.txt", appdata_path);
        snprintf(temp_path, MAX_PATH, "%s\\Catime\\config.txt.tmp", appdata_path);
    } else {
        strcpy(config_path, ".\\asset\\config.txt");
        strcpy(temp_path, ".\\asset\\config.txt.tmp");
    }
    
    FILE* file = fopen(config_path, "r");
    FILE* temp = fopen(temp_path, "w");
    
    if (!file || !temp) {
        if (file) fclose(file);
        if (temp) fclose(temp);
        return;
    }
    
    char line[256];
    int found = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "CLOCK_DEFAULT_START_TIME=", 25) == 0) {
            fprintf(temp, "CLOCK_DEFAULT_START_TIME=%d\n", seconds);
            found = 1;
        } else {
            fputs(line, temp);
        }
    }
    
    if (!found) {
        fprintf(temp, "CLOCK_DEFAULT_START_TIME=%d\n", seconds);
    }
    
    fclose(file);
    fclose(temp);
    
    remove(config_path);
    rename(temp_path, config_path);
}

void WriteConfigStartupMode(const char* mode) {
    char config_path[MAX_PATH];
    char temp_path[MAX_PATH];
    
    // 获取配置文件路径
    char* appdata_path = getenv("LOCALAPPDATA");
    if (appdata_path) {
        snprintf(config_path, MAX_PATH, "%s\\Catime\\config.txt", appdata_path);
        snprintf(temp_path, MAX_PATH, "%s\\Catime\\config.txt.tmp", appdata_path);
    } else {
        strcpy(config_path, ".\\asset\\config.txt");
        strcpy(temp_path, ".\\asset\\config.txt.tmp");
    }
    
    FILE* file = fopen(config_path, "r");
    FILE* temp = fopen(temp_path, "w");
    
    if (!file || !temp) {
        if (file) fclose(file);
        if (temp) fclose(temp);
        return;
    }
    
    char line[256];
    int found = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "STARTUP_MODE=", 13) == 0) {
            fprintf(temp, "STARTUP_MODE=%s\n", mode);
            found = 1;
        } else {
            fputs(line, temp);
        }
    }
    
    if (!found) {
        fprintf(temp, "STARTUP_MODE=%s\n", mode);
    }
    
    fclose(file);
    fclose(temp);
    
    remove(config_path);
    rename(temp_path, config_path);
}