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

// 番茄钟时间列表最大容量
#define MAX_POMODORO_TIMES 10
extern int POMODORO_TIMES[MAX_POMODORO_TIMES]; // 存储所有番茄钟时间
extern int POMODORO_TIMES_COUNT;              // 实际的番茄钟时间数量

// 当前正在执行的番茄钟时间索引
static int current_pomodoro_time_index = 0;

// 定义 current_pomodoro_phase 变量，它在 pomodoro.h 中被声明为 extern
POMODORO_PHASE current_pomodoro_phase = POMODORO_PHASE_IDLE;

// 完成的番茄钟循环次数
static int complete_pomodoro_cycles = 0;

// 从main.c引入的函数声明
extern void ShowToastNotification(HWND hwnd, const char* message);

// 从main.c引入的变量声明，用于超时动作
extern int elapsed_time;
extern BOOL message_shown;

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
    // 使用已有的枚举值 POMODORO_PHASE_WORK 代替 POMODORO_PHASE_RUNNING
    current_pomodoro_phase = POMODORO_PHASE_WORK;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;
    
    // 设置初始倒计时为第一个时间值
    if (POMODORO_TIMES_COUNT > 0) {
        CLOCK_TOTAL_TIME = POMODORO_TIMES[0];
    } else {
        // 如果没有配置时间，使用默认的25分钟
        CLOCK_TOTAL_TIME = 1500;
    }
    
    countdown_elapsed_time = 0;
    countdown_message_shown = FALSE;
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
            // 在显示当前时间模式下，每次定时器触发都刷新显示
            InvalidateRect(hwnd, NULL, TRUE);
            return TRUE;
        }

        // 如果计时器暂停，不进行时间更新
        if (CLOCK_IS_PAUSED) {
            return TRUE;
        }

        if (CLOCK_COUNT_UP) {
            countup_elapsed_time++;
            InvalidateRect(hwnd, NULL, TRUE);
        } else {
            if (countdown_elapsed_time < CLOCK_TOTAL_TIME) {
                countdown_elapsed_time++;
                if (countdown_elapsed_time >= CLOCK_TOTAL_TIME && !countdown_message_shown) {
                    countdown_message_shown = TRUE;
                    
                    // 检查是否处于番茄钟模式 - 使用任何非IDLE状态表示番茄钟正在运行
                    if (current_pomodoro_phase != POMODORO_PHASE_IDLE && POMODORO_TIMES_COUNT > 0) {
                        // 显示超时消息
                        ShowLocalizedNotification(hwnd, L"时间到！", L"Time's up!");
                        
                        // 移动到下一个时间段
                        current_pomodoro_time_index++;
                        
                        // 检查是否已经完成了一个完整的循环
                        if (current_pomodoro_time_index >= POMODORO_TIMES_COUNT) {
                            // 重置索引回到第一个时间
                            current_pomodoro_time_index = 0;
                            
                            // 增加完成的循环计数
                            complete_pomodoro_cycles++;
                            
                            // 检查是否已完成所有配置的循环次数
                            if (complete_pomodoro_cycles >= POMODORO_LOOP_COUNT) {
                                // 已完成所有循环次数，结束番茄钟
                                countdown_elapsed_time = 0;
                                countdown_message_shown = FALSE;
                                CLOCK_TOTAL_TIME = 0;
                                
                                // 重置番茄钟状态
                                current_pomodoro_phase = POMODORO_PHASE_IDLE;
                                
                                // 显示完成提示并停止计时器
                                ShowLocalizedNotification(hwnd, L"所有番茄钟循环完成！", L"All Pomodoro cycles completed!");
                                
                                // 切换到空闲状态 - 添加以下代码
                                CLOCK_COUNT_UP = FALSE;       // 确保不是正计时模式
                                CLOCK_SHOW_CURRENT_TIME = FALSE; // 确保不是显示当前时间模式
                                message_shown = TRUE;         // 标记消息已显示
                                
                                // 强制重绘窗口以清除显示
                                InvalidateRect(hwnd, NULL, TRUE);
                                KillTimer(hwnd, 1);
                                return TRUE;
                            }
                        }
                        
                        // 设置下一个时间段的倒计时
                        CLOCK_TOTAL_TIME = POMODORO_TIMES[current_pomodoro_time_index];
                        countdown_elapsed_time = 0;
                        countdown_message_shown = FALSE;
                        
                        // 如果是新一轮的第一个时间段，显示循环提示
                        if (current_pomodoro_time_index == 0 && complete_pomodoro_cycles > 0) {
                            wchar_t cycleMsg[100];
                            swprintf(cycleMsg, 100, 
                                     GetLocalizedString(L"开始第 %d 轮番茄钟", L"Starting Pomodoro cycle %d"), 
                                     complete_pomodoro_cycles + 1);
                            ShowLocalizedNotification(hwnd, cycleMsg, L"");
                        }
                        
                        InvalidateRect(hwnd, NULL, TRUE);
                    } else {
                        // 非番茄钟模式，执行原有的超时动作
                        
                        // 如果超时动作不是打开文件、锁屏、关机或重启，才显示通知消息
                        if (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_OPEN_FILE && 
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_LOCK &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHUTDOWN &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_RESTART) {
                            // 显示超时消息
                            ShowLocalizedNotification(hwnd, L"时间到！", L"Time's up!");
                        }
                        
                        switch (CLOCK_TIMEOUT_ACTION) {
                            case TIMEOUT_ACTION_MESSAGE:
                                // 已经显示了通知，不需要额外操作
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
                            case TIMEOUT_ACTION_SHOW_TIME:
                                // 切换到显示当前时间模式
                                CLOCK_SHOW_CURRENT_TIME = TRUE;
                                CLOCK_COUNT_UP = FALSE;
                                KillTimer(hwnd, 1);
                                SetTimer(hwnd, 1, 1000, NULL);
                                InvalidateRect(hwnd, NULL, TRUE);
                                break;
                            case TIMEOUT_ACTION_COUNT_UP:
                                // 切换到正计时模式并重置
                                CLOCK_COUNT_UP = TRUE;
                                CLOCK_SHOW_CURRENT_TIME = FALSE;
                                countup_elapsed_time = 0;
                                elapsed_time = 0;
                                message_shown = FALSE;
                                countdown_message_shown = FALSE;
                                countup_message_shown = FALSE;
                                
                                // 设置番茄钟状态为空闲
                                CLOCK_IS_PAUSED = FALSE;
                                KillTimer(hwnd, 1);
                                SetTimer(hwnd, 1, 1000, NULL);
                                InvalidateRect(hwnd, NULL, TRUE);
                                break;
                            case TIMEOUT_ACTION_OPEN_WEBSITE:
                                if (strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                                    wchar_t wideUrl[MAX_PATH];
                                    MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_WEBSITE_URL, -1, wideUrl, MAX_PATH);
                                    ShellExecuteW(NULL, L"open", wideUrl, NULL, NULL, SW_NORMAL);
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