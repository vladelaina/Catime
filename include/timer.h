#ifndef TIMER_H
#define TIMER_H

#include <windows.h>
#include <time.h>

// 定义最大时间选项数量
#define MAX_TIME_OPTIONS 10

// 超时动作类型
typedef enum {
    TIMEOUT_ACTION_MESSAGE = 0,
    TIMEOUT_ACTION_LOCK = 1,
    TIMEOUT_ACTION_SHUTDOWN = 2,
    TIMEOUT_ACTION_RESTART = 3,
    TIMEOUT_ACTION_OPEN_FILE = 4   
} TimeoutActionType;

// 计时器状态
extern BOOL CLOCK_IS_PAUSED;
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_USE_24HOUR;
extern BOOL CLOCK_SHOW_SECONDS;
extern BOOL CLOCK_COUNT_UP;
extern char CLOCK_STARTUP_MODE[20];

// 计时器时间
extern int CLOCK_TOTAL_TIME;
extern int countdown_elapsed_time;
extern int countup_elapsed_time;
extern time_t CLOCK_LAST_TIME_UPDATE;

// 消息状态
extern BOOL countdown_message_shown;
extern BOOL countup_message_shown;

// 超时动作相关
extern TimeoutActionType CLOCK_TIMEOUT_ACTION;
extern char CLOCK_TIMEOUT_TEXT[50];
extern char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH];

// 时间选项
extern int time_options[MAX_TIME_OPTIONS];
extern int time_options_count;

// 函数声明
void FormatTime(int remaining_time, char* time_text);
int ParseInput(const char* input, int* total_seconds);
int isValidInput(const char* input);
void WriteConfigDefaultStartTime(int seconds);
void WriteConfigStartupMode(const char* mode);

#endif // TIMER_H