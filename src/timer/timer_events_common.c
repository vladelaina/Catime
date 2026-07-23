/**
 * @file timer_events_common.c
 * @brief Small helpers shared by timer event handlers.
 */

#include <wchar.h>

#include "timer_events_internal.h"

void TimerEvents_ForceWindowRedraw(HWND hwnd) {
    InvalidateRect(hwnd, NULL, TRUE);
}

void TimerEvents_RequestWindowRepaint(HWND hwnd) {
    if (CLOCK_IS_DRAGGING) {
        return;
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

BOOL TimerEvents_StartMainTimerForTimeoutAction(HWND hwnd,
                                                const char* actionName) {
    UINT interval = GetTimerInterval();
    if (MainTimer_Start(hwnd, interval)) {
        return TRUE;
    }

    LOG_WARNING("Failed to start main timer for timeout action %s (interval=%u)",
                actionName ? actionName : "unknown", interval);
    return FALSE;
}

static wchar_t* SafeUtf8ToWide(const char* utf8String,
                               wchar_t* buffer,
                               size_t bufferSize) {
    if (!utf8String || !buffer || utf8String[0] == '\0') {
        return NULL;
    }

    return Utf8ToWide(utf8String, buffer, bufferSize) ? buffer : NULL;
}

void TimerEvents_ShowTimeoutNotification(HWND hwnd,
                                         const char* messageUtf8,
                                         BOOL playSound) {
    if (!messageUtf8 || messageUtf8[0] == '\0') {
        return;
    }

    wchar_t messageBuffer[MESSAGE_BUFFER_SIZE];
    const wchar_t* messageW = SafeUtf8ToWide(messageUtf8,
                                              messageBuffer,
                                              MESSAGE_BUFFER_SIZE);
    if (messageW) {
        ShowNotification(hwnd, messageW);
    }

    if (playSound && CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE) {
        PlayNotificationSound(hwnd);
    }
}

void TimerEvents_ResetTimerState(int newTotalTime) {
    CLOCK_TOTAL_TIME = newTotalTime;
    countdown_elapsed_time = 0;
}

void TimerEvents_ResetMillisecondAccumulator(void) {
    last_timer_tick = GetTickCount();
    ms_accumulator = 0;
    ResetTimerMilliseconds();
}

/* Keep the public API in this module while the implementation is split. */
void ResetMillisecondAccumulator(void) {
    TimerEvents_ResetMillisecondAccumulator();
}
