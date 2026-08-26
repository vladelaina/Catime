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

static BOOL IsTransientTrayDeleteError(DWORD error) {
    return error == ERROR_TIMEOUT ||
           error == ERROR_BUSY ||
           error == ERROR_RETRY ||
           error == (DWORD)E_FAIL ||
           error == (DWORD)RPC_E_CALL_REJECTED;
}

void RemoveTrayIconInternal(BOOL finalCleanup) {
    HWND trayHwnd = nid.hWnd;
    BOOL hadTrayIdentity = trayHwnd &&
                           nid.uID == CLOCK_ID_TRAY_APP_ICON;
    if (finalCleanup) {
        g_trayShuttingDown = TRUE;
        CancelTrayRecreateRetry(trayHwnd ? trayHwnd : g_mainHwnd);
        CancelTrayResumeRefresh(trayHwnd ? trayHwnd : g_mainHwnd);
    }
    g_trayIconActive = FALSE;
    g_trayCallbackRecoveryAllowed = FALSE;
    g_trayCallbackVersion = 0;

    if (trayHwnd) {
        KillTimer(trayHwnd, TRAY_TIP_TIMER_ID);
        g_trayTipTimerActive = FALSE;
        StopTrayHealthCheck(trayHwnd);
        KillTimer(trayHwnd, TRAY_OPACITY_SAVE_TIMER_ID);
        CompleteTrayOpacityFeedback(trayHwnd, FALSE);
        if (finalCleanup) {
            DiscardPendingTrayOpacitySave();
        } else {
            ReschedulePendingTrayOpacitySave(trayHwnd);
        }
    }

    StopTrayHoverDetection();
    if (finalCleanup) {
        SystemMonitor_Shutdown();
        g_traySystemMonitorActive = FALSE;
        CleanupTraySubmenuResources();
        g_trayBackgroundWorkEnabled = FALSE;
    }
    if (hadTrayIdentity) {
        NOTIFYICONDATAW deleteData = {0};
        deleteData.cbSize = sizeof(deleteData);
        deleteData.hWnd = trayHwnd;
        deleteData.uID = CLOCK_ID_TRAY_APP_ICON;
        if (!Shell_NotifyIconW(NIM_DELETE, &deleteData)) {
            LOG_WARNING("Failed to delete tray icon during cleanup: hwnd=0x%p "
                        "error=%lu", trayHwnd, GetLastError());
        }
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
    if (g_trayShuttingDown || !IsValidTrayMainWindow(hwnd)) {
        LOG_WARNING("Ignoring tray recreation for an invalid or shutting-down "
                    "window: hwnd=0x%p", hwnd);
        return;
    }
    KillTimer(hwnd, TRAY_RECREATE_RETRY_TIMER_ID);
    CancelTrayResumeRefresh(hwnd);
    g_trayCallbackRecoveryAllowed = FALSE;
    HWND oldTrayHwnd = nid.hWnd;
    BOOL hadTrayIdentity = oldTrayHwnd &&
                           nid.uID == CLOCK_ID_TRAY_APP_ICON;
    g_trayIconActive = FALSE;
    g_trayCallbackVersion = 0;
    if (oldTrayHwnd) {
        KillTimer(oldTrayHwnd, TRAY_TIP_TIMER_ID);
        g_trayTipTimerActive = FALSE;
        StopTrayHealthCheck(oldTrayHwnd);
    }
    StopTrayHoverDetection();

    BOOL oldTrayItemDeleted = FALSE;
    if (hadTrayIdentity) {
        NOTIFYICONDATAW deleteData = {0};
        deleteData.cbSize = sizeof(deleteData);
        deleteData.hWnd = oldTrayHwnd;
        deleteData.uID = CLOCK_ID_TRAY_APP_ICON;
        oldTrayItemDeleted = Shell_NotifyIconW(NIM_DELETE, &deleteData);
        if (!oldTrayItemDeleted) {
            DWORD deleteError = GetLastError();
            LOG_WARNING("Failed to remove old tray icon before recreation: "
                        "hwnd=0x%p error=%lu", oldTrayHwnd, deleteError);
            /* Explorer can time out while it is rebuilding the notification
             * area after resume.  Do not issue NIM_ADD against that half-ready
             * state: keep the stable identity and let the retry timer try the
             * delete/add pair again after Explorer has settled. */
            if (IsTransientTrayDeleteError(deleteError)) {
                g_trayCallbackRecoveryAllowed = TRUE;
                ScheduleTrayRecreateRetry(hwnd);
                RefreshTrayBackgroundWorkState();
                return;
            }
            LOG_INFO("Old tray icon was not removable; continuing with a new "
                     "registration (error=%lu)", deleteError);
        }
    }
    if (oldTrayItemDeleted) {
        TrayIconLifetime_ReleaseAll();
    }
    ZeroMemory(&nid, sizeof(nid));
    InitTrayIconInternal(hwnd, hInstance, FALSE, FALSE, TRUE);
    if (!IsTrayIconActive(hwnd)) {
        LOG_WARNING("Tray icon recreation did not produce an active icon: hwnd=0x%p",
                    hwnd);
        Tray_LogDiagnosticSnapshot("tray-recreation-inactive", hwnd);
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
