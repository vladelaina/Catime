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
    // 反转暂停状态
    CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
    
    // 如果暂停，记录当前时间
    if (CLOCK_IS_PAUSED) {
        CLOCK_LAST_TIME_UPDATE = time(NULL);
    }
    
    // 更新窗口以反映新状态
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief 重新开始计时器
 * @param hwnd 窗口句柄
 * 
 * 重置计时器到初始状态并继续运行
 */
void RestartTimer(HWND hwnd) {
    if (CLOCK_COUNT_UP) {
        // 重置正计时
        countup_elapsed_time = 0;
    } else {
        // 重置倒计时
        countdown_elapsed_time = 0;
    }
    
    // 确保计时器不处于暂停状态
    CLOCK_IS_PAUSED = FALSE;
    
    // 更新窗口以反映新状态
    InvalidateRect(hwnd, NULL, TRUE);
}