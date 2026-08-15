/**
 * @file drag_scale_scale_input.c
 * @brief Mouse-wheel scale gesture input and target scheduling.
 */

#include "drag_scale_internal.h"
#include "config.h"
#include "plugin/plugin_data.h"

/* Mouse wheel scaling: configurable step per notch, anchored at the cursor. */
BOOL HandleScaleWindow(HWND hwnd, int delta) {
    if (!CLOCK_EDIT_MODE) {
        StopScaleApplyTimer(hwnd);
        return FALSE;
    }

    SuppressDragAfterScale();
    BlockDragUntilLeftUp(hwnd);
    CancelDragForScale(hwnd);

    BOOL isPluginMode = PluginData_IsActive();
    float oldScale = isPluginMode ? PLUGIN_FONT_SCALE_FACTOR
                                  : CLOCK_FONT_SCALE_FACTOR;
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
    float baseScale = (g_scaleTargetValid &&
                       g_scaleTargetPluginMode == isPluginMode)
        ? g_scaleTarget
        : oldScale;
    double scaleDelta = CalculateWheelScaleDelta(delta, stepPercent,
                                                 baseScale);
    if (scaleDelta == 0.0) {
        return FALSE;
    }

    float newScale = ClampScaleFactor((double)baseScale + scaleDelta);
    if (newScale == oldScale && !g_scaleTargetValid) {
        return FALSE;
    }

    BOOL hadActiveTimer = g_scaleApplyTimer != 0 &&
                          g_scaleApplyTimerHwnd == hwnd;
    BOOL shouldStartGesture =
        !hadActiveTimer || !g_scaleTargetValid ||
        g_scaleTargetPluginMode != isPluginMode;

    if (!EnsureScaleApplyTimer(hwnd)) {
        if (shouldStartGesture) {
            g_scaleGestureAnchorRatioX = ClampAnchorRatio(
                (double)(cursorPos.x - windowRect.left) / (double)oldWidth);
            g_scaleGestureAnchorRatioY = ClampAnchorRatio(
                (double)(cursorPos.y - windowRect.top) / (double)oldHeight);
        }
        BOOL applied = ApplyScaleToWindow(hwnd, isPluginMode,
                                          newScale, cursorPos);
        if (applied) {
            MarkWindowSettingsDirty(
                WINDOW_SETTINGS_DIRTY_POSITION |
                (isPluginMode ? WINDOW_SETTINGS_DIRTY_PLUGIN_SCALE
                              : WINDOW_SETTINGS_DIRTY_SCALE));
            ScheduleConfigSave(hwnd);
        }
        return applied;
    }

    if (shouldStartGesture) {
        g_scaleGestureAnchorRatioX = ClampAnchorRatio(
            (double)(cursorPos.x - windowRect.left) / (double)oldWidth);
        g_scaleGestureAnchorRatioY = ClampAnchorRatio(
            (double)(cursorPos.y - windowRect.top) / (double)oldHeight);
        g_scaleTargetAnchor = cursorPos;
        if (hadActiveTimer) {
            AdvanceScaleGestureSerial();
        }
    }

    g_scaleTargetValid = TRUE;
    g_scaleTargetPluginMode = isPluginMode;
    g_scaleTarget = newScale;
    g_lastScaleWheelTick = GetTickCount();
    MarkWindowSettingsDirty(
        WINDOW_SETTINGS_DIRTY_POSITION |
        (isPluginMode ? WINDOW_SETTINGS_DIRTY_PLUGIN_SCALE
                      : WINDOW_SETTINGS_DIRTY_SCALE));
    ScheduleConfigSave(hwnd);

    if (!hadActiveTimer) {
        ApplySmoothedScaleTarget(hwnd, SCALE_INITIAL_RESPONSE_MS);
    }
    return TRUE;
}
