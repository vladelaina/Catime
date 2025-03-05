/**
 * @file timer_events.c
 * @brief 计时器事件处理实现
 * 
 * 本文件实现了应用程序计时器事件处理相关的功能，
 * 包括倒计时和正计时模式的事件处理。
 */

#include <windows.h>
#include <stdlib.h>
#include "../include/timer_events.h"
#include "../include/timer.h"
#include "../include/language.h"
#include "../include/notification.h"

// 从main.c引入的函数声明
extern void ShowToastNotification(HWND hwnd, const char* message);

/**
 * @brief 处理计时器消息
 * @param hwnd 窗口句柄
 * @param wp 消息参数
 * @return BOOL 是否处理了消息
 */
BOOL HandleTimerEvent(HWND hwnd, WPARAM wp) {
    if (wp == 1) {
        if (CLOCK_SHOW_CURRENT_TIME) {
            InvalidateRect(hwnd, NULL, TRUE);
            return TRUE;
        }

        if (CLOCK_COUNT_UP) {
            if (!CLOCK_IS_PAUSED) {
                countup_elapsed_time++;
                InvalidateRect(hwnd, NULL, TRUE);
            }
        } else {
            if (countdown_elapsed_time < CLOCK_TOTAL_TIME) {
                countdown_elapsed_time++;
                if (countdown_elapsed_time >= CLOCK_TOTAL_TIME && !countdown_message_shown) {
                    countdown_message_shown = TRUE;
                    
                    switch (CLOCK_TIMEOUT_ACTION) {
                        case TIMEOUT_ACTION_MESSAGE:
                            ShowToastNotification(hwnd, "Time's up!");
                            break;
                        case TIMEOUT_ACTION_LOCK:
                            LockWorkStation();
                            break;
                        case TIMEOUT_ACTION_SHUTDOWN:
                            system("shutdown /s /t 0");
                            break;
                        case TIMEOUT_ACTION_RESTART:
                            system("shutdown /r /t 0");
                            break;
                        case TIMEOUT_ACTION_OPEN_FILE: {
                            if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
                                wchar_t wPath[MAX_PATH];
                                MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_FILE_PATH, -1, wPath, MAX_PATH);
                                
                                HINSTANCE result = ShellExecuteW(NULL, L"open", wPath, NULL, NULL, SW_SHOWNORMAL);
                                
                                if ((INT_PTR)result <= 32) {
                                    MessageBoxW(hwnd, 
                                        GetLocalizedString(L"无法打开文件", L"Failed to open file"),
                                        GetLocalizedString(L"错误", L"Error"),
                                        MB_ICONERROR);
                                }
                            }
                            break;
                        }
                    }
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        return TRUE;
    }
    return FALSE;
}