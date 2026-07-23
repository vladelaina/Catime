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

/** @brief Update tray icon by recreation */
void UpdateTrayIcon(HWND hwnd) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    RecreateTaskbarIcon(hwnd, hInstance);
}

/**
 * @brief Install mouse hook for tray wheel events
 * @note Called when mouse enters tray icon area (NIN_POPUPOPEN)
 */
void InstallTrayMouseHook(void) {
    if (!g_mouseHook && g_hInstance) {
        DWORD now = GetTickCount();
        if (g_lastMouseHookInstallAttemptTick != 0 &&
            (DWORD)(now - g_lastMouseHookInstallAttemptTick) < 1000u) {
            return;
        }
        g_lastMouseHookInstallAttemptTick = now ? now : 1u;
        g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, g_hInstance, 0);
        if (g_mouseHook) {
            g_lastMouseHookInstallAttemptTick = 0;
            g_lastMouseHookInstallWarningTick = 0;
        } else {
            DWORD error = GetLastError();
            if (g_lastMouseHookInstallWarningTick == 0 ||
                (DWORD)(now - g_lastMouseHookInstallWarningTick) >= 5000u) {
                LOG_WARNING("Failed to install tray mouse hook (error=%lu)", error);
                g_lastMouseHookInstallWarningTick = now ? now : 1u;
            }
        }
        /* Invalidate position cache to get fresh coordinates */
        g_trayIconRectCache.lastQueryTime = 0;
    }
}

static void CALLBACK TrayRecreateRetryTimerProc(HWND hwnd, UINT msg,
                                                UINT_PTR id, DWORD time) {
    (void)time;
    if (msg != WM_TIMER || id != TRAY_RECREATE_RETRY_TIMER_ID) return;

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

    RecreateTaskbarIcon(hwnd, g_hInstance ? g_hInstance : GetModuleHandleW(NULL));
}

/**
 * @brief Check if mouse hook is currently installed
 * @return TRUE if hook is installed
 */
BOOL IsTrayMouseHookInstalled(void) {
    return g_mouseHook != NULL;
}

/**
 * @brief Check if mouse is over tray icon area
 * @param pt Mouse position in screen coordinates
 * @return TRUE if mouse is over tray icon
 */
BOOL IsMouseOverTrayIconArea(POINT pt) {
    return IsMouseOverTrayIconCached(pt);
}

BOOL IsMouseNearTrayIconArea(POINT pt, int marginPx) {
    if (marginPx < 0) {
        marginPx = 0;
    }

    IsMouseOverTrayIconCached(pt);
    if (!g_trayIconRectCache.valid || IsRectEmpty(&g_trayIconRectCache.rect)) {
        return FALSE;
    }

    RECT nearRect = g_trayIconRectCache.rect;
    InflateRect(&nearRect, marginPx, marginPx);
    return PtInRect(&nearRect, pt);
}

void SetTrayInteractionSuspended(BOOL suspended) {
    InterlockedExchange(&g_trayInteractionSuspended, suspended ? 1L : 0L);

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
    return InterlockedCompareExchange(&g_trayInteractionSuspended, 0, 0) != 0;
}

/**
 * @brief Uninstall mouse hook for tray wheel events
 * @note Called when mouse leaves tray icon area (NIN_POPUPCLOSE)
 */
void UninstallTrayMouseHook(void) {
    TryReleaseTrayMouseHook();
    /* Reset opacity tip mode when mouse leaves */
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
