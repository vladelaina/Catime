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

// 完成的番茄钟循环次数
static int complete_pomodoro_cycles = 0;

// 从main.c引入的函数声明
extern void ShowToastNotification(HWND hwnd, const char* message);

/**
 * @brief 将宽字符串转换为UTF-8编码的普通字符串并显示通知
 * @param hwnd 窗口句柄
 * @param chinese 中文消息
 * @param english 英文消息
 */
static void ShowLocalizedNotification(HWND hwnd, const wchar_t* chinese, const wchar_t* english) {
    // 获取本地化字符串
    const wchar_t* localizedMsg = GetLocalizedString(chinese, english);
    
    // 计算所需的缓冲区大小
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, localizedMsg, -1, NULL, 0, NULL, NULL);
    
    // 分配内存
    char* utf8Msg = (char*)malloc(size_needed);
    if (utf8Msg) {
        // 转换为UTF-8
        WideCharToMultiByte(CP_UTF8, 0, localizedMsg, -1, utf8Msg, size_needed, NULL, NULL);
        
        // 显示通知
        ShowToastNotification(hwnd, utf8Msg);
        
        // 释放内存
        free(utf8Msg);
    }
}

/**
 * @brief 设置番茄钟为工作阶段
 * 
 * 重置所有计时器计数并将番茄钟设置为工作阶段
 */
void InitializePomodoro(void) {
    current_pomodoro_phase = POMODORO_PHASE_WORK;
    pomodoro_work_cycles = 0;
    pomodoro_cycle_counter = 0;
    complete_pomodoro_cycles = 0;
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
                        if (pomodoro_work_cycles == 2) {
                            // 第二个工作周期结束，切换到长休息
                            CLOCK_TOTAL_TIME = POMODORO_LONG_BREAK;
                            current_pomodoro_phase = POMODORO_PHASE_LONG_BREAK;
                            // 在这里重置工作周期，这样长休息后就会直接进入新的循环
                            pomodoro_work_cycles = 0;
                        } else {
                            // 第一个工作周期结束，切换到短休息
                            CLOCK_TOTAL_TIME = POMODORO_SHORT_BREAK;
                            current_pomodoro_phase = POMODORO_PHASE_SHORT_BREAK;
                        }
                        countdown_elapsed_time = 0;
                        countdown_message_shown = FALSE;
                        InvalidateRect(hwnd, NULL, TRUE);
                        // 显示超时消息
                        ShowLocalizedNotification(hwnd, L"时间到！", L"Time's up!");
                    } else if (CLOCK_TOTAL_TIME == POMODORO_SHORT_BREAK && current_pomodoro_phase == POMODORO_PHASE_SHORT_BREAK) {
                        // 短休息结束，切换到工作时间
                        CLOCK_TOTAL_TIME = POMODORO_WORK_TIME;
                        current_pomodoro_phase = POMODORO_PHASE_WORK;
                        countdown_elapsed_time = 0;
                        countdown_message_shown = FALSE;
                        InvalidateRect(hwnd, NULL, TRUE);
                        // 显示超时消息
                        ShowLocalizedNotification(hwnd, L"时间到！", L"Time's up!");
                    } else if (CLOCK_TOTAL_TIME == POMODORO_LONG_BREAK && current_pomodoro_phase == POMODORO_PHASE_LONG_BREAK) {
                        // 长休息结束
                        countdown_elapsed_time = 0;
                        countdown_message_shown = FALSE;
                        
                        // 增加完成循环计数器，并检查是否完成了所有配置的循环次数
                        complete_pomodoro_cycles++;
                        
                        if (complete_pomodoro_cycles >= POMODORO_LOOP_COUNT) {
                            // 已完成所有循环次数，结束番茄钟
                            // 重置所有计时参数
                            countdown_elapsed_time = 0;
                            countdown_message_shown = FALSE;
                            CLOCK_TOTAL_TIME = 0;
                            
                            // 完成所有循环后停止计时
                            complete_pomodoro_cycles = 0;
                            current_pomodoro_phase = POMODORO_PHASE_IDLE;
                            
                            // 显示完成提示并停止计时器
                            ShowLocalizedNotification(hwnd, L"所有番茄钟循环完成！", L"All Pomodoro cycles completed!");
                            KillTimer(hwnd, 1);
                        } else {
                            // 准备新工作周期参数
                            CLOCK_TOTAL_TIME = POMODORO_WORK_TIME;
                            countdown_elapsed_time = 0;
                            countdown_message_shown = FALSE;
                            
                            // 先设置阶段再更新显示
                            current_pomodoro_phase = POMODORO_PHASE_WORK;
                            
                            // 更新显示后发送通知
                            InvalidateRect(hwnd, NULL, TRUE);
                            ShowLocalizedNotification(hwnd, L"休息结束！重新开始工作！", L"Break over! Time to focus again.");
                            
                            // 重置周期计数器
                            pomodoro_cycle_counter = 0;
                        }
                    } else {
                        // 显示超时消息
                        ShowLocalizedNotification(hwnd, L"时间到！", L"Time's up!");
                        
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