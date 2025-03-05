/**
 * @file tray_events.h
 * @brief 系统托盘事件处理接口
 * 
 * 本文件定义了应用程序系统托盘事件处理相关的函数接口，
 * 包括托盘图标的点击事件处理等功能。
 */

#ifndef CLOCK_TRAY_EVENTS_H
#define CLOCK_TRAY_EVENTS_H

#include <windows.h>

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
void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg);

#endif // CLOCK_TRAY_EVENTS_H