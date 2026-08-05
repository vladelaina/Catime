/** @file timer_format.c @brief User-facing clock and timer formatting. */

#include "timer/timer.h"
#include "config.h"
#include "menu_preview.h"
#include <stdio.h>

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600

static void FormatComponents(int hours, int minutes, int seconds, char* output) {
    if (hours > 0) snprintf(output, 64, "%d:%02d:%02d", hours, minutes, seconds);
    else if (minutes > 0) snprintf(output, 64, "    %d:%02d", minutes, seconds);
    else snprintf(output, 64, seconds < 10 ? "          %d" : "        %d", seconds);
}

static int To12Hour(int hour) {
    if (hour == 0) return 12;
    return hour > 12 ? hour - 12 : hour;
}

static void FormatSystemClock(char* output) {
    SYSTEMTIME time;
    GetLocalTime(&time);
    if (last_displayed_second < 0 || time.wSecond != last_displayed_second)
        last_displayed_second = time.wSecond;

    int hour = GetActiveUse24Hour() ? time.wHour : To12Hour(time.wHour);
    if (GetActiveShowSeconds())
        snprintf(output, 64, "%d:%02d:%02d", hour, time.wMinute,
                 last_displayed_second);
    else
        snprintf(output, 64, "%d:%02d", hour, time.wMinute);
}

static void FormatElapsed(char* output) {
    int hours = countup_elapsed_time / SECONDS_PER_HOUR;
    int minutes = countup_elapsed_time % SECONDS_PER_HOUR / SECONDS_PER_MINUTE;
    FormatComponents(hours, minutes, countup_elapsed_time % SECONDS_PER_MINUTE, output);
}

static void FormatRemaining(char* output) {
    int remaining = CLOCK_TOTAL_TIME - countdown_elapsed_time;
    if (remaining <= 0) { snprintf(output, 64, "    0:00"); return; }
    int hours = remaining / SECONDS_PER_HOUR;
    int minutes = remaining % SECONDS_PER_HOUR / SECONDS_PER_MINUTE;
    FormatComponents(hours, minutes, remaining % SECONDS_PER_MINUTE, output);
}

void FormatTime(int remaining_time, char* time_text) {
    (void)remaining_time;
    if (CLOCK_SHOW_CURRENT_TIME) FormatSystemClock(time_text);
    else if (CLOCK_COUNT_UP) FormatElapsed(time_text);
    else FormatRemaining(time_text);
}
