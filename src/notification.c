/**
 * @file notification.c
 * @brief 通知功能实现
 * 
 * 本文件实现了应用程序的通知相关功能，
 * 包括系统托盘通知等功能。
 */

#include <windows.h>
#include <stdlib.h>
#include "../include/tray.h"
#include "../include/language.h"
#include "../include/notification.h"

/**
 * @brief 显示提示通知
 * @param hwnd 与通知关联的窗口句柄
 * @param message 要在通知中显示的文本消息
 * 
 * 显示一个系统托盘通知，通知内容为传入的消息。
 */
void ShowToastNotification(HWND hwnd, const char* message) {
    // 使用传入的消息参数
    ShowTrayNotification(hwnd, message);
}