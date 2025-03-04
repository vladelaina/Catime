/**
 * @file notification.h
 * @brief 通知功能接口
 * 
 * 本文件定义了应用程序通知相关的函数接口，
 * 包括系统托盘通知等功能。
 */

#ifndef CLOCK_NOTIFICATION_H
#define CLOCK_NOTIFICATION_H

#include <windows.h>

/**
 * @brief 显示提示通知
 * @param hwnd 与通知关联的窗口句柄
 * @param message 要在通知中显示的文本消息
 * 
 * 显示一个系统托盘通知，通知内容为"时间到了!"或其本地化版本。
 */
void ShowToastNotification(HWND hwnd, const char* message);

#endif // CLOCK_NOTIFICATION_H