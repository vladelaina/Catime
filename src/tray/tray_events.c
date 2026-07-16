/**
 * @file tray_events.c
 * @brief System tray event handlers and timer control
 */
#include <windows.h>
#include <shellapi.h>
#include "tray/tray_events.h"
#include "tray/tray_menu.h"
#include "tray/tray.h"
#include "color/color.h"
#include "timer/timer.h"
#include "timer/main_timer.h"
#include "language.h"
#include "window_procedure/window_events.h"
#include "window/window_core.h"
#include "timer/timer_events.h"
#include "drawing.h"
#include "audio_player.h"
#include "config.h"
#include "log.h"
#include "../resource/resource.h"
#include <wchar.h>

/* Timer for detecting mouse hover over tray icon */
#define TRAY_HOVER_CHECK_TIMER_ID 42422
#define TRAY_HOVER_CHECK_ACTIVE_INTERVAL_MS 200
#define TRAY_HOVER_CHECK_IDLE_INTERVAL_MS 1000
#define TRAY_HOVER_NEAR_MARGIN_PX 96
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

static UINT_PTR g_hoverCheckTimer = 0;
static UINT g_hoverCheckIntervalMs = 0;
static HWND g_trayEventHwnd = NULL;

static void CALLBACK TrayHoverCheckTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);

static BOOL IsValidTrayEventWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static BOOL SetTrayHoverCheckInterval(HWND hwnd, UINT intervalMs) {
    if (!IsValidTrayEventWindow(hwnd) || !IsTrayIconActive(hwnd)) {
        return FALSE;
    }

    if (g_hoverCheckTimer &&
        g_trayEventHwnd == hwnd &&
        g_hoverCheckIntervalMs == intervalMs) {
        return TRUE;
    }

    UINT_PTR newTimer = SetTimer(hwnd, TRAY_HOVER_CHECK_TIMER_ID,
                                 intervalMs, TrayHoverCheckTimerProc);
    if (!newTimer) {
        LOG_WARNING("Tray hover detection timer creation failed (error=%lu)",
                    GetLastError());
        return FALSE;
    }

    g_hoverCheckTimer = newTimer;
    g_hoverCheckIntervalMs = intervalMs;
    g_trayEventHwnd = hwnd;
    return TRUE;
}

/**
 * @brief Timer callback to check if mouse is over tray icon
 * @note Installs hook when mouse enters, uninstalls when mouse leaves
 */
static void CALLBACK TrayHoverCheckTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)time;

    if (msg != WM_TIMER || id != TRAY_HOVER_CHECK_TIMER_ID) {
        return;
    }

    if (!g_trayEventHwnd ||
        hwnd != g_trayEventHwnd ||
        !IsValidTrayEventWindow(g_trayEventHwnd) ||
        !IsTrayIconActive(g_trayEventHwnd)) {
        if (hwnd) {
            KillTimer(hwnd, TRAY_HOVER_CHECK_TIMER_ID);
        }
        if (hwnd == g_trayEventHwnd) {
            g_hoverCheckTimer = 0;
            g_hoverCheckIntervalMs = 0;
            g_trayEventHwnd = NULL;
            if (IsTrayMouseHookInstalled()) {
                UninstallTrayMouseHook();
            }
            SetTrayTooltipActive(FALSE);
        }
        return;
    }

    if (IsTrayInteractionSuspended()) {
        return;
    }
    
    POINT pt = {0};
    if (!GetCursorPos(&pt)) {
        /* Preserve the last known hover state on a transient input-query
         * failure. A random/uninitialized point could otherwise flap it. */
        return;
    }
    
    BOOL isOverIcon = IsMouseOverTrayIconArea(pt);
    BOOL isNearIcon = isOverIcon ||
                      IsMouseNearTrayIconArea(pt, TRAY_HOVER_NEAR_MARGIN_PX);
    BOOL hookInstalled = IsTrayMouseHookInstalled();
    
    if (isOverIcon && !hookInstalled) {
        /* Mouse entered tray icon - install hook */
        InstallTrayMouseHook();
        hookInstalled = TRUE;
    } else if (!isOverIcon && hookInstalled) {
        /* Mouse left tray icon - uninstall hook */
        UninstallTrayMouseHook();
        hookInstalled = IsTrayMouseHookInstalled();
    }

    SetTrayTooltipActive(isOverIcon);
    SetTrayHoverCheckInterval(hwnd,
                              (isNearIcon || hookInstalled) ?
                                  TRAY_HOVER_CHECK_ACTIVE_INTERVAL_MS :
                                  TRAY_HOVER_CHECK_IDLE_INTERVAL_MS);
}

/**
 * @brief Start tray hover detection timer
 */
static void StartTrayHoverDetection(HWND hwnd) {
    if (!IsValidTrayEventWindow(hwnd) || !IsTrayIconActive(hwnd)) {
        return;
    }

    if (g_hoverCheckTimer &&
        g_trayEventHwnd == hwnd &&
        IsValidTrayEventWindow(g_trayEventHwnd)) {
        return;
    }

    HWND previousHwnd = g_trayEventHwnd;
    UINT_PTR newTimer = SetTimer(hwnd, TRAY_HOVER_CHECK_TIMER_ID,
                                 TRAY_HOVER_CHECK_ACTIVE_INTERVAL_MS,
                                 TrayHoverCheckTimerProc);
    if (!newTimer) {
        LOG_WARNING("Tray hover detection timer creation failed (error=%lu)",
                    GetLastError());
        if (g_hoverCheckTimer && !IsValidTrayEventWindow(g_trayEventHwnd)) {
            g_hoverCheckTimer = 0;
            g_trayEventHwnd = NULL;
        }
        return;
    }

    if (g_hoverCheckTimer && previousHwnd != hwnd &&
        IsValidTrayEventWindow(previousHwnd)) {
        KillTimer(previousHwnd, TRAY_HOVER_CHECK_TIMER_ID);
    }
    g_hoverCheckTimer = newTimer;
    g_hoverCheckIntervalMs = TRAY_HOVER_CHECK_ACTIVE_INTERVAL_MS;
    g_trayEventHwnd = hwnd;
}

/**
 * @brief Stop tray hover detection timer
 * @note Called when tray icon is removed
 */
void StopTrayHoverDetection(void) {
    if (IsValidTrayEventWindow(g_trayEventHwnd)) {
        KillTimer(g_trayEventHwnd, TRAY_HOVER_CHECK_TIMER_ID);
    }
    g_hoverCheckTimer = 0;
    g_hoverCheckIntervalMs = 0;
    g_trayEventHwnd = NULL;
    /* Also uninstall hook if still active */
    if (IsTrayMouseHookInstalled()) {
        UninstallTrayMouseHook();
    }
    SetTrayTooltipActive(FALSE);
}

/**
 * @brief Open URL in default browser
 * @param url Wide-character URL string
 */
static inline void OpenUrlInBrowser(const wchar_t* url) {
    if (!url) return;
    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
}

/**
 * @brief Invalidate window to trigger redraw
 * @param hwnd Window handle
 */
static inline void ForceWindowRedraw(HWND hwnd) {
    if (!IsValidTrayEventWindow(hwnd)) return;
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief Restart timer with new interval
 * @param hwnd Window handle
 * @param timerId Timer identifier
 * @param interval Interval in milliseconds
 */
static inline BOOL RestartTimerWithInterval(HWND hwnd, UINT timerId, UINT interval) {
    if (timerId == TIMER_ID_MAIN) {
        return MainTimer_Start(hwnd, interval);
    }

    KillTimer(hwnd, timerId);
    if (!SetTimer(hwnd, timerId, interval, NULL)) {
        LOG_WARNING("Failed to restart timer %u with interval %u ms (error=%lu)",
                    timerId, interval, GetLastError());
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief Check if timer is active (countdown or count-up)
 * @return TRUE if timer is running (not clock display mode)
 */
static inline BOOL IsTimerActive(void) {
    return !CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || CLOCK_TOTAL_TIME > 0);
}

/**
 * @brief Handle tray icon mouse events
 * @param hwnd Main window handle
 * @param uID Tray icon identifier
 * @param uMouseMsg Mouse message (WM_LBUTTONUP, WM_RBUTTONUP, WM_MOUSEMOVE, etc.)
 * @note Right-click: color menu; Left-click: main context menu
 * @note Hover detection is done via timer polling, not message-based
 */
void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg) {
    if (uID != CLOCK_ID_TRAY_APP_ICON ||
        !IsValidTrayEventWindow(hwnd) ||
        !IsTrayIconActive(hwnd)) {
        return;
    }

    /* Nested tray messages can arrive while a popup menu owns the message
     * loop. Do not restart hover polling until that interaction has ended. */
    BOOL interactionSuspended = IsTrayInteractionSuspended();
    if (!interactionSuspended) {
        StartTrayHoverDetection(hwnd);
    }

    switch (uMouseMsg) {
        case WM_MOUSEMOVE:
        case NIN_POPUPOPEN:
            if (interactionSuspended) break;
            InstallTrayMouseHook();
            SetTrayTooltipActive(TRUE);
            SetTrayHoverCheckInterval(hwnd, TRAY_HOVER_CHECK_ACTIVE_INTERVAL_MS);
            break;

        case NIN_POPUPCLOSE:
            if (interactionSuspended) break;
            /* Explorer can close and recreate its native popup even while the
             * pointer is still over the icon. Let geometry polling decide the
             * actual leave state instead of briefly resuming icon updates. */
            if (!SetTrayHoverCheckInterval(hwnd,
                                           TRAY_HOVER_CHECK_ACTIVE_INTERVAL_MS)) {
                POINT pt = {0};
                if (GetCursorPos(&pt) && !IsMouseOverTrayIconArea(pt)) {
                    SetTrayTooltipActive(FALSE);
                }
            }
            break;

        case WM_RBUTTONUP:
            if (interactionSuspended) break;
            StopNotificationSound();
            SetCursor(LoadCursorW(NULL, IDC_ARROW));
            TryRestorePendingWindowPosition(hwnd);
            SetTrayInteractionSuspended(TRUE);
            ShowColorMenu(hwnd);
            SetTrayInteractionSuspended(FALSE);
            break;
            
        case WM_LBUTTONUP:
            if (interactionSuspended) break;
            StopNotificationSound();
            SetCursor(LoadCursorW(NULL, IDC_ARROW));
            TryRestorePendingWindowPosition(hwnd);
            SetTrayInteractionSuspended(TRUE);
            ShowContextMenu(hwnd);
            SetTrayInteractionSuspended(FALSE);
            break;
            
        default:
            break;
    }
}

/**
 * @brief Toggle timer pause/resume state
 * @param hwnd Main window handle for timer operations
 * @note Preserves milliseconds and manages notification sound state
 */
void TogglePauseResumeTimer(HWND hwnd) {
    if (!IsTimerActive()) {
        return;
    }
    
    /* Use TogglePauseTimer() to properly handle g_target_end_time and g_pause_start_time */
    TogglePauseTimer();
    
    if (CLOCK_IS_PAUSED) {
        CLOCK_LAST_TIME_UPDATE = time(NULL);
        MainTimer_Stop();
        PauseNotificationSound();
    } else {
        if (!RestartTimerWithInterval(hwnd, TIMER_ID_MAIN, GetTimerInterval())) {
            LOG_WARNING("Failed to resume timer; keeping timer paused");
            TogglePauseTimer();
            ForceWindowRedraw(hwnd);
            return;
        }
        ResumeNotificationSound();
    }
    
    ForceWindowRedraw(hwnd);
}

/**
 * @brief Persist startup mode and refresh UI
 * @param hwnd Window handle for redraw
 * @param mode Startup mode ("DEFAULT", "COUNT_UP", "SHOW_TIME", "NO_DISPLAY")
 */
void SetStartupMode(HWND hwnd, const char* mode) {
    if (!WriteConfigStartupMode(mode)) {
        LOG_WARNING("Failed to save startup mode: %s", mode ? mode : "(null)");
        return;
    }
    
    HMENU hMenu = GetMenu(hwnd);
    if (hMenu) {
        ForceWindowRedraw(hwnd);
    }
}

/** @brief Open user guide in browser */
void OpenUserGuide(void) {
    OpenUrlInBrowser(L"https://cati.me/guide");
}

/** @brief Open support page in browser */
void OpenSupportPage(void) {
    OpenUrlInBrowser(L"https://cati.me/support");
}

/** @brief Open Vlaina project page in browser */
void OpenVlainaPage(void) {
    OpenUrlInBrowser(URL_VLAINA);
}

/**
 * @brief Open feedback page (language-aware)
 * @note Chinese users → localized form; others → GitHub Issues
 */
void OpenFeedbackPage(void) {
    const wchar_t* url = (CURRENT_LANGUAGE == APP_LANG_CHINESE_SIMP) 
        ? URL_FEEDBACK 
        : L"https://github.com/vladelaina/Catime/issues";
    
    OpenUrlInBrowser(url);
}
