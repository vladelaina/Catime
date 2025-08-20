/**
 * @file tray.c
 * @brief System tray icon management and notification handling
 * Manages tray icon lifecycle, tooltip display, and system notifications
 */
#include <windows.h>
#include <shellapi.h>
#include "../include/language.h"
#include "../resource/resource.h"
#include "../include/tray.h"

/** @brief Global tray icon data structure for Shell_NotifyIcon operations */
NOTIFYICONDATAW nid;

/** @brief Custom Windows message ID for taskbar recreation events */
UINT WM_TASKBARCREATED = 0;

/**
 * @brief Register for taskbar recreation notification messages
 * Enables automatic tray icon restoration when Windows Explorer restarts
 */
void RegisterTaskbarCreatedMessage() {
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
}

/**
 * @brief Initialize and add tray icon to system notification area
 * @param hwnd Main window handle for tray icon callbacks
 * @param hInstance Application instance for icon resource loading
 * Sets up icon, tooltip with version info, and callback message routing
 */
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CATIME));
    nid.hWnd = hwnd;
    nid.uCallbackMessage = CLOCK_WM_TRAYICON;
    
    /** Build version-aware tooltip text for tray icon hover */
    wchar_t versionText[128] = {0};
    wchar_t versionWide[64] = {0};
    MultiByteToWideChar(CP_UTF8, 0, CATIME_VERSION, -1, versionWide, _countof(versionWide));
    swprintf_s(versionText, _countof(versionText), L"Catime %s", versionWide);
    wcscpy_s(nid.szTip, _countof(nid.szTip), versionText);
    
    Shell_NotifyIconW(NIM_ADD, &nid);
    
    /** Ensure taskbar recreation handling is registered */
    if (WM_TASKBARCREATED == 0) {
        RegisterTaskbarCreatedMessage();
    }
}

/**
 * @brief Remove tray icon from system notification area
 * Cleanly removes icon when application exits or hides
 */
void RemoveTrayIcon(void) {
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

/**
 * @brief Display balloon notification from system tray icon
 * @param hwnd Window handle associated with the tray icon
 * @param message UTF-8 encoded message text to display
 * Shows 3-second notification balloon without title or special icons
 */
void ShowTrayNotification(HWND hwnd, const char* message) {
    NOTIFYICONDATAW nid_notify = {0};
    nid_notify.cbSize = sizeof(NOTIFYICONDATAW);
    nid_notify.hWnd = hwnd;
    nid_notify.uID = CLOCK_ID_TRAY_APP_ICON;
    nid_notify.uFlags = NIF_INFO;
    nid_notify.dwInfoFlags = NIIF_NONE;        /**< No special icon in notification */
    nid_notify.uTimeout = 3000;                /**< 3-second display duration */
    
    /** Convert UTF-8 message to wide character format for Windows API */
    MultiByteToWideChar(CP_UTF8, 0, message, -1, nid_notify.szInfo, sizeof(nid_notify.szInfo)/sizeof(WCHAR));
    nid_notify.szInfoTitle[0] = L'\0';         /**< No title, message only */
    
    Shell_NotifyIconW(NIM_MODIFY, &nid_notify);
}

/**
 * @brief Recreate tray icon after taskbar restart or system changes
 * @param hwnd Main window handle
 * @param hInstance Application instance handle
 * Performs clean removal and re-initialization to restore tray icon
 */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance) {
    RemoveTrayIcon();
    InitTrayIcon(hwnd, hInstance);
}

/**
 * @brief Update tray icon by recreation with current window state
 * @param hwnd Main window handle to extract instance from
 * Convenience function that retrieves instance and recreates icon
 */
void UpdateTrayIcon(HWND hwnd) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    RecreateTaskbarIcon(hwnd, hInstance);
}