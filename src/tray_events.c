/**
 * @file tray_events.c
 * @brief 系统托盘事件处理实现
 * 
 * 本文件实现了应用程序系统托盘事件处理相关的功能，
 * 包括托盘图标的点击事件处理等。
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
 * 处理系统托盘的鼠标事件，包括：
 * - 左键点击：显示上下文菜单
 * - 右键点击：显示颜色菜单
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
 * 根据当前状态暂停或继续计时器，并更新相关状态变量
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
 * 重置计时器到初始状态并继续运行
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
 * @param mode 启动模式
 * 
 * 设置应用程序的启动模式并更新到配置文件
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