/**
 * @file drawing_render_timer.c
 * @brief Animation timing and render-window limit calculations.
 */

#include "drawing_render_internal.h"

BOOL CalculatePixelCount(int width, int height, size_t* pixelCount) {
    if (!pixelCount || width <= 0 || height <= 0) return FALSE;
    if ((size_t)width > ((size_t)-1) / (size_t)height / sizeof(DWORD)) return FALSE;

    *pixelCount = (size_t)width * (size_t)height;
    return TRUE;
}

UINT GetRenderAnimationTimerInterval(size_t pixelCount, BOOL hasColorTagGradient) {
    if (GetActiveEffect() == EFFECT_TYPE_AQUA) {
        (void)pixelCount;
        return 120u;
    }

    if (hasColorTagGradient) {
        return (pixelCount < 50000u) ? 33u :
               (pixelCount < 200000u) ? 50u : 80u;
    }

    return (pixelCount < 50000u) ? 33u :
           (pixelCount < 200000u) ? 50u :
           (pixelCount < 500000u) ? 80u : 120u;
}

UINT ChooseRenderTimerResolutionMs(UINT intervalMs) {
    UINT resolution = intervalMs / 2u;
    if (resolution < RENDER_TIMER_RESOLUTION_MIN_MS) {
        resolution = RENDER_TIMER_RESOLUTION_MIN_MS;
    }
    if (resolution > RENDER_TIMER_RESOLUTION_MAX_MS) {
        resolution = RENDER_TIMER_RESOLUTION_MAX_MS;
    }
    return resolution;
}

UINT ClampRenderTimerResolutionToDeviceCaps(UINT requestedMs) {
    TIMECAPS caps;
    if (timeGetDevCaps(&caps, sizeof(caps)) != TIMERR_NOERROR) {
        return requestedMs;
    }

    if (requestedMs < caps.wPeriodMin) {
        return caps.wPeriodMin;
    }
    if (requestedMs > caps.wPeriodMax) {
        return caps.wPeriodMax;
    }
    return requestedMs;
}

void ReleaseRenderAnimationTimerResolution(void) {
    if (s_renderAnimationTimerResolutionMs > 0) {
        timeEndPeriod(s_renderAnimationTimerResolutionMs);
        s_renderAnimationTimerResolutionMs = 0;
    }
}

void UpdateRenderAnimationTimerResolution(UINT interval) {
    UINT requestedResolutionMs = 0;

    if (interval <= 25u) {
        requestedResolutionMs = ClampRenderTimerResolutionToDeviceCaps(
            ChooseRenderTimerResolutionMs(interval));
    }

    if (s_renderAnimationTimerResolutionMs == requestedResolutionMs) {
        return;
    }

    ReleaseRenderAnimationTimerResolution();

    if (requestedResolutionMs > 0) {
        MMRESULT res = timeBeginPeriod(requestedResolutionMs);
        if (res == TIMERR_NOERROR) {
            s_renderAnimationTimerResolutionMs = requestedResolutionMs;
        } else {
            WriteLog(LOG_LEVEL_WARNING,
                     "Failed to set render animation timer resolution (resolution=%u)",
                     requestedResolutionMs);
        }
    }
}

BOOL SetDrawingRenderAnimationTimer(HWND hwnd, UINT interval) {
    if (!IsValidRenderAnimationWindow(hwnd) || interval == 0) {
        StopDrawingRenderAnimationTimer(NULL);
        return FALSE;
    }

    if (s_renderAnimationTimerActive &&
        s_renderAnimationTimerHwnd == hwnd &&
        s_renderAnimationTimerInterval == interval) {
        return TRUE;
    }

    if (!SetTimer(hwnd, TIMER_ID_RENDER_ANIMATION, interval, NULL)) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Failed to set render animation timer (interval=%u, error=%lu)",
                 interval, GetLastError());
        StopDrawingRenderAnimationTimer(hwnd);
        return FALSE;
    }

    HWND previousHwnd = s_renderAnimationTimerHwnd;
    BOOL hadPreviousTimer = s_renderAnimationTimerActive;
    if (hadPreviousTimer && previousHwnd != hwnd) {
        HWND killHwnd = IsValidRenderAnimationWindow(previousHwnd) ? previousHwnd : NULL;
        if (killHwnd) {
            KillTimer(killHwnd, TIMER_ID_RENDER_ANIMATION);
        }
    }

    s_renderAnimationTimerActive = TRUE;
    s_renderAnimationTimerInterval = interval;
    s_renderAnimationTimerHwnd = hwnd;
    UpdateRenderAnimationTimerResolution(interval);
    return TRUE;
}

BOOL IsActiveTextColorAnimated(void) {
    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));

    static char s_lastActiveColor[COLOR_HEX_BUFFER] = {0};
    static BOOL s_lastActiveColorAnimated = FALSE;

    if (strcmp(activeColor, s_lastActiveColor) == 0) {
        return s_lastActiveColorAnimated;
    }

    strncpy_s(s_lastActiveColor, sizeof(s_lastActiveColor), activeColor, _TRUNCATE);
    s_lastActiveColorAnimated = IsGradientNameAnimated(activeColor);
    return s_lastActiveColorAnimated;
}

BOOL ShouldRunRenderAnimationTimer(BOOL hasRenderableContent,
                                          BOOL hasColorTagGradient) {
    /* Holographic is a static prism/glow pass; only liquid and animated
     * gradients need a render-only timer.
     */
    if (!hasRenderableContent) {
        return FALSE;
    }

    EffectType activeEffect = GetActiveEffect();
    return TextEffect_NeedsRenderTimer(activeEffect) ||
           hasColorTagGradient ||
           IsActiveTextColorAnimated();
}

BOOL UpdateDrawingRenderAnimationTimerForFrame(HWND hwnd,
                                                      BOOL hasRenderableContent,
                                                      BOOL hasColorTagGradient) {
    if (!IsValidRenderAnimationWindow(hwnd)) {
        StopDrawingRenderAnimationTimer(NULL);
        return FALSE;
    }

    /* Scale frames already repaint at the gesture cadence. Running the
     * independent effect timer at the same time only queues duplicate paints
     * and makes wheel input feel uneven on slower machines. */
    if (GetScaleWindowGestureSerial(hwnd) != 0) {
        StopDrawingRenderAnimationTimer(hwnd);
        return FALSE;
    }

    if (!IsWindowVisible(hwnd) ||
        !ShouldRunRenderAnimationTimer(hasRenderableContent, hasColorTagGradient)) {
        StopDrawingRenderAnimationTimer(hwnd);
        return FALSE;
    }

    size_t pixelCount = 0;
    if (hwnd) {
        RECT rect;
        GetClientRect(hwnd, &rect);
        CalculatePixelCount(rect.right, rect.bottom, &pixelCount);
    }

    return SetDrawingRenderAnimationTimer(
        hwnd,
        GetRenderAnimationTimerInterval(pixelCount, hasColorTagGradient));
}

void StopDrawingRenderAnimationTimer(HWND hwnd) {
    HWND trackedHwnd = s_renderAnimationTimerHwnd;

    if (IsValidRenderAnimationWindow(hwnd)) {
        KillTimer(hwnd, TIMER_ID_RENDER_ANIMATION);
    }

    if (trackedHwnd != hwnd && IsValidRenderAnimationWindow(trackedHwnd)) {
        KillTimer(trackedHwnd, TIMER_ID_RENDER_ANIMATION);
    }

    s_renderAnimationTimerActive = FALSE;
    s_renderAnimationTimerInterval = 0;
    s_renderAnimationTimerHwnd = NULL;
    ReleaseRenderAnimationTimerResolution();
}

int ClampRenderInt64(long long value, int minValue, int maxValue) {
    if (value < (long long)minValue) return minValue;
    if (value > (long long)maxValue) return maxValue;
    return (int)value;
}

int AddRenderDimensionClamped(int value, int delta) {
    return ClampRenderInt64((long long)value + (long long)delta, 0, INT_MAX);
}

BOOL GetRenderWindowLimits(HWND hwnd, SIZE* outLimits) {
    if (!outLimits) return FALSE;

    int maxWindowWidth = MAX_RENDER_DIB_DIMENSION;
    int maxWindowHeight = MAX_RENDER_DIB_DIMENSION;

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo;
    ZeroMemory(&monitorInfo, sizeof(monitorInfo));
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
        int workWidth = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
        int workHeight = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
        if (workWidth > 0 && workWidth < maxWindowWidth) {
            maxWindowWidth = workWidth;
        }
        if (workHeight > 0 && workHeight < maxWindowHeight) {
            maxWindowHeight = workHeight;
        }
    }

    if (maxWindowWidth <= 0 || maxWindowHeight <= 0) {
        return FALSE;
    }

    size_t maxPixels = 0;
    if (!CalculatePixelCount(maxWindowWidth, maxWindowHeight, &maxPixels)) {
        return FALSE;
    }
    if (maxPixels > MAX_RENDER_DIB_PIXELS) {
        size_t limitedHeight = MAX_RENDER_DIB_PIXELS / (size_t)maxWindowWidth;
        maxWindowHeight = (limitedHeight > 0 && limitedHeight < (size_t)INT_MAX)
            ? (int)limitedHeight
            : 1;
    }

    outLimits->cx = maxWindowWidth;
    outLimits->cy = maxWindowHeight;
    return TRUE;
}

BOOL GetConstrainedRenderWindowSize(HWND hwnd, const SIZE* contentSize, SIZE* outSize) {
    if (!contentSize || !outSize || contentSize->cx <= 0 || contentSize->cy <= 0) {
        return FALSE;
    }

    SIZE limits;
    if (!GetRenderWindowLimits(hwnd, &limits)) {
        return FALSE;
    }

    int requestedWidth = ClampRenderInt64((long long)contentSize->cx + WINDOW_HORIZONTAL_PADDING,
                                          1, limits.cx);
    int requestedHeight = ClampRenderInt64((long long)contentSize->cy + WINDOW_VERTICAL_PADDING,
                                           1, limits.cy);

    outSize->cx = requestedWidth;
    outSize->cy = requestedHeight;
    return TRUE;
}
