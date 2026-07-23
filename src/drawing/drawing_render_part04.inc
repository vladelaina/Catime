    }
}

static BOOL SetDrawingRenderAnimationTimer(HWND hwnd, UINT interval) {
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

static BOOL IsActiveTextColorAnimated(void) {
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

static BOOL ShouldRunRenderAnimationTimer(BOOL hasRenderableContent,
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

static BOOL UpdateDrawingRenderAnimationTimerForFrame(HWND hwnd,
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

static int ClampRenderInt64(long long value, int minValue, int maxValue) {
    if (value < (long long)minValue) return minValue;
    if (value > (long long)maxValue) return maxValue;
    return (int)value;
}

static int AddRenderDimensionClamped(int value, int delta) {
    return ClampRenderInt64((long long)value + (long long)delta, 0, INT_MAX);
}

static BOOL GetRenderWindowLimits(HWND hwnd, SIZE* outLimits) {
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

static BOOL GetConstrainedRenderWindowSize(HWND hwnd, const SIZE* contentSize, SIZE* outSize) {
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

static void FreePaintMarkdownImages(MarkdownImage* images, int imageCount, BOOL heapAllocated) {
    if (!images) return;

    if (heapAllocated) {
        FreeMarkdownImages(images, imageCount);
    } else {
        FreeMarkdownImageEntries(images, imageCount);
    }
}

static BOOL HasTickReached(DWORD tick) {
    return (DWORD)(GetTickCount() - tick) < 0x80000000u;
}

static BOOL IsMarkdownImageRetryPending(const MarkdownImage* image) {
    return image && image->downloadFailed && image->downloadRetryScheduled &&
           !HasTickReached(image->downloadRetryTick);
}

static void PreparePaintMarkdownImagesForFrame(MarkdownImage* images, int imageCount) {
    if (!images || imageCount <= 0) return;

    DWORD now = GetTickCount();
    BOOL checkResolvedFiles = s_nextMarkdownImageFileCheckTick == 0 ||
                              HasTickReached(s_nextMarkdownImageFileCheckTick);
    if (checkResolvedFiles) {
        s_nextMarkdownImageFileCheckTick = now + MARKDOWN_IMAGE_FILE_RECHECK_MS;
    }

    for (int i = 0; i < imageCount; i++) {
        SetRectEmpty(&images[i].imageRect);

        if (checkResolvedFiles &&
            images[i].resolvedPath &&
            !RefreshMarkdownImageResolvedFileState(&images[i])) {
            free(images[i].resolvedPath);
            images[i].resolvedPath = NULL;
            images[i].isDownloaded = FALSE;
            images[i].intrinsicWidth = 0;
            images[i].intrinsicHeight = 0;
            ZeroMemory(&images[i].resolvedLastWriteTime,
                       sizeof(images[i].resolvedLastWriteTime));
            images[i].resolvedFileSize = 0;
            images[i].resolvedFileInfoValid = FALSE;
        }

        if (images[i].isNetworkImage) {
            if (!images[i].isDownloaded) {
                images[i].intrinsicWidth = 0;
                images[i].intrinsicHeight = 0;

                if (images[i].isDownloading &&
                    !IsMarkdownImageDownloadInProgress(images[i].imagePath)) {
                    images[i].isDownloading = FALSE;
                    DWORD retryTick = 0;
                    if (GetMarkdownImageDownloadRetryTick(images[i].imagePath, &retryTick)) {
                        images[i].downloadFailed = TRUE;
                        images[i].downloadRetryScheduled = TRUE;
                        images[i].downloadRetryTick = retryTick;
                    }
                }

                if (IsMarkdownImageRetryPending(&images[i])) {
                    continue;
                }

                if (images[i].downloadRetryScheduled &&
                    HasTickReached(images[i].downloadRetryTick)) {
