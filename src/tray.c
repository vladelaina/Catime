/**
 * @file tray.c
 * @brief System tray functionality implementation
 */

#include <windows.h>
#include <shellapi.h>
#include "../include/language.h"
#include "../resource/resource.h"
#include "../include/tray.h"

NOTIFYICONDATAW nid;
UINT WM_TASKBARCREATED = 0;

/**
 * @brief Register the TaskbarCreated message
 */
void RegisterTaskbarCreatedMessage() {
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
}

/**
 * @brief Initialize the system tray icon
 * @param hwnd Window handle
 * @param hInstance Application instance handle
 */
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CATIME));
    nid.hWnd = hwnd;
    nid.uCallbackMessage = CLOCK_WM_TRAYICON;
    
    wchar_t versionText[128] = {0};
    wchar_t versionWide[64] = {0};
    MultiByteToWideChar(CP_UTF8, 0, CATIME_VERSION, -1, versionWide, _countof(versionWide));
    swprintf_s(versionText, _countof(versionText), L"Catime %s", versionWide);
    wcscpy_s(nid.szTip, _countof(nid.szTip), versionText);
    
    Shell_NotifyIconW(NIM_ADD, &nid);
    if (WM_TASKBARCREATED == 0) {
        RegisterTaskbarCreatedMessage();
    }
}

/**
 * @brief Remove the system tray icon
 */
void RemoveTrayIcon(void) {
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

/**
 * @brief Display a notification in the system tray
 * @param hwnd Window handle
 * @param message Text message to display
 */
void ShowTrayNotification(HWND hwnd, const char* message) {
    NOTIFYICONDATAW nid_notify = {0};
    nid_notify.cbSize = sizeof(NOTIFYICONDATAW);
    nid_notify.hWnd = hwnd;
    nid_notify.uID = CLOCK_ID_TRAY_APP_ICON;
    nid_notify.uFlags = NIF_INFO;
    nid_notify.dwInfoFlags = NIIF_NONE;
    nid_notify.uTimeout = 3000;
    
    MultiByteToWideChar(CP_UTF8, 0, message, -1, nid_notify.szInfo, sizeof(nid_notify.szInfo)/sizeof(WCHAR));
    nid_notify.szInfoTitle[0] = L'\0';
    
    Shell_NotifyIconW(NIM_MODIFY, &nid_notify);
}

/**
 * @brief Recreate the taskbar icon
 * @param hwnd Window handle
 * @param hInstance Instance handle
 */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance) {
    RemoveTrayIcon();
    InitTrayIcon(hwnd, hInstance);
}

/**
 * @brief Update the tray icon and menu
 * @param hwnd Window handle
 */
void UpdateTrayIcon(HWND hwnd) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    RecreateTaskbarIcon(hwnd, hInstance);
}