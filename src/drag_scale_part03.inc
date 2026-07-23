    UINT desiredInterval = GetScaleApplyInterval(hwnd);
    if (desiredInterval == g_scaleApplyIntervalMs) {
        return;
    }

    UINT_PTR updatedTimer = SetTimer(hwnd,
                                     SCALE_APPLY_TIMER_ID,
                                     desiredInterval,
                                     (TIMERPROC)ScaleApplyTimerProc);
    if (updatedTimer != 0) {
        g_scaleApplyTimer = updatedTimer;
        g_scaleApplyIntervalMs = desiredInterval;
    }
}

static VOID CALLBACK ScaleApplyTimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD dwTime) {
    (void)dwTime;

    if (msg != WM_TIMER ||
        idEvent != SCALE_APPLY_TIMER_ID ||
        hwnd != g_scaleApplyTimerHwnd) {
        return;
    }

    DWORD now = GetTickCount();
    DWORD elapsedMs = g_lastScaleApplyTick != 0
        ? TickElapsedMs(now, g_lastScaleApplyTick)
        : SCALE_APPLY_INTERVAL_MS;
    g_lastScaleApplyTick = now;
    BOOL targetReached = ApplySmoothedScaleTarget(hwnd, elapsedMs);
    if (!targetReached) {
        RefreshScaleApplyTimerInterval(hwnd);
    }
    BOOL inputIdle = TickElapsedMs(now, g_lastScaleWheelTick) >=
                     SCALE_APPLY_IDLE_STOP_MS;
    if (!g_scaleTargetValid ||
        !CLOCK_EDIT_MODE ||
        (inputIdle && targetReached)) {
        if (g_scaleTargetValid && CLOCK_EDIT_MODE &&
            inputIdle && targetReached) {
            ScheduleConfigSave(hwnd);
        }
        StopScaleApplyTimer(hwnd);
    }
}

static BOOL EnsureScaleApplyTimer(HWND hwnd) {
    if (!IsValidDragScaleWindow(hwnd)) return FALSE;
    if (g_scaleApplyTimer != 0 && g_scaleApplyTimerHwnd == hwnd) return TRUE;

    StopScaleApplyTimer(hwnd);
    UINT interval = GetScaleApplyInterval(hwnd);
    g_scaleApplyTimer = SetTimer(hwnd,
                                 SCALE_APPLY_TIMER_ID,
                                 interval,
                                 (TIMERPROC)ScaleApplyTimerProc);
    if (!g_scaleApplyTimer) {
        return FALSE;
    }

    g_scaleApplyTimerHwnd = hwnd;
    g_scaleApplyIntervalMs = interval;
    g_lastScaleApplyTick = GetTickCount();
    AdvanceScaleGestureSerial();
    return TRUE;
}

static void SetPendingScaleResizeAnchorWithRatio(HWND hwnd, POINT anchor,
                                                 double ratioX, double ratioY) {
    g_pendingScaleResizeAnchorValid = TRUE;
    g_pendingScaleResizeAnchorHwnd = hwnd;
    g_pendingScaleResizeAnchor = anchor;
    g_pendingScaleResizeAnchorRatioX = ClampAnchorRatio(ratioX);
    g_pendingScaleResizeAnchorRatioY = ClampAnchorRatio(ratioY);
    g_pendingScaleResizeAnchorPostScale = FALSE;
    g_pendingScaleResizeAnchorUntilTick = 0;
}

BOOL GetPendingScaleResizeAnchor(HWND hwnd, POINT* anchor) {
    if (g_pendingScaleResizeAnchorPostScale &&
        !IsPostScaleResizeAnchorActive(hwnd)) {
        ForceClearPendingScaleResizeAnchor();
    }

    if (!anchor ||
        !g_pendingScaleResizeAnchorValid ||
        g_pendingScaleResizeAnchorHwnd != hwnd) {
        return FALSE;
    }

    *anchor = g_pendingScaleResizeAnchor;
    return TRUE;
}

BOOL GetPendingScaleResizeAnchorInfo(HWND hwnd, POINT* anchor, double* ratioX, double* ratioY) {
    if (!GetPendingScaleResizeAnchor(hwnd, anchor)) {
        return FALSE;
    }

    if (ratioX) *ratioX = g_pendingScaleResizeAnchorRatioX;
    if (ratioY) *ratioY = g_pendingScaleResizeAnchorRatioY;
    return TRUE;
}

BOOL IsScaleWindowGestureActive(HWND hwnd) {
    return CLOCK_EDIT_MODE &&
           g_scaleApplyTimer != 0 &&
           g_scaleApplyTimerHwnd == hwnd &&
           IsValidDragScaleWindow(hwnd);
}

void FinalizeScaleWindowGestureForManualMove(HWND hwnd) {
    if (!CLOCK_EDIT_MODE || !IsValidDragScaleWindow(hwnd)) {
        return;
    }

    ApplyPendingScaleTarget(hwnd);
    StopScaleApplyTimer(hwnd);
    ForceClearPendingScaleResizeAnchor();
}

DWORD GetScaleWindowGestureSerial(HWND hwnd) {
    if (!IsScaleWindowGestureActive(hwnd)) {
        return 0;
    }

    return g_scaleGestureSerial;
}

DWORD GetScaleWindowVisualSerial(HWND hwnd) {
    DWORD activeSerial = GetScaleWindowGestureSerial(hwnd);
    if (activeSerial != 0) {
        return activeSerial;
    }

    if (g_pendingScaleResizeAnchorValid &&
        g_pendingScaleResizeAnchorHwnd == hwnd &&
        IsPostScaleResizeAnchorActive(hwnd)) {
        return g_scaleGestureSerial;
    }

    return 0;
}

void ClearPendingScaleResizeAnchor(HWND hwnd) {
    if (g_scaleApplyTimer != 0 &&
        (!hwnd || g_scaleApplyTimerHwnd == hwnd)) {
        return;
    }

    if (!g_pendingScaleResizeAnchorValid ||
        (hwnd && g_pendingScaleResizeAnchorHwnd != hwnd)) {
        return;
    }

    if (IsPostScaleResizeAnchorActive(hwnd)) {
        return;
    }

    ForceClearPendingScaleResizeAnchor();
}

void ConsumePendingScaleResizeAnchor(HWND hwnd) {
    if (!g_pendingScaleResizeAnchorValid ||
        (hwnd && g_pendingScaleResizeAnchorHwnd != hwnd)) {
        return;
    }

    ForceClearPendingScaleResizeAnchor();
}

static void ForceClearPendingScaleResizeAnchor(void) {
    g_pendingScaleResizeAnchorValid = FALSE;
    g_pendingScaleResizeAnchorHwnd = NULL;
    g_pendingScaleResizeAnchor.x = 0;
    g_pendingScaleResizeAnchor.y = 0;
    g_pendingScaleResizeAnchorRatioX = 0.5;
    g_pendingScaleResizeAnchorRatioY = 0.5;
    g_pendingScaleResizeAnchorPostScale = FALSE;
    g_pendingScaleResizeAnchorUntilTick = 0;
}

static BOOL IsPostScaleResizeAnchorActive(HWND hwnd) {
    if (!g_pendingScaleResizeAnchorValid ||
        !g_pendingScaleResizeAnchorPostScale ||
        (hwnd && g_pendingScaleResizeAnchorHwnd != hwnd)) {
        return FALSE;
    }

    if ((LONG)(GetTickCount() - g_pendingScaleResizeAnchorUntilTick) < 0) {
        return TRUE;
    }

    return FALSE;
}

static VOID CALLBACK ConfigSaveTimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD dwTime) {
    (void)dwTime;

    if (msg != WM_TIMER ||
        idEvent != TIMER_ID_CONFIG_SAVE ||
        hwnd != g_configSaveTimerHwnd ||
        !IsValidDragScaleWindow(hwnd)) {
        return;
    }

    KillTimer(hwnd, TIMER_ID_CONFIG_SAVE);
    g_configSaveTimer = 0;
    g_configSaveTimerHwnd = NULL;
    SaveWindowSettings(hwnd);
}

/* Debouncing: Only save after operations stop for CONFIG_SAVE_DELAY_MS */
void ScheduleConfigSave(HWND hwnd) {
    if (!IsValidDragScaleWindow(hwnd)) return;

    /* This API is reached only by explicit drag/keyboard/scale gestures. */
    CLOCK_WINDOW_POSITION_MANUAL = TRUE;

    if (g_configSaveTimer != 0) {
        HWND timerHwnd = g_configSaveTimerHwnd ? g_configSaveTimerHwnd : hwnd;
        if (IsValidDragScaleWindow(timerHwnd)) {
            KillTimer(timerHwnd, TIMER_ID_CONFIG_SAVE);
        }
        g_configSaveTimer = 0;
        g_configSaveTimerHwnd = NULL;
    }

    g_configSaveTimer = SetTimer(hwnd, TIMER_ID_CONFIG_SAVE,
                                 CONFIG_SAVE_DELAY_MS,
                                 (TIMERPROC)ConfigSaveTimerProc);
    if (g_configSaveTimer) {
        g_configSaveTimerHwnd = hwnd;
    } else {
        SaveWindowSettings(hwnd);
    }
}

void CancelScheduledConfigSave(HWND hwnd) {
    if (g_configSaveTimer == 0) return;

    HWND timerHwnd = g_configSaveTimerHwnd ? g_configSaveTimerHwnd : hwnd;
    if (IsValidDragScaleWindow(timerHwnd)) {
        KillTimer(timerHwnd, TIMER_ID_CONFIG_SAVE);
    }
    g_configSaveTimer = 0;
    g_configSaveTimerHwnd = NULL;
}

void StartDragWindow(HWND hwnd) {
    if (!CLOCK_EDIT_MODE) return;

    if (CLOCK_IS_DRAGGING) {
        return;
    }

    if (IsDragBlockedUntilLeftUp()) {
        return;
    }

    if (IsScaleWindowGestureActive(hwnd) || IsDragSuppressedAfterScale()) {
        return;
    }

    SetCapture(hwnd);
    if (GetCapture() != hwnd) {
        LOG_WARNING("Failed to capture mouse for edit-mode drag");
        return;
    }

    POINT cursorPos;
    if (!GetCursorPos(&cursorPos) ||
        !SetDragAnchorFromCurrentWindow(hwnd, cursorPos)) {
        ReleaseCapture();
        LOG_WARNING("Failed to initialize edit-mode drag anchor");
        return;
    }

    /* A manual drag supersedes any short-lived cursor anchor left by scaling. */
    if (g_pendingScaleResizeAnchorValid) {
        LOG_DEBUG("Discarding stale scale resize anchor before manual drag");
    }
    ForceClearPendingScaleResizeAnchor();

    CLOCK_IS_DRAGGING = TRUE;
    CLOCK_LAST_MOUSE_POS = cursorPos;
    StopDragApplyTimer(hwnd);
    ResetDragApplyThrottle();
    StopDrawingRenderAnimationTimer(hwnd);
}

BOOL TryStartDragWindowFromMouseMove(HWND hwnd) {
    if (!CLOCK_EDIT_MODE || CLOCK_IS_DRAGGING) return FALSE;
    if (!IsLeftButtonPhysicallyDown()) {
        IsDragBlockedUntilLeftUp();
        return FALSE;
    }
    if (GetCapture() && GetCapture() != hwnd) return FALSE;
    if (IsDragBlockedUntilLeftUp()) return FALSE;
    if (IsScaleWindowGestureActive(hwnd) || IsDragSuppressedAfterScale()) return FALSE;
