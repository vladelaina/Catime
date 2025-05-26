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
#include "../include/config.h"
#include <stdio.h>
#include <string.h>
#include "../include/window.h"
#include "audio_player.h"  // 添加头文件引用

// 番茄钟时间列表最大容量
#define MAX_POMODORO_TIMES 10
extern int POMODORO_TIMES[MAX_POMODORO_TIMES]; // 存储所有番茄钟时间
extern int POMODORO_TIMES_COUNT;              // 实际的番茄钟时间数量

// 当前正在执行的番茄钟时间索引
int current_pomodoro_time_index = 0;

// 定义 current_pomodoro_phase 变量，它在 pomodoro.h 中被声明为 extern
POMODORO_PHASE current_pomodoro_phase = POMODORO_PHASE_IDLE;

// 完成的番茄钟循环次数
int complete_pomodoro_cycles = 0;

// 从main.c引入的函数声明
extern void ShowNotification(HWND hwnd, const char* message);

// 从main.c引入的变量声明，用于超时动作
extern int elapsed_time;
extern BOOL message_shown;

// 从config.c引入的自定义消息文本
extern char CLOCK_TIMEOUT_MESSAGE_TEXT[100];
extern char POMODORO_TIMEOUT_MESSAGE_TEXT[100]; // 新增番茄钟专用提示
extern char POMODORO_CYCLE_COMPLETE_TEXT[100];

// 定义ClockState类型
typedef enum {
    CLOCK_STATE_IDLE,
    CLOCK_STATE_COUNTDOWN,
    CLOCK_STATE_COUNTUP,
    CLOCK_STATE_POMODORO
} ClockState;

// 定义PomodoroState类型
typedef struct {
    BOOL isLastCycle;
    int cycleIndex;
    int totalCycles;
} PomodoroState;

extern HWND g_hwnd; // 主窗口句柄
extern ClockState g_clockState;
extern PomodoroState g_pomodoroState;

// 计时器行为函数声明
extern void ShowTrayNotification(HWND hwnd, const char* message);
extern void ShowNotification(HWND hwnd, const char* message);
extern void OpenFileByPath(const char* filePath);
extern void OpenWebsite(const char* url);
extern void SleepComputer(void);
extern void ShutdownComputer(void);
extern void RestartComputer(void);
extern void SetTimeDisplay(void);
extern void ShowCountUp(HWND hwnd);

// 添加外部函数声明到文件开头或函数前
extern void StopNotificationSound(void);

/**
 * @brief 将 UTF-8 编码的 char* 字符串转换为 wchar_t* 字符串
 * @param utf8String 输入的 UTF-8 字符串
 * @return 转换后的 wchar_t* 字符串，使用后需要调用 free() 释放内存。转换失败返回 NULL。
 */
static wchar_t* Utf8ToWideChar(const char* utf8String) {
    if (!utf8String || utf8String[0] == '\0') {
        return NULL; // 返回 NULL 处理空字符串或 NULL 指针
    }
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, NULL, 0);
    if (size_needed == 0) {
        // 转换失败
        return NULL;
    }
    wchar_t* wideString = (wchar_t*)malloc(size_needed * sizeof(wchar_t));
    if (!wideString) {
        // 内存分配失败
        return NULL;
    }
    int result = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, wideString, size_needed);
    if (result == 0) {
        // 转换失败
        free(wideString);
        return NULL;
    }
    return wideString;
}

/**
 * @brief 将宽字符串转换为UTF-8编码的普通字符串并显示通知
 * @param hwnd 窗口句柄
 * @param message 要显示的宽字符串消息 (从配置读取并转换而来)
 */
static void ShowLocalizedNotification(HWND hwnd, const wchar_t* message) {
    // 如果消息为空，则不显示
    if (!message || message[0] == L'\0') {
        return;
    }

    // 计算所需的缓冲区大小
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, message, -1, NULL, 0, NULL, NULL);
    if (size_needed == 0) return; // 转换失败

    // 分配内存
    char* utf8Msg = (char*)malloc(size_needed);
    if (utf8Msg) {
        // 转换为UTF-8
        int result = WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8Msg, size_needed, NULL, NULL);

        if (result > 0) {
            // 显示通知，使用新的ShowNotification函数
            ShowNotification(hwnd, utf8Msg);
            
            // 如果超时动作是MESSAGE，播放通知音频
            if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE) {
                // 读取最新的音频设置
                ReadNotificationSoundConfig();
                
                // 播放通知音频
                PlayNotificationSound(hwnd);
            }
        }

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
            // 在显示当前时间模式下，每次定时器触发都重置上次显示的秒数，确保显示最新时间
            extern int last_displayed_second;
            last_displayed_second = -1; // 强制重置秒数缓存，确保每次都显示最新系统时间
            
            // 刷新显示
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

                    // 在显示通知前，重新读取配置文件中的消息文本
                    ReadNotificationMessagesConfig();
                    // 强制重新读取通知类型配置，确保使用最新设置
                    ReadNotificationTypeConfig();
                    
                    // 变量声明放在分支前面，保证在所有分支中可用
                    wchar_t* timeoutMsgW = NULL;

                    // 检查是否处于番茄钟模式 - 必须同时满足两个条件：
                    // 1. 当前番茄钟阶段不为IDLE 
                    // 2. 番茄钟时间配置有效
                    // 3. 当前倒计时总时长与番茄钟时间列表中的当前索引时间匹配
                    if (current_pomodoro_phase != POMODORO_PHASE_IDLE && 
                        POMODORO_TIMES_COUNT > 0 && 
                        current_pomodoro_time_index < POMODORO_TIMES_COUNT &&
                        CLOCK_TOTAL_TIME == POMODORO_TIMES[current_pomodoro_time_index]) {
                        
                        // 使用番茄钟专用提示消息
                        timeoutMsgW = Utf8ToWideChar(POMODORO_TIMEOUT_MESSAGE_TEXT);
                        
                        // 显示超时消息 (使用配置或默认值)
                        if (timeoutMsgW) {
                            ShowLocalizedNotification(hwnd, timeoutMsgW);
                        } else {
                            ShowLocalizedNotification(hwnd, L"番茄钟时间到！"); // Fallback
                        }
                        
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
                                
                                // 尝试从配置读取并转换完成消息
                                wchar_t* cycleCompleteMsgW = Utf8ToWideChar(POMODORO_CYCLE_COMPLETE_TEXT);
                                // 显示完成提示 (使用配置或默认值)
                                if (cycleCompleteMsgW) {
                                    ShowLocalizedNotification(hwnd, cycleCompleteMsgW);
                                    free(cycleCompleteMsgW); // 释放完成消息内存
                                } else {
                                    ShowLocalizedNotification(hwnd, L"所有番茄钟循环完成！"); // Fallback
                                }
                                
                                // 切换到空闲状态 - 添加以下代码
                                CLOCK_COUNT_UP = FALSE;       // 确保不是正计时模式
                                CLOCK_SHOW_CURRENT_TIME = FALSE; // 确保不是显示当前时间模式
                                message_shown = TRUE;         // 标记消息已显示
                                
                                // 强制重绘窗口以清除显示
                                InvalidateRect(hwnd, NULL, TRUE);
                                KillTimer(hwnd, 1);
                                if (timeoutMsgW) free(timeoutMsgW); // 释放超时消息内存
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
                            // GetLocalizedString 需要重新考虑，或者硬编码英文/中文
                            // 暂时保留原来的方式，但理想情况下也应配置化
                            const wchar_t* formatStr = GetLocalizedString(L"开始第 %d 轮番茄钟", L"Starting Pomodoro cycle %d");
                            swprintf(cycleMsg, 100, formatStr, complete_pomodoro_cycles + 1);
                            ShowLocalizedNotification(hwnd, cycleMsg); // 调用修改后的函数
                        }
                        
                        InvalidateRect(hwnd, NULL, TRUE);
                    } else {
                        // 非番茄钟模式，或者已经切换到普通倒计时模式
                        // 使用普通倒计时提示消息
                        timeoutMsgW = Utf8ToWideChar(CLOCK_TIMEOUT_MESSAGE_TEXT);
                        
                        // 如果超时动作不是打开文件、锁屏、关机或重启，才显示通知消息
                        if (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_OPEN_FILE && 
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_LOCK &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHUTDOWN &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_RESTART &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SLEEP) {
                            // 显示超时消息 (使用配置或默认值)
                            if (timeoutMsgW) {
                                ShowLocalizedNotification(hwnd, timeoutMsgW);
                            } else {
                                ShowLocalizedNotification(hwnd, L"时间到！"); // Fallback
                            }
                        }
                        
                        // 如果当前模式不是番茄钟（手动切换到了普通倒计时），确保不会回到番茄钟循环
                        if (current_pomodoro_phase != POMODORO_PHASE_IDLE &&
                            (current_pomodoro_time_index >= POMODORO_TIMES_COUNT ||
                             CLOCK_TOTAL_TIME != POMODORO_TIMES[current_pomodoro_time_index])) {
                            // 如果已经切换到普通倒计时，重置番茄钟状态
                            current_pomodoro_phase = POMODORO_PHASE_IDLE;
                            current_pomodoro_time_index = 0;
                            complete_pomodoro_cycles = 0;
                        }
                        
                        // 如果是睡眠选项，立即处理，跳过其他处理逻辑
                        if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SLEEP) {
                            // 重置显示并应用更改
                            CLOCK_TOTAL_TIME = 0;
                            countdown_elapsed_time = 0;
                            
                            // 停止计时器
                            KillTimer(hwnd, 1);
                            
                            // 立即强制重绘窗口以清除显示
                            InvalidateRect(hwnd, NULL, TRUE);
                            UpdateWindow(hwnd);
                            
                            // 释放内存
                            if (timeoutMsgW) {
                                free(timeoutMsgW);
                            }
                            
                            // 执行睡眠命令
                            system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
                            return TRUE;
                        }
                        
                        // 如果是关机选项，立即处理，跳过其他处理逻辑
                        if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN) {
                            // 重置显示并应用更改
                            CLOCK_TOTAL_TIME = 0;
                            countdown_elapsed_time = 0;
                            
                            // 停止计时器
                            KillTimer(hwnd, 1);
                            
                            // 立即强制重绘窗口以清除显示
                            InvalidateRect(hwnd, NULL, TRUE);
                            UpdateWindow(hwnd);
                            
                            // 释放内存
                            if (timeoutMsgW) {
                                free(timeoutMsgW);
                            }
                            
                            // 执行关机命令
                            system("shutdown /s /t 0");
                            return TRUE;
                        }
                        
                        // 如果是重启选项，立即处理，跳过其他处理逻辑
                        if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART) {
                            // 重置显示并应用更改
                            CLOCK_TOTAL_TIME = 0;
                            countdown_elapsed_time = 0;
                            
                            // 停止计时器
                            KillTimer(hwnd, 1);
                            
                            // 立即强制重绘窗口以清除显示
                            InvalidateRect(hwnd, NULL, TRUE);
                            UpdateWindow(hwnd);
                            
                            // 释放内存
                            if (timeoutMsgW) {
                                free(timeoutMsgW);
                            }
                            
                            // 执行重启命令
                            system("shutdown /r /t 0");
                            return TRUE;
                        }
                        
                        switch (CLOCK_TIMEOUT_ACTION) {
                            case TIMEOUT_ACTION_MESSAGE:
                                // 已经显示了通知，不需要额外操作
                                break;
                            case TIMEOUT_ACTION_LOCK:
                                LockWorkStation();
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
                                // 停止任何正在播放的通知音频
                                StopNotificationSound();
                                
                                // 切换到显示当前时间模式
                                CLOCK_SHOW_CURRENT_TIME = TRUE;
                                CLOCK_COUNT_UP = FALSE;
                                KillTimer(hwnd, 1);
                                SetTimer(hwnd, 1, 1000, NULL);
                                InvalidateRect(hwnd, NULL, TRUE);
                                break;
                            case TIMEOUT_ACTION_COUNT_UP:
                                // 停止任何正在播放的通知音频
                                StopNotificationSound();
                                
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

                    // 释放转换后的宽字符串内存
                    if (timeoutMsgW) {
                        free(timeoutMsgW);
                    }
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        return TRUE;
    }
    return FALSE;
}

void OnTimerTimeout(HWND hwnd) {
    // 根据timeout action执行不同的行为
    switch (CLOCK_TIMEOUT_ACTION) {
        case TIMEOUT_ACTION_MESSAGE: {
            char utf8Msg[256] = {0};
            
            // 根据当前状态选择不同的提示信息
            if (g_clockState == CLOCK_STATE_POMODORO) {
                // 检查番茄钟是否完成所有循环
                if (g_pomodoroState.isLastCycle && g_pomodoroState.cycleIndex >= g_pomodoroState.totalCycles - 1) {
                    strncpy(utf8Msg, POMODORO_CYCLE_COMPLETE_TEXT, sizeof(utf8Msg) - 1);
                } else {
                    strncpy(utf8Msg, POMODORO_TIMEOUT_MESSAGE_TEXT, sizeof(utf8Msg) - 1);
                }
            } else {
                strncpy(utf8Msg, CLOCK_TIMEOUT_MESSAGE_TEXT, sizeof(utf8Msg) - 1);
            }
            
            utf8Msg[sizeof(utf8Msg) - 1] = '\0'; // 确保字符串以空字符结尾
            
            // 显示自定义提示信息
            ShowNotification(hwnd, utf8Msg);
            
            // 读取最新的音频设置并播放提示音
            ReadNotificationSoundConfig();
            PlayNotificationSound(hwnd);
            
            break;
        }
        
        // ... existing code ...
    }
}

// 添加缺失的全局变量定义（如果其他地方没有定义）
#ifndef STUB_VARIABLES_DEFINED
#define STUB_VARIABLES_DEFINED
// 主窗口句柄
HWND g_hwnd = NULL;
// 当前时钟状态
ClockState g_clockState = CLOCK_STATE_IDLE;
// 番茄钟状态
PomodoroState g_pomodoroState = {FALSE, 0, 1};
#endif

// 添加stub函数定义，如果需要的话
#ifndef STUB_FUNCTIONS_DEFINED
#define STUB_FUNCTIONS_DEFINED
__attribute__((weak)) void SleepComputer(void) {
    // 这是一个弱符号定义，如果其他地方有实际实现，将使用那个实现
    system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
}

__attribute__((weak)) void ShutdownComputer(void) {
    system("shutdown /s /t 0");
}

__attribute__((weak)) void RestartComputer(void) {
    system("shutdown /r /t 0");
}

__attribute__((weak)) void SetTimeDisplay(void) {
    // 设置时间显示的stub实现
}

__attribute__((weak)) void ShowCountUp(HWND hwnd) {
    // 显示正计时的stub实现
}
#endif
