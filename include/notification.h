/**
 * @file notification.h
 * @brief 应用通知系统接口
 * 
 * 本文件定义了应用程序的通知系统接口，包括自定义样式的弹出通知和系统托盘通知。
 */

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <windows.h>

// 全局变量：通知显示持续时间(毫秒)
extern int NOTIFICATION_TIMEOUT_MS;  // 新增：通知显示时间变量

/**
 * @brief 显示自定义样式的提示通知
 * @param hwnd 父窗口句柄，用于获取应用实例和计算位置
 * @param message 要显示的通知消息文本(UTF-8编码)
 * 
 * 在屏幕右下角显示一个带动画效果的自定义通知窗口
 */
void ShowToastNotification(HWND hwnd, const char* message);

#endif // NOTIFICATION_H