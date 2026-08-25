/**
 * @file tray_mouse.c
 * @brief On-demand tray hover cache, recovery timer, and interaction state.
 */

#include "tray_internal.h"
#include "tray/tray_animation_core.h"
#include "log.h"
#include <shellapi.h>

static DWORD g_lastDeferredTrayRecoveryLogTick = 0;
static DWORD g_lastTrayRectFailureLogTick = 0;

BOOL IsMouseOverTrayIconCached(POINT pt) {
    if (!g_trayIconActive || !nid.hWnd) {
        TrayHoverRectCache_Reset(&g_trayIconRectCache);
        return FALSE;
    }

    DWORD now = GetTickCount();
    if (TrayHoverRectCache_NeedsRefresh(
            &g_trayIconRectCache, now, ICON_RECT_CACHE_TIMEOUT_MS)) {
        NOTIFYICONIDENTIFIER iconId = {0};
        iconId.cbSize = sizeof(iconId);
        iconId.hWnd = nid.hWnd;
        iconId.uID = nid.uID;

        RECT refreshedRect = {0};
        HRESULT hr = Shell_NotifyIconGetRect(&iconId, &refreshedRect);
        if (FAILED(hr)) {
            if (g_lastTrayRectFailureLogTick == 0 ||
                (DWORD)(now - g_lastTrayRectFailureLogTick) >= 10000u) {
                g_lastTrayRectFailureLogTick = now ? now : 1u;
                LOG_WARNING("Failed to query tray icon rectangle: hwnd=0x%p "
                            "iconId=%u hr=0x%08lX", nid.hWnd, nid.uID,
                            (unsigned long)hr);
            }
        } else {
            g_lastTrayRectFailureLogTick = 0;
        }
        TrayHoverRectCache_RecordQuery(
            &g_trayIconRectCache, SUCCEEDED(hr), &refreshedRect, now,
            ICON_RECT_STALE_GRACE_MS);
    }
    return TrayHoverRectCache_Contains(&g_trayIconRectCache, pt);
}

void CALLBACK TrayRecreateRetryTimerProc(HWND hwnd, UINT msg,
                                         UINT_PTR id, DWORD time) {
    (void)time;
    if (msg != WM_TIMER || id != TRAY_RECREATE_RETRY_TIMER_ID) {
        return;
    }

    KillTimer(hwnd, TRAY_RECREATE_RETRY_TIMER_ID);
    if (!IsValidTrayMainWindow(hwnd)) {
        CancelTrayRecreateRetry(NULL);
        return;
    }
    if (IsTrayIconActiveForWindow(hwnd)) {
        CancelTrayRecreateRetry(hwnd);
        return;
    }
    if (IsTrayInteractionSuspended()) {
        DWORD now = GetTickCount();
        if (g_lastDeferredTrayRecoveryLogTick == 0 ||
            (DWORD)(now - g_lastDeferredTrayRecoveryLogTick) >= 60000u) {
            g_lastDeferredTrayRecoveryLogTick = now ? now : 1u;
            LOG_WARNING("Tray icon recreation retry deferred while interaction "
                        "is suspended (attempt=%u)",
                        g_trayRecreateRetryCount);
            Tray_LogDiagnosticSnapshot("tray-recovery-deferred", hwnd);
        }
        if (g_trayRecreateRetryCount > 0 &&
            g_trayRecreateRetryCount <
                TRAY_RECREATE_RETRY_MAX_ATTEMPTS) {
            g_trayRecreateRetryCount--;
        }
        ScheduleTrayRecreateRetry(hwnd);
        return;
    }
    g_lastDeferredTrayRecoveryLogTick = 0;
    RecreateTaskbarIcon(hwnd, g_hInstance ? g_hInstance :
                        GetModuleHandleW(NULL));
}

BOOL IsMouseOverTrayIconArea(POINT pt) {
    return IsMouseOverTrayIconCached(pt);
}

BOOL IsMouseNearTrayIconArea(POINT pt, int marginPx) {
    if (marginPx < 0) {
        marginPx = 0;
    }
    IsMouseOverTrayIconCached(pt);
    if (!g_trayIconRectCache.valid ||
        IsRectEmpty(&g_trayIconRectCache.rect)) {
        return FALSE;
    }

    RECT nearRect = g_trayIconRectCache.rect;
    InflateRect(&nearRect, marginPx, marginPx);
    return PtInRect(&nearRect, pt);
}

void SetTrayInteractionSuspended(BOOL suspended) {
    LONG requested = suspended ? 1L : 0L;
    LONG previous = InterlockedExchange(&g_trayInteractionSuspended,
                                        requested);
    if (previous != requested) {
        LOG_INFO("Tray interaction state changed: suspended=%d previous=%d "
                 "hwnd=0x%p", requested != 0, previous != 0,
                 g_mainHwnd);
    }
    HWND hwndMain = GetValidTrayMainWindow();
    if (suspended) {
        InterlockedExchange(&g_trayTooltipActive, 0);
        if (g_showingOpacityTip) {
            g_showingOpacityTip = FALSE;
            EndTrayOpacityPreview(hwndMain);
        }
        if (hwndMain) {
            KillTimer(hwndMain, TRAY_TIP_TIMER_ID);
        }
        g_trayTipTimerActive = FALSE;
        RefreshTrayBackgroundWorkState();
        return;
    }

    if (g_showingOpacityTip) {
        g_showingOpacityTip = FALSE;
        EndTrayOpacityPreview(hwndMain);
    }
    if (hwndMain) {
        POINT cursor = {0};
        BOOL pointerOverTray = GetCursorPos(&cursor) &&
                               IsMouseOverTrayIconArea(cursor);
        if (pointerOverTray) {
            SetTrayTooltipActive(TRUE);
        } else {
            TrayAnimation_RefreshCurrentIcon();
        }
        RefreshTrayBackgroundWorkState();
        if (!pointerOverTray && CurrentTrayIconNeedsBackgroundRefresh()) {
            TrayTipTimerProc(hwndMain, WM_TIMER, TRAY_TIP_TIMER_ID, 0);
        }
    }
}

BOOL IsTrayInteractionSuspended(void) {
    return InterlockedCompareExchange(
        &g_trayInteractionSuspended, 0, 0) != 0;
}
