#ifndef TIMER_H
#define TIMER_H

#include <windows.h>
#include <time.h>

#define MAX_TIME_OPTIONS 50

extern int POMODORO_WORK_TIME;
extern int POMODORO_SHORT_BREAK;
extern int POMODORO_LONG_BREAK;
extern int POMODORO_LOOP_COUNT;

typedef enum {
    TIMEOUT_ACTION_MESSAGE = 0,
    TIMEOUT_ACTION_LOCK = 1,
    TIMEOUT_ACTION_SHUTDOWN = 2,
    TIMEOUT_ACTION_RESTART = 3,
    TIMEOUT_ACTION_OPEN_FILE = 4,
    TIMEOUT_ACTION_SHOW_TIME = 5,
    TIMEOUT_ACTION_COUNT_UP = 6,
    TIMEOUT_ACTION_OPEN_WEBSITE = 7,
    TIMEOUT_ACTION_SLEEP = 8,
    TIMEOUT_ACTION_RUN_COMMAND = 9,
    TIMEOUT_ACTION_HTTP_REQUEST = 10
} TimeoutActionType;

extern BOOL CLOCK_IS_PAUSED;
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_USE_24HOUR;
extern BOOL CLOCK_SHOW_SECONDS;
extern BOOL CLOCK_COUNT_UP;
extern char CLOCK_STARTUP_MODE[20];

extern int CLOCK_TOTAL_TIME;
extern int countdown_elapsed_time;
extern int countup_elapsed_time;
extern time_t CLOCK_LAST_TIME_UPDATE;
extern int last_displayed_second;

extern BOOL countdown_message_shown;
extern BOOL countup_message_shown;
extern int pomodoro_work_cycles;

extern TimeoutActionType CLOCK_TIMEOUT_ACTION;
extern char CLOCK_TIMEOUT_TEXT[50];
extern char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH];
extern wchar_t CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH];

extern int time_options[MAX_TIME_OPTIONS];
extern int time_options_count;

void FormatTime(int remaining_time, char* time_text);

int ParseInput(const char* input, int* total_seconds);

int isValidInput(const char* input);

void WriteConfigDefaultStartTime(int seconds);

void WriteConfigStartupMode(const char* mode);

#endif