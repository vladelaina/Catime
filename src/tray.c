/**
 * @file tray.c
 * @brief System tray icon management and notification handling
 * Manages tray icon lifecycle, tooltip display, and system notifications
 */
#include <windows.h>
#include <shellapi.h>
#include <string.h>
#include "../include/language.h"
#include "../resource/resource.h"
#include "../include/tray.h"
#include "../include/tray_animation.h"
#include "../include/system_monitor.h"
#include "../include/config.h"

/** @brief Global tray icon data structure for Shell_NotifyIcon operations */
NOTIFYICONDATAW nid;

/** @brief Custom Windows message ID for taskbar recreation events */
UINT WM_TASKBARCREATED = 0;

/** @brief Timer ID for periodically updating tray tooltip with CPU/MEM */
#define TRAY_TIP_TIMER_ID 42421
/** @brief One-shot timer to quickly update CPU/MEM percent icon after startup (removed after sync sampling) */

/**
 * @brief TimerProc to refresh tray tooltip text with version + CPU/MEM usage.
 */
static void CALLBACK TrayTipTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)msg; (void)id; (void)time;

    float cpu = 0.0f, mem = 0.0f;
    SystemMonitor_GetUsage(&cpu, &mem);
    {
        // If using percent icon mode and value is still zero at startup, force a refresh once
        const char* animNow = GetCurrentAnimationName();
        if (animNow && (_stricmp(animNow, "__cpu__") == 0 || _stricmp(animNow, "__mem__") == 0)) {
            float chosen = (_stricmp(animNow, "__cpu__") == 0) ? cpu : mem;
            if ((int)(chosen + 0.5f) == 0) {
                SystemMonitor_ForceRefresh();
                SystemMonitor_GetUsage(&cpu, &mem);
            }
        }
    }

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

    /** Append animation speed line (English), unless current animation is logo/cpu/mem */
    {
        BOOL showSpeed = TRUE;
        const char* anim = GetCurrentAnimationName();
        if (anim && (_stricmp(anim, "__logo__") == 0 || _stricmp(anim, "__cpu__") == 0 || _stricmp(anim, "__mem__") == 0)) {
            showSpeed = FALSE;
        }
        if (anim && showSpeed) {
            size_t alen = strlen(anim);
            if (alen >= 4) {
                const char* suf4 = anim + (alen - 4);
                if (_stricmp(suf4, ".ico") == 0 || _stricmp(suf4, ".png") == 0 ||
                    _stricmp(suf4, ".bmp") == 0 || _stricmp(suf4, ".jpg") == 0 ||
                    _stricmp(suf4, ".tif") == 0) {
                    showSpeed = FALSE; /** static single image selection */
                }
            }
            if (alen >= 5 && showSpeed) {
                const char* suf5 = anim + (alen - 5);
                if (_stricmp(suf5, ".jpeg") == 0 || _stricmp(suf5, ".tiff") == 0) {
                    showSpeed = FALSE; /** static single image selection */
                }
            }
        }
        if (!showSpeed) {
            NOTIFYICONDATAW n = {0};
            n.cbSize = sizeof(n);
            n.hWnd = nid.hWnd;
            n.uID = nid.uID;
            n.uFlags = NIF_TIP;
            wcsncpy_s(n.szTip, _countof(n.szTip), tip, _TRUNCATE);
            Shell_NotifyIconW(NIM_MODIFY, &n);
            /** Ensure percent icon updates in __cpu__/__mem__ modes even when skipping Speed line */
            extern void TrayAnimation_UpdatePercentIconIfNeeded(void);
            TrayAnimation_UpdatePercentIconIfNeeded();
            return;
        }

        double percent = 0.0;
        AnimationSpeedMetric metric = GetAnimationSpeedMetric();
        if (metric == ANIMATION_SPEED_CPU) {
            percent = cpu;
        } else if (metric == ANIMATION_SPEED_TIMER) {
            extern BOOL CLOCK_COUNT_UP;
            extern BOOL CLOCK_SHOW_CURRENT_TIME;
            extern int CLOCK_TOTAL_TIME;
            extern int countdown_elapsed_time;
            if (!CLOCK_SHOW_CURRENT_TIME) {
                if (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0) {
                    double p = (double)countdown_elapsed_time / (double)CLOCK_TOTAL_TIME;
                    if (p < 0.0) p = 0.0; if (p > 1.0) p = 1.0;
                    percent = p * 100.0;
                } else {
                    percent = 0.0;
                }
            } else {
                percent = 0.0;
            }
        } else {
            percent = mem;
        }

        BOOL applyScaling = TRUE;
        if (metric == ANIMATION_SPEED_TIMER) {
            extern BOOL CLOCK_COUNT_UP;
            extern BOOL CLOCK_SHOW_CURRENT_TIME;
            extern int CLOCK_TOTAL_TIME;
            if (CLOCK_SHOW_CURRENT_TIME || CLOCK_COUNT_UP || CLOCK_TOTAL_TIME <= 0) {
                applyScaling = FALSE;
            }
        }

        double scalePercent = 100.0;
        if (applyScaling) {
            scalePercent = GetAnimationSpeedScaleForPercent(percent);
            if (scalePercent <= 0.0) scalePercent = 100.0;
        } else {
            scalePercent = GetAnimationSpeedScaleForPercent(0.0);
            if (scalePercent <= 0.0) scalePercent = 100.0;
        }

        const wchar_t* metricLabel = L"Memory";
        if (metric == ANIMATION_SPEED_CPU) metricLabel = L"CPU";
        else if (metric == ANIMATION_SPEED_TIMER) metricLabel = L"Timer";

        wchar_t extra[128] = {0};
        swprintf_s(extra, _countof(extra), L"\nSpeed · %s %.1f%%", metricLabel, scalePercent);

        wcsncat_s(tip, _countof(tip), extra, _TRUNCATE);
    }

    NOTIFYICONDATAW n = {0};
    n.cbSize = sizeof(n);
    n.hWnd = nid.hWnd;
    n.uID = nid.uID;
    n.uFlags = NIF_TIP;
    wcsncpy_s(n.szTip, _countof(n.szTip), tip, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &n);

    /** Update percent icon if current animation is __cpu__/__mem__ */
    extern void TrayAnimation_UpdatePercentIconIfNeeded(void);
    TrayAnimation_UpdatePercentIconIfNeeded();
}

/**
 * @brief One-shot timer to obtain first non-zero CPU/MEM sample and update percent icon.
 */
/* removed: TrayFirstCpuTimerProc */

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
    extern void ReadPercentIconColorsConfig(void);
    ReadPercentIconColorsConfig();
    
    // Initialize system monitor early so we can render CPU/MEM percent icon immediately
    SystemMonitor_Init();

    PreloadAnimationFromConfig();

    HICON hInitial = NULL;
    {
        const char* animName = GetCurrentAnimationName();
        if (animName && (_stricmp(animName, "__cpu__") == 0 || _stricmp(animName, "__mem__") == 0)) {
            float cpu = 0.0f, mem = 0.0f;
            // Take two samples synchronously to avoid initial 0% (requires a small delta time)
            SystemMonitor_ForceRefresh();
            Sleep(120);
            SystemMonitor_ForceRefresh();
            SystemMonitor_GetUsage(&cpu, &mem);
            int percent = (_stricmp(animName, "__cpu__") == 0) ? (int)(cpu + 0.5f) : (int)(mem + 0.5f);
            if (percent < 0) percent = 0; if (percent > 100) percent = 100;
            hInitial = CreatePercentIcon16(percent);
        } else {
            hInitial = GetInitialAnimationHicon();
        }
    }

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

    /** Start periodic tooltip updates */
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