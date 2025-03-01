#ifndef CLOCK_TRAY_H
#define CLOCK_TRAY_H

#include <windows.h>

// 系统托盘消息ID
#define CLOCK_WM_TRAYICON (WM_USER + 2)

// 系统托盘图标ID
#define CLOCK_ID_TRAY_APP_ICON 1001

// 初始化系统托盘图标
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance);

// 删除系统托盘图标
void RemoveTrayIcon(void);

// 显示系统托盘通知
void ShowTrayNotification(HWND hwnd, const char* message);

#endif // CLOCK_TRAY_H