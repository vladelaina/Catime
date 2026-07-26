/**
 * @file tray_icon_lifecycle.c
 * @brief Tray icon removal and taskbar-recreation handling.
 */

#include "tray_internal.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_icon_lifetime.h"
#include "tray/tray_events.h"
#include "tray/tray_menu_submenus.h"
#include "system_monitor.h"
#include "log.h"
#include <shellapi.h>

void RemoveTrayIconInternal(BOOL finalCleanup) {
    HWND trayHwnd = nid.hWnd;
    BOOL hadActiveTrayItem = g_trayIconActive && trayHwnd;
    if (finalCleanup) {
        g_trayShuttingDown = TRUE;
        CancelTrayRecreateRetry(trayHwnd ? trayHwnd : g_mainHwnd);
    }
    g_trayIconActive = FALSE;

    if (trayHwnd) {
        KillTimer(trayHwnd, TRAY_TIP_TIMER_ID);
        g_trayTipTimerActive = FALSE;
        KillTimer(trayHwnd, TRAY_OPACITY_SAVE_TIMER_ID);
        CompleteTrayOpacityFeedback(trayHwnd, FALSE);
        if (finalCleanup) {
            DiscardPendingTrayOpacitySave();
        } else {
            ReschedulePendingTrayOpacitySave(trayHwnd);
        }
    }

    StopTrayHoverDetection();
    TryReleaseTrayMouseHook();
    if (finalCleanup) {
        SystemMonitor_Shutdown();
        g_traySystemMonitorActive = FALSE;
        CleanupTraySubmenuResources();
        g_trayBackgroundWorkEnabled = FALSE;
    }
    if (hadActiveTrayItem) {
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }
    TrayIconLifetime_ReleaseAll();
    InterlockedExchange(&g_trayTooltipActive, 0);
    g_trayTipTimerActive = FALSE;
    ZeroMemory(&nid, sizeof(nid));
    g_mainHwnd = NULL;
    g_lastTrayTooltip[0] = L'\0';
    TrayHoverRectCache_Reset(&g_trayIconRectCache);
    if (finalCleanup) {
        DiscardPendingTrayOpacitySave();
    }
}

void RemoveTrayIcon(void) {
    RemoveTrayIconInternal(TRUE);
}

void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance) {
    KillTimer(hwnd, TRAY_RECREATE_RETRY_TIMER_ID);
    HWND oldTrayHwnd = nid.hWnd;
    BOOL hadActiveTrayItem = g_trayIconActive && oldTrayHwnd;
    g_trayIconActive = FALSE;
    if (oldTrayHwnd) {
        KillTimer(oldTrayHwnd, TRAY_TIP_TIMER_ID);
        g_trayTipTimerActive = FALSE;
    }
    StopTrayHoverDetection();

    BOOL oldTrayItemDeleted = FALSE;
    if (hadActiveTrayItem) {
        oldTrayItemDeleted = Shell_NotifyIconW(NIM_DELETE, &nid);
    }
    if (oldTrayItemDeleted) {
        TrayIconLifetime_ReleaseAll();
    }
    ZeroMemory(&nid, sizeof(nid));
    InitTrayIconInternal(hwnd, hInstance, FALSE, FALSE, TRUE);
    if (!IsTrayIconActive(hwnd)) {
        KillTimer(hwnd, TRAY_TIP_TIMER_ID);
        g_trayTipTimerActive = FALSE;
        KillTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID);
        CompleteTrayOpacityFeedback(hwnd, FALSE);
        ReschedulePendingTrayOpacitySave(hwnd);
        if (TrayAnimation_IsRunning()) {
            StopTrayAnimation(hwnd);
        }
        return;
    }

    if (TrayAnimation_IsRunning()) {
        TrayAnimation_RefreshCurrentIcon();
    } else {
        StartTrayAnimation(hwnd, TRAY_ANIMATION_DEFAULT_INTERVAL_MS);
    }
    RefreshTrayBackgroundWorkState();
}

void UpdateTrayIcon(HWND hwnd) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    RecreateTaskbarIcon(hwnd, hInstance);
}
