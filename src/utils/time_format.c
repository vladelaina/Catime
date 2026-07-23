#include "utils/time_parser.h"

#include <ctype.h>
#include <stdio.h>

void TimeParser_FormatToString(int seconds, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    if (seconds < 0) {
        snprintf(buffer, bufferSize, "0s");
        return;
    }

    int hours = seconds / SECONDS_PER_HOUR;
    int minutes = (seconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
    int remainingSeconds = seconds % SECONDS_PER_MINUTE;

    if (hours > 0 && minutes > 0 && remainingSeconds > 0) {
        snprintf(buffer, bufferSize, "%dh%dm%ds", hours, minutes,
                 remainingSeconds);
    } else if (hours > 0 && minutes > 0) {
        snprintf(buffer, bufferSize, "%dh%dm", hours, minutes);
    } else if (hours > 0 && remainingSeconds > 0) {
        snprintf(buffer, bufferSize, "%dh%ds", hours, remainingSeconds);
    } else if (minutes > 0 && remainingSeconds > 0) {
        snprintf(buffer, bufferSize, "%dm%ds", minutes, remainingSeconds);
    } else if (hours > 0) {
        snprintf(buffer, bufferSize, "%dh", hours);
    } else if (minutes > 0) {
        snprintf(buffer, bufferSize, "%dm", minutes);
    } else {
        snprintf(buffer, bufferSize, "%ds", remainingSeconds);
    }
}

void TimeParser_FormatToHMS(int seconds, int* hours, int* mins, int* secs) {
    if (hours) *hours = seconds / SECONDS_PER_HOUR;
    if (mins) *mins = (seconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
    if (secs) *secs = seconds % SECONDS_PER_MINUTE;
}

int TimeParser_GetUnitMultiplier(char unit) {
    switch (tolower((unsigned char)unit)) {
        case 'h': return SECONDS_PER_HOUR;
        case 'm': return SECONDS_PER_MINUTE;
        case 's': return 1;
        default: return 0;
    }
}
