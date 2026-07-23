/**
 * @file tray.c
 * @brief System tray icon management and tooltip updates
 */
#include <windows.h>
#include <shellapi.h>
#include <string.h>
#include "language.h"
#include "../resource/resource.h"
#include "tray/tray.h"
#include "timer/timer.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_animation_loader.h"
#include "tray/tray_animation_percent.h"
#include "tray/tray_icon_lifetime.h"
#include "tray/tray_hover_cache.h"
#include "system_monitor.h"
#include "config.h"
#include "config/config_defaults.h"
#include "preview_display.h"
#include "tray/tray_events.h"
#include "tray/tray_menu_submenus.h"
#include "log.h"

#define TOOLTIP_UPDATE_INTERVAL_MS 1000
#define ICON_RECT_CACHE_TIMEOUT_MS 250  /* Cache tray icon position to reduce Shell API calls */
#define ICON_RECT_STALE_GRACE_MS 2000   /* Tolerate transient Explorer query failures */
#define TRAY_OPACITY_SAVE_TIMER_ID 42423
#define TRAY_RECREATE_RETRY_TIMER_ID 42430
#define TRAY_OPACITY_SAVE_DELAY_MS 400
#define TRAY_OPACITY_SAVE_MAX_RETRIES 3
#define TRAY_RECREATE_RETRY_DELAY_MS 750
#define TRAY_RECREATE_RETRY_MAX_ATTEMPTS 5
#define TRAY_RECREATE_RETRY_RESET_MS 30000
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

/** @brief Global tray icon data for Shell_NotifyIcon */
NOTIFYICONDATAW nid;

/** @brief Taskbar recreation message ID */
UINT WM_TASKBARCREATED = 0;

/** @brief Mouse hook for tray icon wheel events (installed on-demand) */
static HHOOK g_mouseHook = NULL;
static HWND g_mainHwnd = NULL;
static HINSTANCE g_hInstance = NULL;
static BOOL g_trayIconActive = FALSE;
static BOOL g_trayBackgroundWorkEnabled = FALSE;
static volatile LONG g_trayTooltipActive = FALSE;
static BOOL g_trayTipTimerActive = FALSE;
static BOOL g_traySystemMonitorActive = FALSE;
static DWORD g_lastMouseHookInstallAttemptTick = 0;
static DWORD g_lastMouseHookInstallWarningTick = 0;
static DWORD g_lastMouseHookReleaseWarningTick = 0;
static UINT g_trayRecreateRetryCount = 0;
static DWORD g_lastTrayRecreateRetryTick = 0;
static BOOL g_trayRecreateRetryLimitLogged = FALSE;
static BOOL g_trayShuttingDown = FALSE;

/** @brief Opacity tooltip mode flag */
BOOL g_showingOpacityTip = FALSE;

/** @brief Cached tray icon rectangle for performance */
static TrayHoverRectCache g_trayIconRectCache = {0};
static volatile LONG g_trayInteractionSuspended = FALSE;
static int g_pendingOpacityToSave = -1;
static int g_opacityRollbackValue = -1;
static int g_pendingOpacitySaveRetryCount = 0;
static wchar_t g_lastTrayTooltip[256] = {0};

extern void ReadPercentIconColorsConfig(void);

static void InitTrayIconInternal(HWND hwnd, HINSTANCE hInstance,
                                 BOOL preloadAnimation,
                                 BOOL startBackgroundWork,
                                 BOOL useAnimationInitialIcon);
static void RemoveTrayIconInternal(BOOL finalCleanup);
static void CALLBACK TrayOpacitySaveTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);
static void CALLBACK TrayRecreateRetryTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);
static BOOL IsStaticImageFile(const char* filename);
static BOOL IsValidTrayMainWindow(HWND hwnd);
static BOOL IsTrayIconActiveForWindow(HWND hwnd);

static void CancelTrayRecreateRetry(HWND hwnd) {
    if (hwnd) {
        KillTimer(hwnd, TRAY_RECREATE_RETRY_TIMER_ID);
    }
    g_trayRecreateRetryCount = 0;
    g_lastTrayRecreateRetryTick = 0;
    g_trayRecreateRetryLimitLogged = FALSE;
}

static void ScheduleTrayRecreateRetry(HWND hwnd) {
    if (g_trayShuttingDown || !IsValidTrayMainWindow(hwnd) ||
        IsTrayIconActiveForWindow(hwnd)) {
        return;
    }
    DWORD now = GetTickCount();
    if (g_trayRecreateRetryCount >= TRAY_RECREATE_RETRY_MAX_ATTEMPTS &&
        g_lastTrayRecreateRetryTick != 0 &&
        (DWORD)(now - g_lastTrayRecreateRetryTick) >= TRAY_RECREATE_RETRY_RESET_MS) {
        g_trayRecreateRetryCount = 0;
        g_trayRecreateRetryLimitLogged = FALSE;
    }
    if (g_trayRecreateRetryCount >= TRAY_RECREATE_RETRY_MAX_ATTEMPTS) {
        if (!g_trayRecreateRetryLimitLogged) {
            LOG_WARNING("Tray icon recreation retry limit reached");
            g_trayRecreateRetryLimitLogged = TRUE;
        }
        return;
    }
    if (!SetTimer(hwnd, TRAY_RECREATE_RETRY_TIMER_ID,
                  TRAY_RECREATE_RETRY_DELAY_MS,
                  TrayRecreateRetryTimerProc)) {
        LOG_WARNING("Failed to schedule tray icon recreation retry (error=%lu)",
                    GetLastError());
        return;
    }
    g_trayRecreateRetryCount++;
    g_lastTrayRecreateRetryTick = now ? now : 1u;
}

static BOOL IsValidTrayMainWindow(HWND hwnd) {
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

static HWND GetValidTrayMainWindow(void) {
    HWND hwnd = g_mainHwnd;
    if (!IsValidTrayMainWindow(hwnd)) {
        g_mainHwnd = NULL;
        return NULL;
    }
    return hwnd;
}

static BOOL IsTrayIconActiveForWindow(HWND hwnd) {
    return IsValidTrayMainWindow(hwnd) &&
           g_trayIconActive &&
           nid.hWnd == hwnd &&
           nid.uID == CLOCK_ID_TRAY_APP_ICON;
}

BOOL IsTrayIconActive(HWND hwnd) {
    return IsTrayIconActiveForWindow(hwnd);
}

/**
 * @brief Check if mouse is over tray icon (with caching)
 * @note Uses cached position to reduce Shell API calls.
 */
static BOOL IsMouseOverTrayIconCached(POINT pt) {
    if (!g_trayIconActive || !nid.hWnd) {
        TrayHoverRectCache_Reset(&g_trayIconRectCache);
        return FALSE;
    }

    DWORD now = GetTickCount();

    /* Refresh cache if expired */
    if (TrayHoverRectCache_NeedsRefresh(&g_trayIconRectCache, now,
                                        ICON_RECT_CACHE_TIMEOUT_MS)) {
        NOTIFYICONIDENTIFIER iconId = {0};
        iconId.cbSize = sizeof(iconId);
        iconId.hWnd = nid.hWnd;
        iconId.uID = nid.uID;

        RECT refreshedRect = {0};
        HRESULT hr = Shell_NotifyIconGetRect(&iconId, &refreshedRect);
        /* Explorer can transiently reject this query while it is updating
         * notification-area UI. Preserve a recent valid rectangle so one
         * failed sample cannot flap the hover state and replace the icon. */
        TrayHoverRectCache_RecordQuery(&g_trayIconRectCache, SUCCEEDED(hr),
                                       &refreshedRect, now,
                                       ICON_RECT_STALE_GRACE_MS);
    }

    return TrayHoverRectCache_Contains(&g_trayIconRectCache, pt);
}

/**
 * @brief Mouse hook callback for tray icon wheel events
 * @note Only handles wheel events, installed on-demand when mouse enters tray icon
 */
static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (IsTrayInteractionSuspended()) {
        return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
    }

    if (nCode >= 0 && wParam == WM_MOUSEWHEEL) {
        MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;

        /* Only process if mouse is over tray icon */
        if (IsMouseOverTrayIconCached(pMouseStruct->pt)) {
            int delta = GET_WHEEL_DELTA_WPARAM(pMouseStruct->mouseData);
            BOOL ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            HWND hwndMain = GetValidTrayMainWindow();
            if (hwndMain &&
                PostMessage(hwndMain, CLOCK_WM_TRAY_OPACITY_WHEEL,
                            (WPARAM)(delta > 0 ? 1 : -1), (LPARAM)ctrlPressed)) {
                return 1;
            }
        }
    }

    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

static BOOL TryReleaseTrayMouseHook(void) {
    if (!g_mouseHook) return TRUE;

    HHOOK hook = g_mouseHook;
    if (UnhookWindowsHookEx(hook)) {
        g_mouseHook = NULL;
        g_lastMouseHookReleaseWarningTick = 0;
        return TRUE;
    }

    DWORD error = GetLastError();
    if (error == ERROR_INVALID_HOOK_HANDLE) {
        g_mouseHook = NULL;
        g_lastMouseHookReleaseWarningTick = 0;
        return TRUE;
    }

    DWORD now = GetTickCount();
    if (g_lastMouseHookReleaseWarningTick == 0 ||
        (DWORD)(now - g_lastMouseHookReleaseWarningTick) >= 5000u) {
        LOG_WARNING("Failed to uninstall tray mouse hook (error=%lu)", error);
        g_lastMouseHookReleaseWarningTick = now ? now : 1u;
    }
    return FALSE;
}

/** @brief Animation type categories */
typedef enum {
    ANIM_TYPE_CUSTOM,
    ANIM_TYPE_LOGO,
    ANIM_TYPE_CPU,
    ANIM_TYPE_MEMORY,
    ANIM_TYPE_BATTERY,
    ANIM_TYPE_CAPSLOCK,
    ANIM_TYPE_NONE
} AnimationType;

/** @brief Formatted bytes with unit */
typedef struct {
    double value;
    const wchar_t* unit;
} FormattedBytes;

/**
 * @brief Format bytes/sec with unit scaling
 * @return Formatted value (e.g., 1536 → {1.5, "KB/s"})
 */
static FormattedBytes FormatBytesPerSecond(double bytes) {
    FormattedBytes result = { bytes, L"B/s" };

    if (result.value <= 0.0) {
        result.value = 0.0;
        result.unit = L"KB/s";
        return result;
    }

    if (result.value >= 1024.0) { result.value /= 1024.0; result.unit = L"KB/s"; }
    if (result.value >= 1024.0) { result.value /= 1024.0; result.unit = L"MB/s"; }
