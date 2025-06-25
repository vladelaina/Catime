/**
 * @file tray.c
 * @brief System tray functionality implementation
 * 
 * This file implements the application's system tray operations, including initialization, removal and notification display,
 * as well as the mechanism for automatically restoring the tray icon when Windows Explorer restarts.
 */

#include <windows.h>
#include <shellapi.h>
#include "../include/language.h"
#include "../resource/resource.h"
#include "../include/tray.h"

/// Global notification icon data structure
NOTIFYICONDATAW nid;

/// ID for the TaskbarCreated message
UINT WM_TASKBARCREATED = 0;

/**
 * @brief Register the TaskbarCreated message
 * 
 * Registers the TaskbarCreated message sent by the system, used to receive messages
 * and recreate the tray icon after Windows Explorer restarts. This mechanism ensures
 * that the program still displays the icon normally after the system tray is refreshed.
 */
void RegisterTaskbarCreatedMessage() {
    // Register to receive message sent after Explorer restarts
    WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));
}

/**
 * @brief Initialize the system tray icon
 * @param hwnd Window handle associated with the tray icon
 * @param hInstance Application instance handle
 * 
 * Creates and displays a system tray icon with default settings.
 * The icon will receive messages through the CLOCK_WM_TRAYICON callback.
 * Also ensures that the TaskbarCreated message is registered to support automatic icon recovery.
 */
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CATIME));
    nid.hWnd = hwnd;
    nid.uCallbackMessage = CLOCK_WM_TRAYICON;
    
    // Create tooltip text containing the application name and version number
    wchar_t versionText[128] = {0};
    // Convert version number from UTF-8 to Unicode
    wchar_t versionWide[64] = {0};
    MultiByteToWideChar(CP_UTF8, 0, CATIME_VERSION, -1, versionWide, _countof(versionWide));
    swprintf_s(versionText, _countof(versionText), L"Catime %s", versionWide);
    wcscpy_s(nid.szTip, _countof(nid.szTip), versionText);
    
    Shell_NotifyIconW(NIM_ADD, &nid);
    
    // Ensure TaskbarCreated message is registered
    if (WM_TASKBARCREATED == 0) {
        RegisterTaskbarCreatedMessage();
    }
}

/**
 * @brief Remove the system tray icon
 * 
 * Removes the application's icon from the system tray.
 * Should be called when the application closes to ensure proper release of system resources.
 */
void RemoveTrayIcon(void) {
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

/**
 * @brief Display a notification in the system tray
 * @param hwnd Window handle associated with the notification
 * @param message Text message to display in the notification
 * 
 * Displays a balloon tip notification from the system tray icon.
 * The notification uses NIIF_NONE style (no icon) and times out after 3 seconds.
 * 
 * @note Message text is converted from UTF-8 to Unicode to correctly display various language characters.
 */
void ShowTrayNotification(HWND hwnd, const char* message) {
    NOTIFYICONDATAW nid_notify = {0};
    nid_notify.cbSize = sizeof(NOTIFYICONDATAW);
    nid_notify.hWnd = hwnd;
    nid_notify.uID = CLOCK_ID_TRAY_APP_ICON;
    nid_notify.uFlags = NIF_INFO;
    nid_notify.dwInfoFlags = NIIF_NONE; // Don't display an icon
    nid_notify.uTimeout = 3000;
    
    // Convert UTF-8 string to Unicode
    MultiByteToWideChar(CP_UTF8, 0, message, -1, nid_notify.szInfo, sizeof(nid_notify.szInfo)/sizeof(WCHAR));
    // Keep the title empty
    nid_notify.szInfoTitle[0] = L'\0';
    
    Shell_NotifyIconW(NIM_MODIFY, &nid_notify);
}

/**
 * @brief Recreate the taskbar icon
 * @param hwnd Window handle
 * @param hInstance Instance handle
 * 
 * Recreates the tray icon after Windows Explorer restarts.
 * This function should be called when receiving the TaskbarCreated message to ensure automatic recovery of the tray icon,
 * preventing situations where the program is running but the tray icon has disappeared.
 */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance) {
    // First try to delete any existing old icon
    RemoveTrayIcon();
    
    // Recreate the tray icon
    InitTrayIcon(hwnd, hInstance);
}

/**
 * @brief Update the tray icon and menu
 * @param hwnd Window handle
 * 
 * Updates the tray icon and menu after the application language or settings change.
 * This function first removes the current tray icon and then recreates it,
 * ensuring that the text displayed in the tray menu is consistent with the current language settings.
 */
void UpdateTrayIcon(HWND hwnd) {
    // Get the instance handle
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    
    // Use the recreate taskbar icon function to complete the update
    RecreateTaskbarIcon(hwnd, hInstance);
}