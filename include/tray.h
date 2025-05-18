/**
 * @file tray.h
 * @brief 系统托盘功能接口
 * 
 * 本文件定义了应用程序的系统托盘操作接口，包括初始化、移除和通知显示功能。
 */

#ifndef TRAY_H
#define TRAY_H

#include <windows.h>

/// @name 系统托盘消息常量
/// @{
#define CLOCK_WM_TRAYICON (WM_USER + 2)  ///< 自定义托盘图标消息ID
/// @}

/// @name 系统托盘标识常量
/// @{
#define CLOCK_ID_TRAY_APP_ICON 1001      ///< 托盘图标标识ID
/// @}

/// TaskbarCreated消息ID
extern UINT WM_TASKBARCREATED;

/**
 * @brief 注册TaskbarCreated消息
 * 
 * 注册系统发送的TaskbarCreated消息，用于在资源管理器重启后重新创建托盘图标
 */
void RegisterTaskbarCreatedMessage(void);

/**
 * @brief 初始化系统托盘图标
 * @param hwnd 与托盘图标关联的窗口句柄
 * @param hInstance 应用程序实例句柄
 * 
 * 创建并显示带有默认设置的系统托盘图标。
 * 该图标将通过CLOCK_WM_TRAYICON回调接收消息。
 */
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance);

/**
 * @brief 删除系统托盘图标
 * 
 * 从系统托盘中移除应用程序的图标。
 * 应在应用程序关闭时调用。
 */
void RemoveTrayIcon(void);

/**
 * @brief 在系统托盘中显示通知
 * @param hwnd 与通知关联的窗口句柄
 * @param message 要在通知中显示的文本消息
 * 
 * 从系统托盘图标显示气球提示通知。
 * 通知使用NIIF_NONE样式（无图标）并在3秒后超时。
 */
void ShowTrayNotification(HWND hwnd, const char* message);

/**
 * @brief 重新创建托盘图标
 * @param hwnd 窗口句柄
 * @param hInstance 实例句柄
 * 
 * 在Windows资源管理器重启后重新创建托盘图标。
 * 应在收到TaskbarCreated消息时调用此函数。
 */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance);

/**
 * @brief 更新托盘图标和菜单
 * @param hwnd 窗口句柄
 * 
 * 在应用程序语言或设置更改后更新托盘图标和菜单。
 * 用于确保托盘菜单显示的文本与当前语言设置一致。
 */
void UpdateTrayIcon(HWND hwnd);

#endif // TRAY_H