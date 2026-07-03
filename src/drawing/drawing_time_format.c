/**
 * @file drawing_time_format.c
 * @brief Time component retrieval and formatting implementation
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "drawing/drawing_time_format.h"
#include "timer/timer.h"
#include "config.h"
#include "menu_preview.h"

/** Declared in drawing_timer_precision.c */
extern int GetElapsedCentiseconds(void);
extern int GetSystemCentiseconds(void);

#define DRAWING_MS_PER_SECOND 1000LL

static int ClampMillisecondsToSecondsFloor(int64_t milliseconds) {
    int64_t seconds;
    if (milliseconds <= 0) return 0;

    seconds = milliseconds / DRAWING_MS_PER_SECOND;
    if (seconds > INT_MAX) return INT_MAX;
    return (int)seconds;
}

static int ClampMillisecondsToSecondsCeil(int64_t milliseconds) {
    if (milliseconds <= 0) return 0;
    if (milliseconds > (int64_t)INT_MAX * DRAWING_MS_PER_SECOND) {
        return INT_MAX;
    }

    return (int)((milliseconds + DRAWING_MS_PER_SECOND - 1) / DRAWING_MS_PER_SECOND);
}

TimeComponents GetCurrentTimeComponents(BOOL use24Hour) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    int centiseconds = st.wMilliseconds / 10;
    
    TimeComponents tc;
    tc.hours = st.wHour;
    tc.minutes = st.wMinute;
    tc.seconds = st.wSecond;
    tc.centiseconds = centiseconds;
    
    if (!use24Hour) {
        if (tc.hours == 0) {
            tc.hours = 12;
        } else if (tc.hours > 12) {
            tc.hours -= 12;
        }
    }
    
    return tc;
}

/**
 * Centiseconds display uses real sampled values to prioritize accuracy.
 */
/* Get centiseconds value (0-99) from real elapsed/remaining milliseconds */
static int GetSmoothedCentiseconds(int64_t elapsed_ms, BOOL isCountdown) {
    (void)isCountdown;
    return (int)((elapsed_ms % 1000) / 10);
}

TimeComponents GetCountUpComponents(void) {
    
    /* Single time sample for both seconds and centiseconds */
    int64_t now = CLOCK_IS_PAUSED ? g_pause_start_time : GetAbsoluteTimeMs();
    int64_t elapsed_ms = now - g_start_time;
    if (elapsed_ms < 0) elapsed_ms = 0;
    
    int total_seconds = ClampMillisecondsToSecondsFloor(elapsed_ms);
    
    /* Use real centiseconds sampled from elapsed milliseconds */
    int centis = GetSmoothedCentiseconds(elapsed_ms, FALSE);
    
    TimeComponents tc;
    tc.hours = total_seconds / 3600;
    tc.minutes = (total_seconds % 3600) / 60;
    tc.seconds = total_seconds % 60;
    tc.centiseconds = centis;
    return tc;
}

TimeComponents GetCountDownComponents(void) {
    
    /* Single time sample for both seconds and centiseconds */
    int64_t now = CLOCK_IS_PAUSED ? g_pause_start_time : GetAbsoluteTimeMs();
    int64_t remaining_ms = g_target_end_time - now;
    if (remaining_ms < 0) remaining_ms = 0;
    
    /* With centiseconds shown, use real second value to avoid +1s display skew.
     * Without centiseconds, keep round-up behavior for UX (e.g. 10:00 not 9:59). */
    int total_seconds;
    if (GetActiveShowMilliseconds()) {
        total_seconds = ClampMillisecondsToSecondsFloor(remaining_ms);
    } else {
        total_seconds = ClampMillisecondsToSecondsCeil(remaining_ms);
    }
    
    /* Use real centiseconds sampled from remaining milliseconds */
    int centis = GetSmoothedCentiseconds(remaining_ms, TRUE);
    
    TimeComponents tc;
    tc.hours = total_seconds / 3600;
    tc.minutes = (total_seconds % 3600) / 60;
    tc.seconds = total_seconds % 60;
    tc.centiseconds = centis;
    return tc;
}


void FormatTimeComponentsForDisplay(
    const TimeComponents* tc,
    TimeFormatType format,
    BOOL showMilliseconds,
    wchar_t* buffer,
    size_t bufferSize
) {
    if (!tc || !buffer || bufferSize == 0) return;
    
    if (tc->hours > 0) {
        if (showMilliseconds) {
            switch (format) {
                case TIME_FORMAT_ZERO_PADDED:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%02d:%02d:%02d.%02d", 
                            tc->hours, tc->minutes, tc->seconds, tc->centiseconds);
                    break;
                case TIME_FORMAT_FULL_PADDED:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%02d:%02d:%02d.%02d", 
                            tc->hours, tc->minutes, tc->seconds, tc->centiseconds);
                    break;
                default:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d:%02d.%02d", 
                            tc->hours, tc->minutes, tc->seconds, tc->centiseconds);
                    break;
            }
        } else {
            switch (format) {
                case TIME_FORMAT_ZERO_PADDED:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%02d:%02d:%02d", 
                            tc->hours, tc->minutes, tc->seconds);
                    break;
                case TIME_FORMAT_FULL_PADDED:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%02d:%02d:%02d", 
                            tc->hours, tc->minutes, tc->seconds);
                    break;
                default:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d:%02d", 
                            tc->hours, tc->minutes, tc->seconds);
                    break;
            }
        }
    } else if (tc->minutes > 0) {
        if (showMilliseconds) {
            switch (format) {
                case TIME_FORMAT_ZERO_PADDED:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%02d:%02d.%02d", 
                            tc->minutes, tc->seconds, tc->centiseconds);
                    break;
                case TIME_FORMAT_FULL_PADDED:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"00:%02d:%02d.%02d", 
                            tc->minutes, tc->seconds, tc->centiseconds);
                    break;
                default:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d.%02d", 
                            tc->minutes, tc->seconds, tc->centiseconds);
                    break;
            }
        } else {
            switch (format) {
                case TIME_FORMAT_ZERO_PADDED:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%02d:%02d", 
                            tc->minutes, tc->seconds);
                    break;
                case TIME_FORMAT_FULL_PADDED:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"00:%02d:%02d", 
                            tc->minutes, tc->seconds);
                    break;
                default:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d", 
                            tc->minutes, tc->seconds);
                    break;
            }
        }
    } else {
        if (showMilliseconds) {
            switch (format) {
                case TIME_FORMAT_ZERO_PADDED:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"00:%02d.%02d", 
                            tc->seconds, tc->centiseconds);
                    break;
                case TIME_FORMAT_FULL_PADDED:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"00:00:%02d.%02d", 
                            tc->seconds, tc->centiseconds);
                    break;
                default:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d.%02d", 
                            tc->seconds, tc->centiseconds);
                    break;
            }
        } else {
            switch (format) {
                case TIME_FORMAT_ZERO_PADDED:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"00:%02d", tc->seconds);
                    break;
                case TIME_FORMAT_FULL_PADDED:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"00:00:%02d", tc->seconds);
                    break;
                default:
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d", tc->seconds);
                    break;
            }
        }
    }
}

void GetTimeText(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    

    TimeFormatType finalFormat = GetActiveTimeFormat();
    BOOL finalShowMs = GetActiveShowMilliseconds();
    BOOL finalShowSeconds = GetActiveShowSeconds();

    if (CLOCK_SHOW_CURRENT_TIME) {
        TimeComponents tc = GetCurrentTimeComponents(GetActiveUse24Hour());

        if (finalShowSeconds) {
            /** Current time always shows hours (even if 0 in 24-hour mode) */
            if (finalShowMs) {
                if (finalFormat == TIME_FORMAT_ZERO_PADDED || finalFormat == TIME_FORMAT_FULL_PADDED) {
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%02d:%02d:%02d.%02d", 
                            tc.hours, tc.minutes, tc.seconds, tc.centiseconds);
                } else {
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d:%02d.%02d", 
                            tc.hours, tc.minutes, tc.seconds, tc.centiseconds);
                }
            } else {
                if (finalFormat == TIME_FORMAT_ZERO_PADDED || finalFormat == TIME_FORMAT_FULL_PADDED) {
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%02d:%02d:%02d", 
                            tc.hours, tc.minutes, tc.seconds);
                } else {
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d:%02d", 
                            tc.hours, tc.minutes, tc.seconds);
                }
            }
        } else {
            /** Milliseconds override seconds hiding */
            if (finalShowMs) {
                if (finalFormat == TIME_FORMAT_ZERO_PADDED || finalFormat == TIME_FORMAT_FULL_PADDED) {
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%02d:%02d:%02d.%02d", 
                            tc.hours, tc.minutes, tc.seconds, tc.centiseconds);
                } else {
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d:%02d.%02d", 
                            tc.hours, tc.minutes, tc.seconds, tc.centiseconds);
                }
            } else {
                if (finalFormat == TIME_FORMAT_ZERO_PADDED || finalFormat == TIME_FORMAT_FULL_PADDED) {
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%02d:%02d", tc.hours, tc.minutes);
                } else {
                    _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"%d:%02d", tc.hours, tc.minutes);
                }
            }
        }
    } else if (CLOCK_COUNT_UP) {
        TimeComponents tc = GetCountUpComponents();
        FormatTimeComponentsForDisplay(&tc, finalFormat, finalShowMs, buffer, bufferSize);
    } else {
        int remaining = CLOCK_TOTAL_TIME - countdown_elapsed_time;
        
        if (remaining <= 0) {
            /** Empty timeout text hides window */
            if (CLOCK_TOTAL_TIME == 0 && countdown_elapsed_time == 0) {
                buffer[0] = L'\0';
            } else if (strcmp(CLOCK_TIMEOUT_TEXT, "0") == 0) {
                buffer[0] = L'\0';
            } else if (strlen(CLOCK_TIMEOUT_TEXT) > 0) {
                if (bufferSize > (size_t)INT_MAX ||
                    MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_TEXT, -1, buffer, (int)bufferSize) <= 0) {
                    buffer[0] = L'\0';
                }
            } else {
                buffer[0] = L'\0';
            }
        } else {
            TimeComponents tc = GetCountDownComponents();
            FormatTimeComponentsForDisplay(&tc, finalFormat, finalShowMs, buffer, bufferSize);
        }
    }
}

