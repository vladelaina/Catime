
static BOOL IsDragBlockedUntilLeftUp(void) {
    if (!g_dragBlockedUntilLeftUp) {
        return FALSE;
    }

    if (!IsLeftButtonPhysicallyDown()) {
        ClearDragBlockUntilLeftUp();
        return FALSE;
    }

    return TRUE;
}

static void BlockDragUntilLeftUp(HWND hwnd) {
    if (g_dragBlockedUntilLeftUp) {
        return;
    }

    if (!IsLeftButtonPhysicallyDown() &&
        !CLOCK_IS_DRAGGING &&
        GetCapture() != hwnd) {
        return;
    }

    g_dragBlockedUntilLeftUp = TRUE;
    g_dragBlockNeedsReleaseCooldown = TRUE;
}

static void ClearDragAnchor(void) {
    g_dragAnchorValid = FALSE;
    g_dragStartCursorPos.x = 0;
    g_dragStartCursorPos.y = 0;
    ZeroMemory(&g_dragStartWindowRect, sizeof(g_dragStartWindowRect));
}

static BOOL SetDragAnchorFromCurrentWindow(HWND hwnd, POINT cursorPos) {
    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        ClearDragAnchor();
        return FALSE;
    }

    g_dragStartCursorPos = cursorPos;
    g_dragStartWindowRect = windowRect;
    g_dragAnchorValid = TRUE;
    return TRUE;
}

static BOOL IsValidDragScaleWindow(HWND hwnd) {
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

static inline void RefreshWindow(HWND hwnd, BOOL eraseBackground) {
    InvalidateRect(hwnd, NULL, eraseBackground);
}

static void RestoreManualTopLeftAfterEditLayout(HWND hwnd,
                                                const RECT* manualRect) {
    if (!manualRect || !IsValidDragScaleWindow(hwnd)) {
        return;
    }

    RECT layoutRect = {0};
    POINT restorePosition = {0};
    if (!GetWindowRect(hwnd, &layoutRect) ||
        !WindowPlacement_GetManualTopLeftRestore(
            manualRect, &layoutRect, &restorePosition)) {
        return;
    }

    LOG_DEBUG("Restoring manual edit position after layout: (%ld, %ld) -> (%ld, %ld)",
              layoutRect.left, layoutRect.top,
              manualRect->left, manualRect->top);
    if (SetWindowPos(hwnd, NULL,
                     restorePosition.x, restorePosition.y,
                     0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
        CLOCK_WINDOW_POS_X = restorePosition.x;
        CLOCK_WINDOW_POS_Y = restorePosition.y;
    }
}

static void FinishDragWindow(HWND hwnd,
                             BOOL saveSettings,
                             BOOL refreshAfterDrag,
                             BOOL applyFinalPosition) {
    if (applyFinalPosition &&
        CLOCK_IS_DRAGGING &&
        g_dragAnchorValid &&
        IsValidDragScaleWindow(hwnd)) {
        POINT finalCursor = {0};
        if (GetCursorPos(&finalCursor)) {
            ApplyDragPositionForCursor(hwnd, finalCursor);
        }
    }
    StopDragApplyTimer(hwnd);

    CLOCK_IS_DRAGGING = FALSE;
    ClearDragAnchor();
    ResetDragApplyThrottle();

    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }

    if (saveSettings && CLOCK_EDIT_MODE) {
        ScheduleConfigSave(hwnd);
    }

    if (refreshAfterDrag && IsValidDragScaleWindow(hwnd)) {
        RefreshWindow(hwnd, FALSE);
    }
}

static inline float ClampScaleFactor(double scale) {
    if (!isfinite(scale)) {
        return scale > 0.0 ? FLT_MAX : MIN_SCALE_FACTOR;
    }
    if (scale < MIN_SCALE_FACTOR) return MIN_SCALE_FACTOR;
    if (scale > (double)FLT_MAX) return FLT_MAX;
    return (float)scale;
}

static double CalculateWheelScaleDelta(int delta, int stepPercent, float currentScale) {
    if (delta == 0) return 0.0;
    if (stepPercent <= 0) stepPercent = 1;

    double wheelUnits = (double)delta / (double)WHEEL_DELTA;
    double scaleStep = (double)stepPercent / 100.0;
    double speedFactor = sqrt((double)currentScale);
    if (!isfinite(speedFactor) || speedFactor < 1.0) speedFactor = 1.0;

    double scaleDelta = wheelUnits * scaleStep * speedFactor;
    if (!isfinite(scaleDelta)) return 0.0;
    return scaleDelta;
}

static double ClampAnchorRatio(double ratio) {
    if (!isfinite(ratio)) return 0.5;
    if (ratio < 0.0) return 0.0;
    if (ratio > 1.0) return 1.0;
    return ratio;
}

static void AdvanceScaleGestureSerial(void) {
    g_scaleGestureSerial++;
    if (g_scaleGestureSerial == 0) {
        g_scaleGestureSerial = 1;
    }
}

static float GetActiveScaleFactor(BOOL pluginMode) {
    return pluginMode ? PLUGIN_FONT_SCALE_FACTOR : CLOCK_FONT_SCALE_FACTOR;
}

static void SetActiveScaleFactor(BOOL pluginMode, float scale) {
    if (pluginMode) {
        PLUGIN_FONT_SCALE_FACTOR = scale;
    } else {
        CLOCK_FONT_SCALE_FACTOR = scale;
        CLOCK_WINDOW_SCALE = scale;
    }
}

static BOOL ApplyScaleToWindow(HWND hwnd, BOOL pluginMode, float newScale, POINT anchor) {
    float oldScale = GetActiveScaleFactor(pluginMode);
    if (oldScale <= 0.0f || newScale == oldScale) return FALSE;

    /* Scaling becomes the latest placement gesture and owns the next anchor. */
    ClearManualEditPosition();
    SetActiveScaleFactor(pluginMode, newScale);
    SetPendingScaleResizeAnchorWithRatio(hwnd,
                                         anchor,
                                         g_scaleGestureAnchorRatioX,
                                         g_scaleGestureAnchorRatioY);

    RefreshWindow(hwnd, FALSE);
    return TRUE;
}

static void StopScaleApplyTimer(HWND hwnd) {
    BOOL keepAnchorForPostScaleResize =
        CLOCK_EDIT_MODE &&
        (g_scaleApplyTimer != 0 || g_scaleTargetValid) &&
        g_pendingScaleResizeAnchorValid &&
        (!hwnd || g_pendingScaleResizeAnchorHwnd == hwnd);

    if (g_scaleApplyTimer != 0 || g_scaleTargetValid) {
        SuppressDragAfterScale();
    }

    HWND timerHwnd = g_scaleApplyTimerHwnd ? g_scaleApplyTimerHwnd : hwnd;
    if (g_scaleApplyTimer != 0 && IsValidDragScaleWindow(timerHwnd)) {
        KillTimer(timerHwnd, SCALE_APPLY_TIMER_ID);
    }

    g_scaleApplyTimer = 0;
    g_scaleApplyTimerHwnd = NULL;
    g_scaleApplyIntervalMs = 0;
    g_scaleTargetValid = FALSE;
    g_scaleTargetPluginMode = FALSE;
    g_scaleTarget = 1.0f;
    g_scaleTargetAnchor.x = 0;
    g_scaleTargetAnchor.y = 0;
    g_scaleGestureAnchorRatioX = 0.5;
    g_scaleGestureAnchorRatioY = 0.5;
    g_lastScaleWheelTick = 0;
    g_lastScaleApplyTick = 0;
    if (keepAnchorForPostScaleResize) {
        DWORD until = GetTickCount() + SCALE_POST_RESIZE_ANCHOR_MS;
        g_pendingScaleResizeAnchorPostScale = TRUE;
        g_pendingScaleResizeAnchorUntilTick = until ? until : 1;
    } else {
        ForceClearPendingScaleResizeAnchor();
    }
}

static BOOL ApplyPendingScaleTarget(HWND hwnd) {
    if (!g_scaleTargetValid || !IsValidDragScaleWindow(hwnd)) {
        return FALSE;
    }
    if (!CLOCK_EDIT_MODE) {
        StopScaleApplyTimer(hwnd);
        return FALSE;
    }

    return ApplyScaleToWindow(hwnd,
                              g_scaleTargetPluginMode,
                              g_scaleTarget,
                              g_scaleTargetAnchor);
}

static float GetScaleSettleTolerance(float targetScale) {
    float relative = fabsf(targetScale) * SCALE_SETTLE_REL_EPSILON;
    return relative > SCALE_SETTLE_ABS_EPSILON
        ? relative
        : SCALE_SETTLE_ABS_EPSILON;
}

static BOOL ApplySmoothedScaleTarget(HWND hwnd, DWORD elapsedMs) {
    if (!g_scaleTargetValid || !CLOCK_EDIT_MODE ||
        !IsValidDragScaleWindow(hwnd)) {
        return FALSE;
    }

    float currentScale = GetActiveScaleFactor(g_scaleTargetPluginMode);
    float tolerance = GetScaleSettleTolerance(g_scaleTarget);
    double remaining = (double)g_scaleTarget - (double)currentScale;
    if (fabs(remaining) <= (double)tolerance) {
        ApplyScaleToWindow(hwnd,
                           g_scaleTargetPluginMode,
                           g_scaleTarget,
                           g_scaleTargetAnchor);
        return TRUE;
    }

    if (elapsedMs == 0) elapsedMs = 1;
    if (elapsedMs > SCALE_FRAME_DELTA_MAX_MS) {
        elapsedMs = SCALE_FRAME_DELTA_MAX_MS;
    }

    double blend = 1.0 - exp(-(double)elapsedMs / SCALE_SMOOTH_RESPONSE_MS);
    if (blend > SCALE_MAX_BLEND_PER_FRAME) {
        blend = SCALE_MAX_BLEND_PER_FRAME;
    }
    double nextValue = (double)currentScale + remaining * blend;
    float nextScale = ClampScaleFactor(nextValue);
    if (fabs((double)g_scaleTarget - (double)nextScale) <=
        (double)tolerance) {
        nextScale = g_scaleTarget;
    }

    ApplyScaleToWindow(hwnd,
                       g_scaleTargetPluginMode,
                       nextScale,
                       g_scaleTargetAnchor);
    return nextScale == g_scaleTarget;
}

static void RefreshScaleApplyTimerInterval(HWND hwnd) {
    if (g_scaleApplyTimer == 0 || g_scaleApplyTimerHwnd != hwnd) {
        return;
    }
