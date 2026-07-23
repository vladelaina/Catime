    HandleTimerEvent(hwnd, wp);
    return 0;
}

/**
 * @brief Handle high-precision multimedia timer tick
 * Called from worker thread via PostMessage for smooth milliseconds display
 */
LRESULT HandleMainTimerTick(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    LONG generation = (LONG)wp;
    if (MainTimer_ShouldHandleTick(generation)) {
        /* Delegate to main timer event handler */
        HandleTimerEvent(hwnd, TIMER_ID_MAIN);
    }
    /* Release coalescing gate for the next high-precision tick message. */
    MainTimer_NotifyTickHandled(generation);
    return 0;
}

LRESULT HandleDestroy(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ResetEditExitRightClickState(hwnd);
    StopMenuPreviewTrackingForCommand(hwnd);
    CancelPreview(hwnd);
    UnregisterGlobalHotkeys(hwnd);
    HandleWindowDestroy(hwnd);
    ConfigWatcher_Stop();

    return 0;
}

LRESULT HandleTrayIcon(HWND hwnd, WPARAM wp, LPARAM lp) {
    HandleTrayIconMessage(hwnd, (UINT)wp, (UINT)lp);
    return 0;
}

LRESULT HandleWindowPosChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    const WINDOWPOS* pwp = (const WINDOWPOS*)lp;
    HandleTopmostVisibilityChange(hwnd, pwp);
    if (!(pwp->flags & SWP_NOSIZE)) {
        if (CLOCK_EDIT_MODE) {
            // Region update logic removed - relying on UpdateLayeredWindow alpha channel
        }
    }
    return 0;
}

LRESULT HandleShowWindow(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp) {
        HandleTopmostShownEvent(hwnd);
    } else {
        HandleTopmostHiddenEvent(hwnd);
    }
    return DefWindowProc(hwnd, WM_SHOWWINDOW, wp, lp);
}

LRESULT HandleDisplayChange(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (CLOCK_IS_DRAGGING) {
        return 0;
    }

    if (!BeginSystemPositionChangeGuard(hwnd)) {
        RestoreWindowPositionAfterSystemChange(hwnd);
    }
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
}

LRESULT HandleDpiChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    if (CLOCK_IS_DRAGGING) {
        return 0;
    }

    RECT* suggested = (RECT*)lp;
    if (CLOCK_EDIT_MODE) {
        if (suggested) {
            RECT manualRect = {0};
            POINT position = {suggested->left, suggested->top};
            if (GetWindowRect(hwnd, &manualRect)) {
                POINT restorePosition = {0};
                if (WindowPlacement_GetManualTopLeftRestore(
                        &manualRect, suggested, &restorePosition)) {
                    position = restorePosition;
                }
            }
            SetWindowPos(hwnd, NULL,
                         position.x,
                         position.y,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        ClearPendingSystemPositionRestore();
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    BOOL restoreScheduled = BeginSystemPositionChangeGuard(hwnd);

    if (suggested) {
        SetWindowPos(hwnd, NULL,
                     suggested->left,
                     suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (!restoreScheduled) {
        RestoreWindowPositionAfterSystemChange(hwnd);
    }
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

LRESULT HandleRButtonUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (g_pendingEditExitRightClick) {
        /* End the left-drag transaction before releasing right-click capture. */
        g_pendingEditExitRightClick = FALSE;
        if (CLOCK_EDIT_MODE) {
            EndEditMode(hwnd);
        }
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        SuppressContextMenuBriefly();
        StartEditExitRightClickShield(hwnd);
        return 0;
    }
    if (CLOCK_EDIT_MODE) {
        EndEditMode(hwnd);
        SuppressContextMenuBriefly();
        StartEditExitRightClickShield(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_RBUTTONUP, wp, lp);
}

LRESULT HandleRButtonDown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (CLOCK_EDIT_MODE) {
        g_pendingEditExitRightClick = TRUE;
        SetCapture(hwnd);
        return 0;
    }

    ClearPendingEditExitRightClick(hwnd);

    if (GetKeyState(VK_CONTROL) & 0x8000) {
        ToggleEditMode(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_RBUTTONDOWN, wp, lp);
}

LRESULT HandleContextMenu(HWND hwnd, WPARAM wp, LPARAM lp) {
    BOOL suppressed = CLOCK_EDIT_MODE || IsContextMenuSuppressed();
    if (suppressed) {
        return 0;
    }
    return DefWindowProc(hwnd, WM_CONTEXTMENU, wp, lp);
}

LRESULT HandleCaptureChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    if ((HWND)lp != hwnd && CLOCK_IS_DRAGGING) {
        EndDragWindow(hwnd);
    }
    if ((HWND)lp != hwnd && g_pendingEditExitRightClick) {
        g_pendingEditExitRightClick = FALSE;
    }
    return DefWindowProc(hwnd, WM_CAPTURECHANGED, wp, lp);
}

LRESULT HandleExitMenuLoop(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ResetMenuPreviewTracking(hwnd);
    StartMenuDebounceTimer(hwnd);
    return 0;
}

LRESULT HandleClose(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CancelScheduledConfigSave(hwnd);
    if (CLOCK_EDIT_MODE) {
        EndEditMode(hwnd);
    } else {
        SaveWindowSettings(hwnd);
    }
    HideWindowIntentionally(hwnd);
    DestroyWindow(hwnd);
    return 0;
}

LRESULT HandleSysCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    if (HandleTopmostMinimizeCommand(hwnd, (UINT)wp)) {
        return 0;
    }

    return DefWindowProc(hwnd, WM_SYSCOMMAND, wp, lp);
}

LRESULT HandleSize(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (HandleTopmostSizeEvent(hwnd, wp)) {
        return 0;
    }

    return DefWindowProc(hwnd, WM_SIZE, wp, lp);
}

LRESULT HandleLButtonDblClk(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (!CLOCK_EDIT_MODE) {
        StartEditMode(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_LBUTTONDBLCLK, wp, lp);
}

LRESULT HandleKeyDown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (CLOCK_EDIT_MODE) {
        /* Only process arrow keys in edit mode */
        if (wp != VK_UP && wp != VK_DOWN && wp != VK_LEFT && wp != VK_RIGHT) {
            return DefWindowProc(hwnd, WM_KEYDOWN, wp, lp);
        }

        int step = g_AppConfig.display.move_step_small;
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            step = g_AppConfig.display.move_step_large;
        }

        /* The current WM_KEYDOWN is authoritative; poll the other arrows only
         * to preserve diagonal movement across remote/input-driver variants. */
        BOOL upDown = wp == VK_UP || (GetKeyState(VK_UP) & 0x8000);
        BOOL downDown = wp == VK_DOWN || (GetKeyState(VK_DOWN) & 0x8000);
        BOOL leftDown = wp == VK_LEFT || (GetKeyState(VK_LEFT) & 0x8000);
        BOOL rightDown = wp == VK_RIGHT || (GetKeyState(VK_RIGHT) & 0x8000);
        int dx = (rightDown ? step : 0) - (leftDown ? step : 0);
        int dy = (downDown ? step : 0) - (upDown ? step : 0);

        if (dx != 0 || dy != 0) {
            FinalizeScaleWindowGestureForManualMove(hwnd);
            /* Keyboard placement also supersedes a completed scale anchor. */
            ConsumePendingScaleResizeAnchor(hwnd);

            RECT rect;
            GetWindowRect(hwnd, &rect);
            int newX = rect.left + dx;
            int newY = rect.top + dy;
            if (SetWindowPos(hwnd, NULL, newX, newY, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
                CLOCK_WINDOW_POS_X = newX;
                CLOCK_WINDOW_POS_Y = newY;
                MarkManualEditWindowPosition(hwnd);
                ScheduleConfigSave(hwnd);
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, WM_KEYDOWN, wp, lp);
}

LRESULT HandleHotkey(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (DispatchHotkey(hwnd, (int)wp)) return 0;
    return DefWindowProc(hwnd, WM_HOTKEY, wp, lp);
}

LRESULT HandleCopyData(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lp;
