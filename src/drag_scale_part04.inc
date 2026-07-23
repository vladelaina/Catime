
    StartDragWindow(hwnd);
    return CLOCK_IS_DRAGGING;
}

void StartEditMode(HWND hwnd) {
    ClearPendingSystemPositionRestore();
    EnsureWindowVisibleWithTopmostState(hwnd);
    StopScaleApplyTimer(hwnd);
    ClearDragBlockUntilLeftUp();
    ClearManualEditPosition();

    PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
    g_editModeForcedTopmost = FALSE;
    g_editModeTopmostOverride = FALSE;

    if (!CLOCK_WINDOW_TOPMOST) {
        g_editModeForcedTopmost = TRUE;
        SetWindowTopmostTransient(hwnd, TRUE);
    }

    CLOCK_EDIT_MODE = TRUE;

    RefreshWindow(hwnd, TRUE);
    SetBlurBehind(hwnd, TRUE);
    SetClickThrough(hwnd, FALSE);
    InitializeOleDropTarget(hwnd);
    RefreshWindow(hwnd, TRUE);

    SetCursor(LoadCursorW(NULL, IDC_ARROW));
}

void EndEditMode(HWND hwnd) {
    if (!CLOCK_EDIT_MODE) return;

    BOOL hadActiveDrag = CLOCK_IS_DRAGGING;
    if (hadActiveDrag) {
        /* A missed left-button-up must not snap to a later right-click point. */
        FinishDragWindow(hwnd, FALSE, FALSE, IsLeftButtonPhysicallyDown());
        /* Exiting mid-drag is still an explicit manual placement gesture. */
        CLOCK_WINDOW_POSITION_MANUAL = TRUE;
    }
    ApplyPendingScaleTarget(hwnd);
    StopScaleApplyTimer(hwnd);
    ConsumePendingScaleResizeAnchor(hwnd);
    ClearDragBlockUntilLeftUp();

    RECT manualRect = {0};
    BOOL hasManualRect = GetWindowRect(hwnd, &manualRect);
    if (hasManualRect &&
        g_manualEditPositionValid &&
        g_manualEditPositionHwnd == hwnd) {
        OffsetRect(&manualRect,
                   g_manualEditPosition.x - manualRect.left,
                   g_manualEditPosition.y - manualRect.top);
    }
    CLOCK_EDIT_MODE = FALSE;

    SetBlurBehind(hwnd, FALSE);
    SetClickThrough(hwnd, TRUE);
    CleanupOleDropTarget(hwnd);
    CancelScheduledConfigSave(hwnd);

    /* Topmost/desktop ownership changes can trigger another layout pass.
     * Complete them before the one authoritative layout and save below. */
    if (g_editModeForcedTopmost &&
        !g_editModeTopmostOverride &&
        !PREVIOUS_TOPMOST_STATE) {
        SetWindowTopmostTransient(hwnd, FALSE);
        KillTimer(hwnd, TIMER_ID_EDIT_MODE_REFRESH);
    }

    /* Finish normal-mode sizing before persisting the authoritative position. */
    RefreshWindow(hwnd, TRUE);
    UpdateWindow(hwnd);
    if (hasManualRect) {
        RestoreManualTopLeftAfterEditLayout(hwnd, &manualRect);
    }
    SaveWindowSettings(hwnd);
    ClearManualEditPosition();

    if (!WriteConfigColor(CLOCK_TEXT_COLOR)) {
        LOG_WARNING("EndEditMode: failed to persist text color");
    }

    g_editModeForcedTopmost = FALSE;
    g_editModeTopmostOverride = FALSE;
}

void MarkEditModeTopmostOverride(void) {
    if (CLOCK_EDIT_MODE) {
        g_editModeTopmostOverride = TRUE;
    }
}

void EndDragWindow(HWND hwnd) {
    if (!IsLeftButtonPhysicallyDown()) {
        ClearDragBlockUntilLeftUp();
    }
    if (!CLOCK_IS_DRAGGING) return;

    FinishDragWindow(hwnd, TRUE, TRUE, TRUE);
}

static void CancelDragForScale(HWND hwnd) {
    BlockDragUntilLeftUp(hwnd);
    if (!CLOCK_IS_DRAGGING) return;

    FinishDragWindow(hwnd, FALSE, FALSE, FALSE);
}

/* Absolute cursor anchoring keeps movement aligned even when mouse messages coalesce. */
static BOOL HandleDragWindowInternal(HWND hwnd, BOOL leftButtonDown) {
    if (!CLOCK_EDIT_MODE || !CLOCK_IS_DRAGGING) return FALSE;

    if (IsDragBlockedUntilLeftUp()) {
        FinishDragWindow(hwnd, TRUE, TRUE, TRUE);
        return FALSE;
    }

    if (IsScaleWindowGestureActive(hwnd) || IsDragSuppressedAfterScale()) {
        FinishDragWindow(hwnd, FALSE, FALSE, FALSE);
        return FALSE;
    }

    if (!leftButtonDown) {
        FinishDragWindow(hwnd, TRUE, TRUE, TRUE);
        return FALSE;
    }

    if (GetCapture() != hwnd) {
        FinishDragWindow(hwnd, TRUE, TRUE, TRUE);
        return FALSE;
    }

    POINT currentPos;
    if (!GetCursorPos(&currentPos)) {
        return TRUE;
    }

    if (!g_dragAnchorValid &&
        !SetDragAnchorFromCurrentWindow(hwnd, CLOCK_LAST_MOUSE_POS)) {
        return TRUE;
    }

    DWORD now = GetTickCount();
    int deltaFromLastX = currentPos.x - CLOCK_LAST_MOUSE_POS.x;
    int deltaFromLastY = currentPos.y - CLOCK_LAST_MOUSE_POS.y;
    if (deltaFromLastX == 0 && deltaFromLastY == 0) {
        return TRUE;
    }

    if (!ShouldApplyDragMoveNow(now)) {
        if (!EnsureDragApplyTimer(hwnd)) {
            ApplyDragPositionForCursor(hwnd, currentPos);
        }
        return TRUE;
    }

    int newX = g_dragStartWindowRect.left + (currentPos.x - g_dragStartCursorPos.x);
    int newY = g_dragStartWindowRect.top + (currentPos.y - g_dragStartCursorPos.y);

    BOOL moved = SetWindowPos(hwnd, NULL, newX, newY, 0, 0,
                              SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (moved) {
        CLOCK_LAST_MOUSE_POS = currentPos;
        CLOCK_WINDOW_POS_X = newX;
        CLOCK_WINDOW_POS_Y = newY;
        g_lastDragApplyTick = now;
        RecordManualEditPosition(hwnd, newX, newY);
    }

    return TRUE;
}

BOOL HandleDragWindowWithButtonState(HWND hwnd, BOOL leftButtonDown) {
    return HandleDragWindowInternal(hwnd, leftButtonDown);
}

BOOL HandleDragWindow(HWND hwnd) {
    return HandleDragWindowInternal(hwnd, IsLeftButtonPhysicallyDown());
}

/* Mouse wheel scaling: configurable step per notch, anchored at the cursor */
BOOL HandleScaleWindow(HWND hwnd, int delta) {
    if (!CLOCK_EDIT_MODE) {
        StopScaleApplyTimer(hwnd);
        return FALSE;
    }

    SuppressDragAfterScale();
    BlockDragUntilLeftUp(hwnd);
    CancelDragForScale(hwnd);

    BOOL isPluginMode = PluginData_IsActive();
    float oldScale = isPluginMode ? PLUGIN_FONT_SCALE_FACTOR : CLOCK_FONT_SCALE_FACTOR;
    if (oldScale <= 0.0f) {
        return FALSE;
    }

    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int oldWidth = windowRect.right - windowRect.left;
    int oldHeight = windowRect.bottom - windowRect.top;
    if (oldWidth <= 0 || oldHeight <= 0) {
        return FALSE;
    }

    POINT cursorPos;
    if (!GetCursorPos(&cursorPos) ||
        cursorPos.x < windowRect.left || cursorPos.x > windowRect.right ||
        cursorPos.y < windowRect.top || cursorPos.y > windowRect.bottom) {
        cursorPos.x = windowRect.left + oldWidth / 2;
        cursorPos.y = windowRect.top + oldHeight / 2;
    }

    BOOL isCtrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    int stepPercent = isCtrlDown ? g_AppConfig.display.scale_step_fast
                                 : g_AppConfig.display.scale_step_normal;
    float baseScale = (g_scaleTargetValid && g_scaleTargetPluginMode == isPluginMode)
        ? g_scaleTarget
        : oldScale;
    double scaleDelta = CalculateWheelScaleDelta(delta, stepPercent, baseScale);
    if (scaleDelta == 0.0) {
        return FALSE;
    }

    float newScale = ClampScaleFactor((double)baseScale + scaleDelta);

    if (newScale == oldScale && !g_scaleTargetValid) {
        return FALSE;
    }

    BOOL hadActiveTimer = g_scaleApplyTimer != 0 && g_scaleApplyTimerHwnd == hwnd;
    BOOL shouldStartGesture =
        !hadActiveTimer || !g_scaleTargetValid || g_scaleTargetPluginMode != isPluginMode;

    if (!EnsureScaleApplyTimer(hwnd)) {
        if (shouldStartGesture) {
            g_scaleGestureAnchorRatioX =
                ClampAnchorRatio((double)(cursorPos.x - windowRect.left) / (double)oldWidth);
            g_scaleGestureAnchorRatioY =
                ClampAnchorRatio((double)(cursorPos.y - windowRect.top) / (double)oldHeight);
        }
        BOOL applied = ApplyScaleToWindow(hwnd, isPluginMode, newScale, cursorPos);
        if (applied) {
            ScheduleConfigSave(hwnd);
        }
        return applied;
    }

    if (shouldStartGesture) {
        g_scaleGestureAnchorRatioX =
            ClampAnchorRatio((double)(cursorPos.x - windowRect.left) / (double)oldWidth);
        g_scaleGestureAnchorRatioY =
            ClampAnchorRatio((double)(cursorPos.y - windowRect.top) / (double)oldHeight);
        g_scaleTargetAnchor = cursorPos;
        if (hadActiveTimer) {
            AdvanceScaleGestureSerial();
        }
    }

    g_scaleTargetValid = TRUE;
    g_scaleTargetPluginMode = isPluginMode;
    g_scaleTarget = newScale;
    g_lastScaleWheelTick = GetTickCount();
    ScheduleConfigSave(hwnd);

    if (!hadActiveTimer) {
        ApplySmoothedScaleTarget(hwnd, SCALE_INITIAL_RESPONSE_MS);
    }

    return TRUE;
}
