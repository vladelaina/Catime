/**
 * @file timer_events_timeout.c
 * @brief Non-system actions performed when a countdown reaches zero.
 */

#include <string.h>

#include "timer_events_internal.h"

void TimerEvents_HandleTimeoutActions(HWND hwnd) {
    switch (CLOCK_TIMEOUT_ACTION) {
        case TIMEOUT_ACTION_MESSAGE:
            break;

        case TIMEOUT_ACTION_LOCK:
            if (!LockWorkStation()) {
                LOG_WARNING("Failed to lock workstation (error: %lu)",
                            GetLastError());
            }
            break;

        case TIMEOUT_ACTION_OPEN_FILE:
            if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
                wchar_t wPath[MAX_PATH];
                if (MultiByteToWideChar(CP_UTF8, 0,
                                        CLOCK_TIMEOUT_FILE_PATH, -1,
                                        wPath, MAX_PATH) <= 0) {
                    LOG_WARNING("Failed to convert timeout file path: %s",
                                CLOCK_TIMEOUT_FILE_PATH);
                    break;
                }

                HINSTANCE result = ShellExecuteW(NULL, L"open", wPath,
                                                 NULL, NULL, SW_SHOWNORMAL);
                if ((INT_PTR)result <= 32) {
                    LOG_WARNING("Failed to open timeout file: %s (error: %d)",
                                CLOCK_TIMEOUT_FILE_PATH,
                                (int)(INT_PTR)result);
                }
            }
            break;

        case TIMEOUT_ACTION_SHOW_TIME:
            StopNotificationSound();
            CLOCK_SHOW_CURRENT_TIME = true;
            CLOCK_COUNT_UP = false;
            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            TimerEvents_ResetMillisecondAccumulator();
            MainTimer_Stop();
            TimerEvents_StartMainTimerForTimeoutAction(hwnd, "show_time");
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case TIMEOUT_ACTION_COUNT_UP:
            StopNotificationSound();
            CLOCK_COUNT_UP = true;
            CLOCK_SHOW_CURRENT_TIME = false;
            countup_elapsed_time = 0;
            elapsed_time = 0;
            g_start_time = GetAbsoluteTimeMs();
            message_shown = FALSE;
            countdown_message_shown = false;
            CLOCK_IS_PAUSED = false;
            TimerEvents_ResetMillisecondAccumulator();
            MainTimer_Stop();
            if (!TimerEvents_StartMainTimerForTimeoutAction(hwnd, "count_up")) {
                CLOCK_IS_PAUSED = true;
            }
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case TIMEOUT_ACTION_OPEN_WEBSITE:
            if (strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                wchar_t wUrl[MAX_PATH];
                if (MultiByteToWideChar(CP_UTF8, 0,
                                        CLOCK_TIMEOUT_WEBSITE_URL, -1,
                                        wUrl, MAX_PATH) <= 0) {
                    LOG_WARNING("Failed to convert timeout website URL: %s",
                                CLOCK_TIMEOUT_WEBSITE_URL);
                    break;
                }

                if (!IsSafeOpenUrlW(wUrl)) {
                    LOG_WARNING("Blocked unsafe timeout website URL: %s",
                                CLOCK_TIMEOUT_WEBSITE_URL);
                    break;
                }

                HINSTANCE result = ShellExecuteW(NULL, L"open", wUrl,
                                                 NULL, NULL, SW_NORMAL);
                if ((INT_PTR)result <= 32) {
                    LOG_WARNING("Failed to open timeout website: %s (error: %d)",
                                CLOCK_TIMEOUT_WEBSITE_URL,
                                (int)(INT_PTR)result);
                }
            }
            break;

        case TIMEOUT_ACTION_SHUTDOWN:
        case TIMEOUT_ACTION_RESTART:
        case TIMEOUT_ACTION_SLEEP:
            /* Handled before this function by TimerEvents_ExecuteSystemAction. */
            break;
    }
}
