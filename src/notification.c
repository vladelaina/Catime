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
 * 显示一个系统托盘通知，通知内容为"时间到了!"或其本地化版本。
 */
void ShowToastNotification(HWND hwnd, const char* message) {
    const wchar_t* timeUpMsg = GetLocalizedString(L"时间到了!", L"Time's up!");

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, timeUpMsg, -1, NULL, 0, NULL, NULL);
    char* utf8Msg = (char*)malloc(size_needed);
    WideCharToMultiByte(CP_UTF8, 0, timeUpMsg, -1, utf8Msg, size_needed, NULL, NULL);

    ShowTrayNotification(hwnd, utf8Msg);
    free(utf8Msg);
}