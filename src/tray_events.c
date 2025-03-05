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