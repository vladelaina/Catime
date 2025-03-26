/**
 * @file tray_events.c
 * @brief 系统托盘事件处理模块实现
 * 
 * 本模块实现了应用程序系统托盘的事件处理相关功能，
 * 包括托盘图标的各类鼠标事件响应、菜单显示与控制，
 * 以及与计时器相关的暂停/继续、重启等托盘操作。
 * 提供了用户通过托盘图标快速控制应用的核心功能。
 */

#include <windows.h>
#include "../include/tray_events.h"
#include "../include/tray_menu.h"
#include "../include/color.h"
#include "../include/timer.h"

// 声明从配置文件读取超时动作的函数
extern void ReadTimeoutActionFromConfig(void);

/**
 * @brief 处理系统托盘消息
 * @param hwnd 窗口句柄
 * @param uID 托盘图标ID
 * @param uMouseMsg 鼠标消息类型
 * 
 * 处理系统托盘的鼠标事件，根据事件类型显示不同的上下文菜单：
 * - 左键点击：显示主功能上下文菜单，包含计时器控制等功能
 * - 右键点击：显示颜色选择菜单，用于快速更改显示颜色
 * 
 * 所有菜单项的命令处理在对应的菜单显示模块中实现。
 */
void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg) {
    if (uMouseMsg == WM_RBUTTONUP) {
        ShowColorMenu(hwnd);
    }
    else if (uMouseMsg == WM_LBUTTONUP) {
        ShowContextMenu(hwnd);
    }
}

/**
 * @brief 暂停或继续计时器
 * @param hwnd 窗口句柄
 * 
 * 根据当前状态切换计时器的暂停/继续状态：
 * 1. 检查当前是否有计时进行中（倒计时或正计时）
 * 2. 若计时正在进行，则切换暂停/继续状态
 * 3. 暂停时记录当前时间点并停止计时器
 * 4. 继续时重新启动计时器
 * 5. 刷新窗口以反映新状态
 * 
 * 注意：仅当显示计时器（而非当前时间）且计时器活动时才能操作
 */
void PauseResumeTimer(HWND hwnd) {
    // 检查当前是否有计时进行中
    if (!CLOCK_SHOW_CURRENT_TIME && 
        ((!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0) || 
         (CLOCK_COUNT_UP && TRUE))) {
        
        // 切换暂停/继续状态
        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
        
        if (CLOCK_IS_PAUSED) {
            // 如果暂停，记录当前时间点
            CLOCK_LAST_TIME_UPDATE = time(NULL);
            // 停止计时器
            KillTimer(hwnd, 1);
        } else {
            // 如果继续，重新启动计时器
            SetTimer(hwnd, 1, 1000, NULL);
        }
        
        // 更新窗口以反映新状态
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief 重新开始计时器
 * @param hwnd 窗口句柄
 * 
 * 重置计时器到初始状态并继续运行，保持当前计时器类型不变：
 * 1. 读取当前的超时动作设置
 * 2. 根据当前模式（倒计时/正计时）重置计时进度
 * 3. 重置所有相关的计时器状态变量
 * 4. 取消暂停状态，确保计时器正在运行
 * 5. 刷新窗口并确保重置后窗口置顶
 * 
 * 此操作不会改变计时模式或总时长，只会将进度重置为初始状态。
 */
void RestartTimer(HWND hwnd) {
    // 从配置文件读取超时动作设置
    ReadTimeoutActionFromConfig();
    
    // 检查当前是否有计时进行中
    if (!CLOCK_SHOW_CURRENT_TIME && 
        ((!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0) || 
         (CLOCK_COUNT_UP && TRUE))) {
        
        // 保持当前计时器类型(倒计时/正计时/番茄钟)，只重置进度
        if (!CLOCK_COUNT_UP) {
            // 倒计时模式 - 保留CLOCK_TOTAL_TIME，只重置已计时部分
            countdown_elapsed_time = 0;
        } else {
            // 正计时模式 - 从0开始重新计时
            countup_elapsed_time = 0;
        }
        
        // 重置通用计时器状态
        extern int elapsed_time;
        extern BOOL message_shown;
        extern BOOL countdown_message_shown;
        extern BOOL countup_message_shown;
        
        elapsed_time = 0;
        message_shown = FALSE;
        countdown_message_shown = FALSE;
        countup_message_shown = FALSE;
        
        // 取消暂停状态
        CLOCK_IS_PAUSED = FALSE;
        
        // 确保计时器正在运行
        KillTimer(hwnd, 1);
        SetTimer(hwnd, 1, 1000, NULL);
        
        // 更新窗口以反映新状态
        InvalidateRect(hwnd, NULL, TRUE);
        
        // 确保重置后窗口置顶
        extern void HandleWindowReset(HWND hwnd);
        HandleWindowReset(hwnd);
    }
}

/**
 * @brief 设置启动模式
 * @param hwnd 窗口句柄
 * @param mode 启动模式（"COUNTDOWN"/"COUNT_UP"/"SHOW_TIME"/"NO_DISPLAY"）
 * 
 * 设置应用程序的默认启动模式，并将其保存到配置文件：
 * 1. 将选择的模式保存到配置文件
 * 2. 更新菜单项的选中状态以反映当前设置
 * 3. 刷新窗口显示
 * 
 * 启动模式决定了应用启动时的默认行为，例如是显示当前时间还是开始计时。
 * 设置后将在下次启动程序时生效。
 */
void SetStartupMode(HWND hwnd, const char* mode) {
    // 保存启动模式到配置文件
    WriteConfigStartupMode(mode);
    
    // 更新菜单项的选中状态
    HMENU hMenu = GetMenu(hwnd);
    if (hMenu) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}