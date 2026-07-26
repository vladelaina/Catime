/**
 * @file tray_mouse.c
 * @brief On-demand tray hover cache, low-level hook, and interaction state.
 */

#include "tray_internal.h"
#include "tray/tray_animation_core.h"
#include "log.h"
#include <shellapi.h>

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
        TrayHoverRectCache_RecordQuery(
            &g_trayIconRectCache, SUCCEEDED(hr), &refreshedRect, now,
            ICON_RECT_STALE_GRACE_MS);
    }
    return TrayHoverRectCache_Contains(&g_trayIconRectCache, pt);
}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (IsTrayInteractionSuspended()) {
        return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
    }

    if (nCode >= 0 && wParam == WM_MOUSEWHEEL) {
        MSLLHOOKSTRUCT* mouse = (MSLLHOOKSTRUCT*)lParam;
        if (IsMouseOverTrayIconCached(mouse->pt)) {
            int delta = GET_WHEEL_DELTA_WPARAM(mouse->mouseData);
            BOOL ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            HWND hwndMain = GetValidTrayMainWindow();
            if (hwndMain && PostMessage(
                    hwndMain, CLOCK_WM_TRAY_OPACITY_WHEEL,
                    (WPARAM)(delta > 0 ? 1 : -1),
                    (LPARAM)ctrlPressed)) {
                return 1;
            }
        }
    }
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

BOOL TryReleaseTrayMouseHook(void) {
    if (!g_mouseHook) {
        return TRUE;
    }

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

void InstallTrayMouseHook(void) {
    if (!g_mouseHook && g_hInstance) {
        DWORD now = GetTickCount();
        if (g_lastMouseHookInstallAttemptTick != 0 &&
            (DWORD)(now - g_lastMouseHookInstallAttemptTick) < 1000u) {
            return;
        }
        g_lastMouseHookInstallAttemptTick = now ? now : 1u;
        g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc,
                                        g_hInstance, 0);
        if (g_mouseHook) {
            g_lastMouseHookInstallAttemptTick = 0;
            g_lastMouseHookInstallWarningTick = 0;
        } else {
            DWORD error = GetLastError();
            if (g_lastMouseHookInstallWarningTick == 0 ||
                (DWORD)(now - g_lastMouseHookInstallWarningTick) >= 5000u) {
                LOG_WARNING("Failed to install tray mouse hook (error=%lu)",
                            error);
                g_lastMouseHookInstallWarningTick = now ? now : 1u;
            }
        }
        g_trayIconRectCache.lastQueryTime = 0;
    }
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
        if (g_trayRecreateRetryCount > 0) {
            g_trayRecreateRetryCount--;
        }
        ScheduleTrayRecreateRetry(hwnd);
        return;
    }
    RecreateTaskbarIcon(hwnd, g_hInstance ? g_hInstance :
                        GetModuleHandleW(NULL));
}

BOOL IsTrayMouseHookInstalled(void) {
    return g_mouseHook != NULL;
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
    InterlockedExchange(&g_trayInteractionSuspended,
                        suspended ? 1L : 0L);
    HWND hwndMain = GetValidTrayMainWindow();
    if (suspended) {
        InterlockedExchange(&g_trayTooltipActive, 0);
        if (g_showingOpacityTip) {
            g_showingOpacityTip = FALSE;
            EndTrayOpacityPreview(hwndMain);
        }
        TryReleaseTrayMouseHook();
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

void UninstallTrayMouseHook(void) {
    TryReleaseTrayMouseHook();
    if (g_showingOpacityTip) {
        g_showingOpacityTip = FALSE;
        HWND hwndMain = GetValidTrayMainWindow();
        EndTrayOpacityPreview(hwndMain);
        if (hwndMain && IsTrayTooltipActive()) {
            TrayTipTimerProc(hwndMain, WM_TIMER, TRAY_TIP_TIMER_ID, 0);
        }
    }
    RefreshTrayBackgroundWorkState();
}
