#include <windows.h>
#include <shellapi.h>
#include "../include/language.h"
#include "../resource/resource.h"
#include "../include/tray.h"

NOTIFYICONDATA nid;

// 初始化系统托盘图标
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

// 删除系统托盘图标
void RemoveTrayIcon(void) {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

// 显示系统托盘通知
void ShowTrayNotification(HWND hwnd, const char* message) {
    NOTIFYICONDATA nid_notify = {0};
    nid_notify.cbSize = sizeof(NOTIFYICONDATA);
    nid_notify.hWnd = hwnd;
    nid_notify.uID = CLOCK_ID_TRAY_APP_ICON;
    nid_notify.uFlags = NIF_INFO;
    nid_notify.dwInfoFlags = NIIF_INFO;
    nid_notify.uTimeout = 3000;
    
    MultiByteToWideChar(CP_UTF8, 0, message, -1, 
                        (LPWSTR)nid_notify.szInfo, 
                        sizeof(nid_notify.szInfo)/sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, "Catime", -1, 
                        (LPWSTR)nid_notify.szInfoTitle, 
                        sizeof(nid_notify.szInfoTitle)/sizeof(WCHAR));
    
    Shell_NotifyIcon(NIM_MODIFY, &nid_notify);
}