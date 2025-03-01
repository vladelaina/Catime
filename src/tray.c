#include <windows.h>
#include <shellapi.h>
#include "../include/language.h"
#include "../resource/resource.h"
#include "../include/tray.h"

NOTIFYICONDATAA nid;

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
    NOTIFYICONDATAW nid_notify = {0};
    nid_notify.cbSize = sizeof(NOTIFYICONDATAW);
    nid_notify.hWnd = hwnd;
    nid_notify.uID = CLOCK_ID_TRAY_APP_ICON;
    nid_notify.uFlags = NIF_INFO;
    nid_notify.dwInfoFlags = NIIF_NONE; // 修改为NIIF_NONE，不显示图标
    nid_notify.uTimeout = 3000;
    
    // 正确处理Unicode字符串转换
    MultiByteToWideChar(CP_UTF8, 0, message, -1, nid_notify.szInfo, sizeof(nid_notify.szInfo)/sizeof(WCHAR));
    // 不设置标题，保持为空字符串
    nid_notify.szInfoTitle[0] = L'\0';
    
    Shell_NotifyIconW(NIM_MODIFY, &nid_notify);
}