/**
 * @file tray.c
 * @brief 系统托盘功能实现
 * 
 * 本文件实现了应用程序的系统托盘操作，包括初始化、移除和通知显示功能，
 * 以及处理Windows资源管理器重启时托盘图标自动恢复的机制。
 */

#include <windows.h>
#include <shellapi.h>
#include "../include/language.h"
#include "../resource/resource.h"
#include "../include/tray.h"

/// 全局通知图标数据结构
NOTIFYICONDATAW nid;

/// 记录TaskbarCreated消息的ID
UINT WM_TASKBARCREATED = 0;

/**
 * @brief 注册TaskbarCreated消息
 * 
 * 注册系统发送的TaskbarCreated消息，用于在Windows资源管理器重启后
 * 能够接收到消息并重新创建托盘图标。此机制确保程序在系统托盘刷新后
 * 仍然正常显示图标。
 */
void RegisterTaskbarCreatedMessage() {
    // 注册接收资源管理器重启后发送的消息
    WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));
}

/**
 * @brief 初始化系统托盘图标
 * @param hwnd 与托盘图标关联的窗口句柄
 * @param hInstance 应用程序实例句柄
 * 
 * 创建并显示带有默认设置的系统托盘图标。
 * 该图标将通过CLOCK_WM_TRAYICON回调接收消息。
 * 同时确保已注册TaskbarCreated消息，以支持托盘图标的自动恢复。
 */
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CATIME));
    nid.hWnd = hwnd;
    nid.uCallbackMessage = CLOCK_WM_TRAYICON;
    
    // 创建包含应用名称和版本号的提示文本
    wchar_t versionText[128] = {0};
    // 将版本号从UTF-8转换为Unicode
    wchar_t versionWide[64] = {0};
    MultiByteToWideChar(CP_UTF8, 0, CATIME_VERSION, -1, versionWide, _countof(versionWide));
    swprintf_s(versionText, _countof(versionText), L"Catime %s", versionWide);
    wcscpy_s(nid.szTip, _countof(nid.szTip), versionText);
    
    Shell_NotifyIconW(NIM_ADD, &nid);
    
    // 确保已注册TaskbarCreated消息
    if (WM_TASKBARCREATED == 0) {
        RegisterTaskbarCreatedMessage();
    }
}

/**
 * @brief 删除系统托盘图标
 * 
 * 从系统托盘中移除应用程序的图标。
 * 应在应用程序关闭时调用，确保系统资源的正确释放。
 */
void RemoveTrayIcon(void) {
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

/**
 * @brief 在系统托盘中显示通知
 * @param hwnd 与通知关联的窗口句柄
 * @param message 要在通知中显示的文本消息
 * 
 * 从系统托盘图标显示气球提示通知。
 * 通知使用NIIF_NONE样式（无图标）并在3秒后超时。
 * 
 * @note 消息文本从UTF-8转换为Unicode以正确显示各种语言字符。
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

/**
 * @brief 重新创建托盘图标
 * @param hwnd 窗口句柄
 * @param hInstance 实例句柄
 * 
 * 在Windows资源管理器重启后重新创建托盘图标。
 * 应在收到TaskbarCreated消息时调用此函数，确保托盘图标自动恢复，
 * 避免出现程序运行但托盘图标消失的情况。
 */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance) {
    // 首先尝试删除可能存在的旧图标
    RemoveTrayIcon();
    
    // 重新创建托盘图标
    InitTrayIcon(hwnd, hInstance);
}

/**
 * @brief 更新托盘图标和菜单
 * @param hwnd 窗口句柄
 * 
 * 在应用程序语言或设置更改后更新托盘图标和菜单。
 * 此函数先移除当前的托盘图标，然后重新创建它，
 * 确保托盘菜单显示的文本与当前语言设置一致。
 */
void UpdateTrayIcon(HWND hwnd) {
    // 获取实例句柄
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    
    // 使用重新创建托盘图标函数来完成更新
    RecreateTaskbarIcon(hwnd, hInstance);
}