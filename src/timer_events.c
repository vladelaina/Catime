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
#include "../include/pomodoro.h"

// 当前番茄钟阶段变量
POMODORO_PHASE current_pomodoro_phase = POMODORO_PHASE_IDLE;

// 番茄钟循环计数器
static int pomodoro_cycle_counter = 0;

// 从main.c引入的函数声明
extern void ShowToastNotification(HWND hwnd, const char* message);

/**
 * @brief 设置番茄钟为工作阶段
 * 
 * 重置所有计时器计数并将番茄钟设置为工作阶段
 */
void InitializePomodoro(void) {
    current_pomodoro_phase = POMODORO_PHASE_WORK;
    pomodoro_work_cycles = 0;
    pomodoro_cycle_counter = 0;
}

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
                    
                    // 使用番茄钟阶段变量判断当前处于哪个阶段，而不仅仅依赖时间值
                    if (CLOCK_TOTAL_TIME == POMODORO_WORK_TIME && current_pomodoro_phase == POMODORO_PHASE_WORK) {
                        // 工作时间结束，增加工作周期计数
                        pomodoro_work_cycles++;
                        pomodoro_cycle_counter++;
                        
                        // 每完成两个工作周期后进入长休息，否则进入短休息
                        if (pomodoro_work_cycles % 2 == 0) {
                            // 第二个工作周期结束，切换到长休息
                            CLOCK_TOTAL_TIME = POMODORO_LONG_BREAK;
                            current_pomodoro_phase = POMODORO_PHASE_LONG_BREAK;
                        } else {
                            // 第一个工作周期结束，切换到短休息
                            CLOCK_TOTAL_TIME = POMODORO_SHORT_BREAK;
                            current_pomodoro_phase = POMODORO_PHASE_SHORT_BREAK;
                        }
                        countdown_elapsed_time = 0;
                        countdown_message_shown = FALSE;
                        InvalidateRect(hwnd, NULL, TRUE);
                        // 显示超时消息
                        ShowToastNotification(hwnd, "Time's up!");
                    } else if (CLOCK_TOTAL_TIME == POMODORO_SHORT_BREAK && current_pomodoro_phase == POMODORO_PHASE_SHORT_BREAK) {
                        // 短休息结束，切换到工作时间
                        CLOCK_TOTAL_TIME = POMODORO_WORK_TIME;
                        current_pomodoro_phase = POMODORO_PHASE_WORK;
                        countdown_elapsed_time = 0;
                        countdown_message_shown = FALSE;
                        InvalidateRect(hwnd, NULL, TRUE);
                        // 显示超时消息
                        ShowToastNotification(hwnd, "Time's up!");
                    } else if ((CLOCK_TOTAL_TIME == POMODORO_LONG_BREAK && current_pomodoro_phase == POMODORO_PHASE_LONG_BREAK) || 
                              (pomodoro_cycle_counter >= 3 && CLOCK_TOTAL_TIME > 0)) {
                        // 长休息结束，一轮番茄钟完成，完全结束番茄钟计时
                        // 或者当工作时间和长休息时间相同时，通过计数器识别长休息结束
                        CLOCK_TOTAL_TIME = 0;
                        countdown_elapsed_time = 0;
                        countdown_message_shown = FALSE;
                        pomodoro_work_cycles = 0;
                        pomodoro_cycle_counter = 0;
                        current_pomodoro_phase = POMODORO_PHASE_IDLE;
                        
                        // 停止计时器
                        KillTimer(hwnd, 1);
                        
                        // 切换到显示当前时间模式
                        CLOCK_SHOW_CURRENT_TIME = TRUE;
                        CLOCK_LAST_TIME_UPDATE = 0;
                        
                        // 重新启动时间显示定时器
                        SetTimer(hwnd, 1, 1000, NULL);
                        
                        InvalidateRect(hwnd, NULL, TRUE);
                        // 显示超时消息
                        ShowToastNotification(hwnd, "Pomodoro cycle completed!");
                    } else {
                        // 显示超时消息
                        ShowToastNotification(hwnd, "Time's up!");
                        
                        // 非番茄钟模式，执行原有的超时动作
                        switch (CLOCK_TIMEOUT_ACTION) {
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
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        return TRUE;
    }
    return FALSE;
}