/**
 * @file tray.c
 * @brief 系统托盘功能实现
 * 
 * 本文件实现了应用程序的系统托盘操作，包括初始化、移除和通知显示功能。
 */

#include <windows.h>
#include <shellapi.h>
#include "../include/language.h"
#include "../resource/resource.h"
#include "../include/tray.h"

/// 全局通知图标数据结构
NOTIFYICONDATAA nid;

/**
 * @brief 初始化系统托盘图标
 * @param hwnd 与托盘图标关联的窗口句柄
 * @param hInstance 应用程序实例句柄
 * 
 * 创建并显示带有默认设置的系统托盘图标。
 * 该图标将通过CLOCK_WM_TRAYICON回调接收消息。
 */
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CATIME));
    nid.hWnd = hwnd;
    nid.uCallbackMessage = CLOCK_WM_TRAYICON;
    strcpy(nid.szTip, "Catime");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

/**
 * @brief 删除系统托盘图标
 * 
 * 从系统托盘中移除应用程序的图标。
 * 应在应用程序关闭时调用。
 */
void RemoveTrayIcon(void) {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

/**
 * @brief 在系统托盘中显示通知
 * @param hwnd 与通知关联的窗口句柄
 * @param message 要在通知中显示的文本消息
 * 
 * 从系统托盘图标显示气球提示通知。
 * 通知使用NIIF_NONE样式（无图标）并在3秒后超时。
 * 
 * @note 消息文本从UTF-8转换为Unicode以正确显示。
 */
void ShowTrayNotification(HWND hwnd, const char* message) {
    NOTIFYICONDATAW nid_notify = {0};
    nid_notify.cbSize = sizeof(NOTIFYICONDATAW);
    nid_notify.hWnd = hwnd;
    nid_notify.uID = CLOCK_ID_TRAY_APP_ICON;
    nid_notify.uFlags = NIF_INFO;
    nid_notify.dwInfoFlags = NIIF_NONE; // 不显示图标
    nid_notify.uTimeout = 3000;
    
    // 将UTF-8字符串转换为Unicode
    MultiByteToWideChar(CP_UTF8, 0, message, -1, nid_notify.szInfo, sizeof(nid_notify.szInfo)/sizeof(WCHAR));
    // 保持标题为空
    nid_notify.szInfoTitle[0] = L'\0';
    
    Shell_NotifyIconW(NIM_MODIFY, &nid_notify);
}