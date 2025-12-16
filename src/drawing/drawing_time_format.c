/**
 * @file drawing_time_format.c
 * @brief Time component retrieval and formatting implementation
 */

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "drawing/drawing_time_format.h"
#include "timer/timer.h"
#include "config.h"

extern int countdown_elapsed_time;
extern int countup_elapsed_time;

/** Declared in drawing_timer_precision.c */
extern int GetElapsedCentiseconds(void);
extern int GetSystemCentiseconds(void);

TimeComponents GetCurrentTimeComponents(BOOL use24Hour) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    TimeComponents tc;
    tc.hours = st.wHour;
    tc.minutes = st.wMinute;
    tc.seconds = st.wSecond;
    tc.centiseconds = st.wMilliseconds / 10;
    
    if (!use24Hour) {
        if (tc.hours == 0) {
            tc.hours = 12;
        } else if (tc.hours > 12) {
            tc.hours -= 12;
        }
    }
    
    return tc;
}

TimeComponents GetCountUpComponents(void) {
    extern BOOL CLOCK_IS_PAUSED;
    extern int64_t g_start_time;
    extern int64_t g_pause_start_time;
    extern int64_t GetAbsoluteTimeMs(void);
    
    /* Single time sample for both seconds and centiseconds */
    int64_t now = CLOCK_IS_PAUSED ? g_pause_start_time : GetAbsoluteTimeMs();
    int64_t elapsed_ms = now - g_start_time;
    if (elapsed_ms < 0) elapsed_ms = 0;
    
    int total_seconds = (int)(elapsed_ms / 1000);
    int centis = (int)((elapsed_ms % 1000) / 10);
    
    TimeComponents tc;
    tc.hours = total_seconds / 3600;
    tc.minutes = (total_seconds % 3600) / 60;
    tc.seconds = total_seconds % 60;
    tc.centiseconds = centis;
    return tc;
}

TimeComponents GetCountDownComponents(void) {
    extern BOOL CLOCK_IS_PAUSED;
    extern int64_t g_target_end_time;
    extern int64_t g_pause_start_time;
    extern int64_t GetAbsoluteTimeMs(void);
    
    /* Single time sample for both seconds and centiseconds */
    int64_t now = CLOCK_IS_PAUSED ? g_pause_start_time : GetAbsoluteTimeMs();
    int64_t remaining_ms = g_target_end_time - now;
    if (remaining_ms < 0) remaining_ms = 0;
    
    int total_seconds = (int)(remaining_ms / 1000);
    int centis = (int)((remaining_ms % 1000) / 10);
    
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
    
    extern TimeFormatType GetActiveTimeFormat(void);
    extern BOOL GetActiveShowMilliseconds(void);
    
    TimeFormatType finalFormat = GetActiveTimeFormat();
    BOOL finalShowMs = GetActiveShowMilliseconds();
    
    if (CLOCK_SHOW_CURRENT_TIME) {
        TimeComponents tc = GetCurrentTimeComponents(CLOCK_USE_24HOUR);
        
        if (CLOCK_SHOW_SECONDS) {
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
                MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_TEXT, -1, buffer, (int)bufferSize);
            } else {
                buffer[0] = L'\0';
            }
        } else {
            TimeComponents tc = GetCountDownComponents();
            FormatTimeComponentsForDisplay(&tc, finalFormat, finalShowMs, buffer, bufferSize);
        }
    }
}

