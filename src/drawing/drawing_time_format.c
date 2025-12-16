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
    
    /* Forward declare for smooth display */
    extern BOOL CLOCK_IS_PAUSED;
    static int64_t s_current_time_last_second = -1;
    static int s_current_time_frame_count = 0;
    
    int64_t current_second = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
    int real_centis = st.wMilliseconds / 10;
    int centiseconds;
    
    /* Don't smooth if paused or if in special states */
    if (CLOCK_IS_PAUSED) {
        centiseconds = real_centis;
    } else {
        /* Resync on second boundary */
        if (current_second != s_current_time_last_second) {
            s_current_time_last_second = current_second;
            s_current_time_frame_count = 0;
        }
        
        /* Smooth display for first 45 frames */
        if (s_current_time_frame_count < 45) {
            centiseconds = (s_current_time_frame_count * 2) % 100;
            s_current_time_frame_count++;
        } else {
            centiseconds = real_centis;
        }
    }
    
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
 * Smooth centiseconds display system
 * 
 * Problem: Real-time sampling causes visual "stuttering" because render timing is uneven.
 * Solution: During active timing, display smoothly incrementing centiseconds.
 *           When paused/stopped, switch to real time for accuracy.
 * 
 * The display increments by 2 centiseconds per 20ms frame for visual smoothness.
 * Final time (on pause/stop) shows the accurate real value.
 */
static int s_smooth_frame_count = 0;        /* Logical frame counter */
static int64_t s_last_real_seconds = -1;    /* Track second boundaries */
static BOOL s_smooth_active = FALSE;        /* Whether smooth mode is active */

/* Get smoothed centiseconds value (0-99)
 * isCountdown: TRUE for countdown (decreasing), FALSE for count-up (increasing)
 */
static int GetSmoothedCentiseconds(int64_t elapsed_ms, BOOL isCountdown) {
    extern BOOL CLOCK_IS_PAUSED;
    
    /* When paused, always show real time for accuracy */
    if (CLOCK_IS_PAUSED) {
        s_smooth_active = FALSE;
        return (int)((elapsed_ms % 1000) / 10);
    }
    
    /* Get real centiseconds for boundary handling */
    int real_centis = (int)((elapsed_ms % 1000) / 10);
    
    /* Calculate real seconds */
    int64_t real_seconds = elapsed_ms / 1000;
    
    /* On second boundary change, resync smooth display */
    if (real_seconds != s_last_real_seconds) {
        s_last_real_seconds = real_seconds;
        s_smooth_frame_count = 0;
        s_smooth_active = TRUE;
    }
    
    if (!s_smooth_active) {
        return real_centis;
    }
    
    /* Each frame advances by 2 centiseconds (20ms interval) */
    int smooth_centis;
    if (isCountdown) {
        /* Countdown: 99 → 97 → 95 → ... → 01 (decreasing) */
        smooth_centis = 99 - ((s_smooth_frame_count * 2) % 100);
    } else {
        /* Count-up: 00 → 02 → 04 → ... → 98 (increasing) */
        smooth_centis = (s_smooth_frame_count * 2) % 100;
    }
    s_smooth_frame_count++;
    
    /* After 45 frames (~900ms), blend towards real time to ensure smooth transition */
    if (s_smooth_frame_count >= 45) {
        /* Use real value for last portion of second to sync naturally */
        return real_centis;
    }
    
    return smooth_centis;
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
    
    /* Use smoothed centiseconds for visual fluidity, real value when paused */
    int centis = GetSmoothedCentiseconds(elapsed_ms, FALSE);  /* FALSE = count-up, increasing */
    
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
    
    /* Use smoothed centiseconds for visual fluidity, real value when paused */
    int centis = GetSmoothedCentiseconds(remaining_ms, TRUE);  /* TRUE = countdown, decreasing */
    
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

