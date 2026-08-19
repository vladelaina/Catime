/**
 * @file drag_scale_scale_core.c
 * @brief Scale math and target application for wheel gestures.
 */

#include "drag_scale_internal.h"
#include "config.h"
#include <math.h>

float ClampScaleFactor(double scale) {
    if (!isfinite(scale)) {
        return scale > 0.0 ? MAX_SCALE_FACTOR : MIN_SCALE_FACTOR;
    }
    if (scale < MIN_SCALE_FACTOR) {
        return MIN_SCALE_FACTOR;
    }
    if (scale > MAX_SCALE_FACTOR) {
        return MAX_SCALE_FACTOR;
    }
    return (float)scale;
}

double CalculateWheelScaleDelta(int delta, int stepPercent,
                                float currentScale) {
    if (delta == 0) {
        return 0.0;
    }
    if (stepPercent <= 0) {
        stepPercent = 1;
    }

    double wheelUnits = (double)delta / (double)WHEEL_DELTA;
    double scaleStep = (double)stepPercent / 100.0;
    double speedFactor = sqrt((double)currentScale);
    if (!isfinite(speedFactor) || speedFactor < 1.0) {
        speedFactor = 1.0;
    }

    double scaleDelta = wheelUnits * scaleStep * speedFactor;
    if (!isfinite(scaleDelta)) {
        return 0.0;
    }
    return scaleDelta;
}

float GetActiveScaleFactor(BOOL pluginMode) {
    return pluginMode ? PLUGIN_FONT_SCALE_FACTOR : CLOCK_FONT_SCALE_FACTOR;
}

void SetActiveScaleFactor(BOOL pluginMode, float scale) {
    if (pluginMode) {
        PLUGIN_FONT_SCALE_FACTOR = scale;
    } else {
        CLOCK_FONT_SCALE_FACTOR = scale;
        CLOCK_WINDOW_SCALE = scale;
    }
}

BOOL ApplyScaleToWindow(HWND hwnd, BOOL pluginMode, float newScale,
                        POINT anchor) {
    float oldScale = GetActiveScaleFactor(pluginMode);
    if (oldScale <= 0.0f || newScale == oldScale) {
        return FALSE;
    }

    /* Scaling becomes the latest placement gesture and owns the next anchor. */
    ClearManualEditPosition();
    SetActiveScaleFactor(pluginMode, newScale);
    SetPendingScaleResizeAnchorWithRatio(
        hwnd, anchor, g_scaleGestureAnchorRatioX,
        g_scaleGestureAnchorRatioY);
    RefreshWindow(hwnd, FALSE);
    return TRUE;
}

void StopScaleApplyTimer(HWND hwnd) {
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

BOOL ApplyPendingScaleTarget(HWND hwnd) {
    if (!g_scaleTargetValid || !IsValidDragScaleWindow(hwnd)) {
        return FALSE;
    }
    if (!CLOCK_EDIT_MODE) {
        StopScaleApplyTimer(hwnd);
        return FALSE;
    }

    return ApplyScaleToWindow(hwnd, g_scaleTargetPluginMode,
                              g_scaleTarget, g_scaleTargetAnchor);
}

static float GetScaleSettleTolerance(float targetScale) {
    float relative = fabsf(targetScale) * SCALE_SETTLE_REL_EPSILON;
    return relative > SCALE_SETTLE_ABS_EPSILON
        ? relative
        : SCALE_SETTLE_ABS_EPSILON;
}

BOOL ApplySmoothedScaleTarget(HWND hwnd, DWORD elapsedMs) {
    if (!g_scaleTargetValid || !CLOCK_EDIT_MODE ||
        !IsValidDragScaleWindow(hwnd)) {
        return FALSE;
    }

    float currentScale = GetActiveScaleFactor(g_scaleTargetPluginMode);
    float tolerance = GetScaleSettleTolerance(g_scaleTarget);
    double remaining = (double)g_scaleTarget - (double)currentScale;
    if (fabs(remaining) <= (double)tolerance) {
        ApplyScaleToWindow(hwnd, g_scaleTargetPluginMode,
                           g_scaleTarget, g_scaleTargetAnchor);
        return TRUE;
    }

    if (elapsedMs == 0) {
        elapsedMs = 1;
    }
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

    ApplyScaleToWindow(hwnd, g_scaleTargetPluginMode,
                       nextScale, g_scaleTargetAnchor);
    return nextScale == g_scaleTarget;
}
