#ifndef TIMER_H
#define TIMER_H

#include <windows.h>
#include <time.h>

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

// 函数声明
void FormatTime(int remaining_time, char* time_text);
int ParseInput(const char* input, int* total_seconds);
int isValidInput(const char* input);

#endif // TIMER_H