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
#include "../include/tray_animation.h"
#include "../include/system_monitor.h"

/** @brief Global tray icon data structure for Shell_NotifyIcon operations */
NOTIFYICONDATAW nid;

/** @brief Custom Windows message ID for taskbar recreation events */
UINT WM_TASKBARCREATED = 0;

/** @brief Timer ID for periodically updating tray tooltip with CPU/MEM */
#define TRAY_TIP_TIMER_ID 42421

/**
 * @brief TimerProc to refresh tray tooltip text with version + CPU/MEM usage.
 */
static void CALLBACK TrayTipTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)msg; (void)id; (void)time;

    float cpu = 0.0f, mem = 0.0f;
    SystemMonitor_GetUsage(&cpu, &mem);

    float upBps = 0.0f, downBps = 0.0f;
    BOOL hasNet = SystemMonitor_GetNetSpeed(&upBps, &downBps);

    wchar_t tip[256] = {0};
    if (hasNet) {
        double up = (double)upBps;
        double down = (double)downBps;
        const wchar_t* upUnit = L"B/s";
        const wchar_t* downUnit = L"B/s";
        double upVal = up;
        double downVal = down;
        if (upVal >= 1024.0) { upVal /= 1024.0; upUnit = L"KB/s"; }
        if (upVal >= 1024.0) { upVal /= 1024.0; upUnit = L"MB/s"; }
        if (upVal >= 1024.0) { upVal /= 1024.0; upUnit = L"GB/s"; }
        if (downVal >= 1024.0) { downVal /= 1024.0; downUnit = L"KB/s"; }
        if (downVal >= 1024.0) { downVal /= 1024.0; downUnit = L"MB/s"; }
        if (downVal >= 1024.0) { downVal /= 1024.0; downUnit = L"GB/s"; }
        swprintf_s(tip, _countof(tip), L"CPU %.1f%%\nMemory %.1f%%\nUpload %.1f %s\nDownload %.1f %s",
                   cpu, mem, upVal, upUnit, downVal, downUnit);
    } else {
        swprintf_s(tip, _countof(tip), L"CPU %.1f%%\nMemory %.1f%%", cpu, mem);
    }

    NOTIFYICONDATAW n = {0};
    n.cbSize = sizeof(n);
    n.hWnd = nid.hWnd;
    n.uID = nid.uID;
    n.uFlags = NIF_TIP;
    wcsncpy_s(n.szTip, _countof(n.szTip), tip, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

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
    // Preload animation from config and get initial frame icon
    PreloadAnimationFromConfig();
    HICON hInitial = GetInitialAnimationHicon();

    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = hInitial ? hInitial : LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CATIME));
    nid.hWnd = hwnd;
    nid.uCallbackMessage = CLOCK_WM_TRAYICON;
    
    /** Default tooltip text before first monitor refresh */
    wcscpy_s(nid.szTip, _countof(nid.szTip), L"CPU --.-%\nMemory --.-%\nUpload --.- ?/s\nDownload --.- ?/s");
    
    Shell_NotifyIconW(NIM_ADD, &nid);
    
    /** Ensure taskbar recreation handling is registered */
    if (WM_TASKBARCREATED == 0) {
        RegisterTaskbarCreatedMessage();
    }

    /** Initialize system monitor and start periodic tooltip updates */
    SystemMonitor_Init();
    SystemMonitor_SetUpdateIntervalMs(1000);
    SetTimer(hwnd, TRAY_TIP_TIMER_ID, 1000, (TIMERPROC)TrayTipTimerProc);
}

/**
 * @brief Remove tray icon from system notification area
 * Cleanly removes icon when application exits or hides
 */
void RemoveTrayIcon(void) {
    if (nid.hWnd) {
        KillTimer(nid.hWnd, TRAY_TIP_TIMER_ID);
    }
    SystemMonitor_Shutdown();
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