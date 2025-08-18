#ifndef TRAY_H
#define TRAY_H

#include <windows.h>

#define CLOCK_WM_TRAYICON (WM_USER + 2)

#define CLOCK_ID_TRAY_APP_ICON 1001

extern UINT WM_TASKBARCREATED;

void RegisterTaskbarCreatedMessage(void);

void InitTrayIcon(HWND hwnd, HINSTANCE hInstance);

void RemoveTrayIcon(void);

void ShowTrayNotification(HWND hwnd, const char* message);

void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance);

void UpdateTrayIcon(HWND hwnd);

#endif