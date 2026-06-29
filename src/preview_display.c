/**
 * @file preview_display.c
 * @brief Temporary display support for live previews.
 */

#include <windows.h>
#include <stdint.h>
#include "preview_display.h"
#include "config.h"
#include "menu_preview.h"
#include "timer/timer.h"
#include "window/window_core.h"
#include "window/window_desktop_integration.h"
#include "log.h"

/* ============================================================================
 * External Dependencies
 * ============================================================================ */

extern void ResetTimerWithInterval(HWND hwnd);

/* ============================================================================
 * Preview Display State
 * ============================================================================ */

typedef struct {
    BOOL wasWindowVisible;
    BOOL didShowForPreview;
    BOOL createdPreviewContent;
    BOOL savedDisplayState;
    BOOL previousShowCurrentTime;
    BOOL previousCountUp;
    BOOL previousIsPaused;
    int32_t previousTotalTime;
    int32_t previousCountdownElapsed;
    int64_t previousTargetEndTime;
    int64_t previousPauseStartTime;
} PreviewDisplayState;

static PreviewDisplayState g_previewDisplayState = {0};

/* ============================================================================
 * Window Visibility Management for Preview
 * ============================================================================ */

void ShowWindowForPreview(HWND hwnd) {
    if (!hwnd) return;

    BOOL isVisible = IsWindowVisible(hwnd);

    BOOL hasActiveContent = CLOCK_SHOW_CURRENT_TIME || CLOCK_COUNT_UP ||
                           (CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time < CLOCK_TOTAL_TIME);

    LOG_DEBUG("ShowWindowForPreview: visible=%d, showTime=%d, countUp=%d, total=%d, elapsed=%d, hasContent=%d, didShow=%d",
             isVisible, CLOCK_SHOW_CURRENT_TIME, CLOCK_COUNT_UP, CLOCK_TOTAL_TIME, countdown_elapsed_time, hasActiveContent,
             g_previewDisplayState.didShowForPreview);

    if (g_previewDisplayState.didShowForPreview) {
        LOG_DEBUG("Already in preview display mode, refreshing display");
        InvalidateRect(hwnd, NULL, TRUE);
        return;
    }

    if (!isVisible || !hasActiveContent) {
        g_previewDisplayState.wasWindowVisible = isVisible;
        g_previewDisplayState.didShowForPreview = TRUE;

        if (!hasActiveContent) {
            LOG_DEBUG("No active content, showing current time for preview");

            g_previewDisplayState.createdPreviewContent = TRUE;
            g_previewDisplayState.savedDisplayState = TRUE;
            g_previewDisplayState.previousShowCurrentTime = CLOCK_SHOW_CURRENT_TIME;
            g_previewDisplayState.previousCountUp = CLOCK_COUNT_UP;
            g_previewDisplayState.previousIsPaused = CLOCK_IS_PAUSED;
            g_previewDisplayState.previousTotalTime = CLOCK_TOTAL_TIME;
            g_previewDisplayState.previousCountdownElapsed = countdown_elapsed_time;
            g_previewDisplayState.previousTargetEndTime = g_target_end_time;
            g_previewDisplayState.previousPauseStartTime = g_pause_start_time;

            CLOCK_SHOW_CURRENT_TIME = true;
            CLOCK_COUNT_UP = false;
            CLOCK_IS_PAUSED = false;

            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            g_pause_start_time = 0;
            g_target_end_time = 0;

            ResetTimerWithInterval(hwnd);
        } else {
            LOG_DEBUG("Window hidden but has active timer, just showing it");
            g_previewDisplayState.createdPreviewContent = FALSE;
            g_previewDisplayState.savedDisplayState = FALSE;
        }

        if (!isVisible) {
            EnsureWindowVisibleWithTopmostState(hwnd);
        }
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        g_previewDisplayState.wasWindowVisible = TRUE;
        g_previewDisplayState.didShowForPreview = FALSE;
        g_previewDisplayState.createdPreviewContent = FALSE;
        g_previewDisplayState.savedDisplayState = FALSE;
    }
}

void RestoreWindowVisibility(HWND hwnd) {
    if (!hwnd || !g_previewDisplayState.didShowForPreview) return;

    LOG_DEBUG("RestoreWindowVisibility: was visible=%d, created preview content=%d",
             g_previewDisplayState.wasWindowVisible, g_previewDisplayState.createdPreviewContent);

    if (g_previewDisplayState.createdPreviewContent) {
        LOG_DEBUG("Clearing preview content that we created");
        if (g_previewDisplayState.savedDisplayState) {
            CLOCK_SHOW_CURRENT_TIME = g_previewDisplayState.previousShowCurrentTime;
            CLOCK_COUNT_UP = g_previewDisplayState.previousCountUp;
            CLOCK_IS_PAUSED = g_previewDisplayState.previousIsPaused;
            CLOCK_TOTAL_TIME = g_previewDisplayState.previousTotalTime;
            countdown_elapsed_time = g_previewDisplayState.previousCountdownElapsed;
            g_target_end_time = g_previewDisplayState.previousTargetEndTime;
            g_pause_start_time = g_previewDisplayState.previousPauseStartTime;
        } else {
            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            CLOCK_SHOW_CURRENT_TIME = false;
            CLOCK_COUNT_UP = false;
            CLOCK_IS_PAUSED = false;
            g_target_end_time = 0;
            g_pause_start_time = 0;
        }
        ResetTimerWithInterval(hwnd);
    } else {
        LOG_DEBUG("Not clearing content - was showing existing active content");
    }

    if (!g_previewDisplayState.wasWindowVisible) {
        HideWindowIntentionally(hwnd);
    } else {
        InvalidateRect(hwnd, NULL, TRUE);
    }

    g_previewDisplayState.didShowForPreview = FALSE;
    g_previewDisplayState.createdPreviewContent = FALSE;
    g_previewDisplayState.savedDisplayState = FALSE;
}

/* ============================================================================
 * Preview Time Text Generation
 * ============================================================================ */

BOOL GetPreviewTimeText(wchar_t* outText, size_t bufferSize) {
    if (!outText || bufferSize == 0) return FALSE;

    if (!CLOCK_EDIT_MODE) {
        return FALSE;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);

    int hours = CLOCK_USE_24HOUR ? st.wHour : (st.wHour % 12 == 0 ? 12 : st.wHour % 12);
    int minutes = st.wMinute;
    int seconds = st.wSecond;

    TimeFormatType format = GetActiveTimeFormat();
    BOOL showMs = GetActiveShowMilliseconds();
    BOOL showSeconds = GetActiveShowSeconds();

    if (showMs) {
        int centiseconds = st.wMilliseconds / 10;
        if (format == TIME_FORMAT_FULL_PADDED || format == TIME_FORMAT_ZERO_PADDED) {
            _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%02d:%02d:%02d.%02d",
                        hours, minutes, seconds, centiseconds);
        } else {
            _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%d:%02d:%02d.%02d",
                        hours, minutes, seconds, centiseconds);
        }
    } else {
        if (showSeconds) {
            if (format == TIME_FORMAT_FULL_PADDED || format == TIME_FORMAT_ZERO_PADDED) {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%02d:%02d:%02d", hours, minutes, seconds);
            } else {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%d:%02d:%02d", hours, minutes, seconds);
            }
        } else {
            if (format == TIME_FORMAT_FULL_PADDED || format == TIME_FORMAT_ZERO_PADDED) {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%02d:%02d", hours, minutes);
            } else {
                _snwprintf_s(outText, bufferSize, _TRUNCATE, L"%d:%02d", hours, minutes);
            }
        }
    }

    return TRUE;
}
