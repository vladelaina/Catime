    if (g_renderDibCache.width != width || g_renderDibCache.height < height) {
        return FALSE;
    }
    if (!CalculatePixelCount(g_renderDibCache.width, g_renderDibCache.height, &cachedPixels)) {
        return FALSE;
    }
    if (requiredPixels > 0 &&
        cachedPixels / RENDER_DIB_SHRINK_THRESHOLD_MULTIPLIER > requiredPixels) {
        return FALSE;
    }
    return TRUE;
}

/** @note GM_ADVANCED + HALFTONE improve text quality on high-DPI displays */
static BOOL SetupDoubleBufferDIB(HDC hdc, const RECT* rect, HDC* memDC, HBITMAP* memBitmap, HBITMAP* oldBitmap, void** ppvBits) {
    size_t pixelCount;
    if (!rect || !CalculatePixelCount(rect->right, rect->bottom, &pixelCount)) {
        return FALSE;
    }
    if (pixelCount > MAX_RENDER_DIB_PIXELS) {
        WriteLog(LOG_LEVEL_WARNING, "Render DIB too large: %dx%d", rect->right, rect->bottom);
        return FALSE;
    }

    if (ShouldReuseRenderDibCache(rect->right, rect->bottom, pixelCount)) {
        *memDC = g_renderDibCache.memDC;
        *memBitmap = g_renderDibCache.memBitmap;
        *oldBitmap = g_renderDibCache.oldBitmap;
        *ppvBits = g_renderDibCache.bits;
        return TRUE;
    }

    HDC newMemDC = CreateCompatibleDC(hdc);
    if (!newMemDC) {
        return FALSE;
    }

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = rect->right;
    // Negative height creates a top-down DIB, matching STB's coordinate system
    bmi.bmiHeader.biHeight = -rect->bottom;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* newBits = NULL;
    HBITMAP newBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &newBits, NULL, 0);
    if (!newBitmap || !newBits) {
        if (newBitmap) DeleteObject(newBitmap);
        DeleteDC(newMemDC);
        return FALSE;
    }

    HBITMAP newOldBitmap = (HBITMAP)SelectObject(newMemDC, newBitmap);
    if (!newOldBitmap) {
        DeleteObject(newBitmap);
        DeleteDC(newMemDC);
        return FALSE;
    }

    SetGraphicsMode(newMemDC, GM_ADVANCED);
    SetBkMode(newMemDC, TRANSPARENT);
    SetStretchBltMode(newMemDC, HALFTONE);
    SetBrushOrgEx(newMemDC, 0, 0, NULL);
    SetTextAlign(newMemDC, TA_LEFT | TA_TOP);
    SetTextCharacterExtra(newMemDC, 0);
    SetMapMode(newMemDC, MM_TEXT);
    SetICMMode(newMemDC, ICM_ON);
    SetLayout(newMemDC, 0);

    ReleaseRenderDibCache();

    g_renderDibCache.memDC = newMemDC;
    g_renderDibCache.memBitmap = newBitmap;
    g_renderDibCache.oldBitmap = newOldBitmap;
    g_renderDibCache.bits = newBits;
    g_renderDibCache.width = rect->right;
    g_renderDibCache.height = rect->bottom;

    *memDC = g_renderDibCache.memDC;
    *memBitmap = g_renderDibCache.memBitmap;
    *oldBitmap = g_renderDibCache.oldBitmap;
    *ppvBits = g_renderDibCache.bits;

    return TRUE;
}

/**
 * @brief Manually set alpha channel to opaque for non-black pixels
 * @details GDI text drawing leaves alpha channel as 0, which DWM treats as transparent.
 *          We iterate pixels to set Alpha=255 where RGB != 0.
 */
static void FixAlphaChannel(void* bits, int width, int height) {
    if (!bits) return;

    DWORD* pixels = (DWORD*)bits;
    size_t count;
    if (!CalculatePixelCount(width, height, &count)) return;

    for (size_t i = 0; i < count; i++) {
        // Check if RGB is not black (0x00RRGGBB)
        if ((pixels[i] & 0x00FFFFFF) != 0) {
            // Only set Alpha to 255 if it's currently 0 (meaning it was drawn by GDI without alpha)
            if ((pixels[i] & 0xFF000000) == 0) {
                pixels[i] |= 0xFF000000;
            }
        } else {
            // Ensure black background is transparent
            pixels[i] &= 0x00FFFFFF;
        }
    }
}

/** @note Skips resize if size unchanged to reduce SetWindowPos overhead */
static void AdjustWindowSize(HWND hwnd, const SIZE* textSize, RECT* rect) {
    SIZE targetSize;
    DWORD scaleGestureSerial = GetScaleWindowGestureSerial(hwnd);
    if (CLOCK_IS_DRAGGING && scaleGestureSerial == 0) {
        ClearPendingScaleResizeAnchor(hwnd);
        return;
    }

    if (!GetConstrainedRenderWindowSize(hwnd, textSize, &targetSize)) {
        ClearPendingScaleResizeAnchor(hwnd);
        return;
    }

    POINT resizeAnchor = {0};
    double resizeAnchorRatioX = 0.5;
    double resizeAnchorRatioY = 0.5;
    BOOL hasResizeAnchor = GetPendingScaleResizeAnchorInfo(hwnd,
                                                           &resizeAnchor,
                                                           &resizeAnchorRatioX,
                                                           &resizeAnchorRatioY);
    LONG currentClientWidth = rect->right - rect->left;
    LONG currentClientHeight = rect->bottom - rect->top;

    if (targetSize.cx == currentClientWidth &&
        targetSize.cy == currentClientHeight) {
        if (hasResizeAnchor && scaleGestureSerial == 0) {
            ConsumePendingScaleResizeAnchor(hwnd);
        } else {
            ClearPendingScaleResizeAnchor(hwnd);
        }
        return;
    }

    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int newX = windowRect.left;
    int newY = windowRect.top;
    if (hasResizeAnchor) {
        newX = resizeAnchor.x - (int)(resizeAnchorRatioX * (double)targetSize.cx + 0.5);
        newY = resizeAnchor.y - (int)(resizeAnchorRatioY * (double)targetSize.cy + 0.5);
    } else {
        HMONITOR monitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = {0};
        RECT taskbarRect = {0};
        RECT intersection = {0};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (monitor && GetMonitorInfo(monitor, &monitorInfo) &&
            GetTaskbarRectForMonitor(monitor, &taskbarRect) &&
            IntersectRect(&intersection, &windowRect, &taskbarRect)) {
            int axisRatio = 0;
            int crossOffset = 0;
            if (WindowPlacement_CaptureTaskbarAnchor(
                    &windowRect, &taskbarRect, &monitorInfo.rcMonitor,
                    &axisRatio, &crossOffset)) {
                WindowPlacement_ResolveTaskbarAnchor(
                    &taskbarRect, &monitorInfo.rcMonitor,
                    targetSize.cx, targetSize.cy,
                    axisRatio, crossOffset, &newX, &newY);
            }
        }
    }

    ClampWindowPositionToVisibleMonitor(targetSize.cx, targetSize.cy,
                                        &newX, &newY);

    SetWindowPos(hwnd, NULL,
        newX, newY,
        targetSize.cx,
        targetSize.cy,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    if (hasResizeAnchor && scaleGestureSerial == 0) {
        ConsumePendingScaleResizeAnchor(hwnd);
    } else {
        ClearPendingScaleResizeAnchor(hwnd);
    }

    CLOCK_WINDOW_POS_X = newX;
    CLOCK_WINDOW_POS_Y = newY;

    GetClientRect(hwnd, rect);
}

void HandleWindowPaint(HWND hwnd, const PAINTSTRUCT* ps) {
    if (CLOCK_IS_DRAGGING) {
        ReleaseScaleFrameSnapshot();
        return;
    }

    PaintTextBuffers* paintBuffers = &g_paintTextBuffers;
    wchar_t* timeText = paintBuffers->timeText;
    wchar_t* pluginText = paintBuffers->pluginText;
    wchar_t* result = paintBuffers->pluginResult;
    HDC hdc = ps->hdc;
    RECT rect;
    GetClientRect(hwnd, &rect);
    DWORD activeScaleSerial = GetScaleWindowGestureSerial(hwnd);
    if (activeScaleSerial == 0) {
        ReleaseScaleFrameSnapshot();
    } else {
        TryCaptureScaleFrameSnapshot(hwnd, hdc, activeScaleSerial, &rect);
    }

    ClearClickableRegions();
    timeText[0] = L'\0';
    GetTimeText(timeText, TIME_TEXT_MAX_LEN);
    wcscpy_s(paintBuffers->timerTextSnapshot, TIME_TEXT_MAX_LEN, timeText);

    // Check for plugin data
    pluginText[0] = L'\0';
    MarkdownImage stackImages[PLUGIN_IMAGE_STACK_CAPACITY];
    MarkdownImage* images = NULL;
    int imageCount = 0;
    int imageCapacity = 0;
    BOOL imagesHeapAllocated = FALSE;
    BOOL imageCapacityExhausted = FALSE;
    BOOL imagesOwnedByCache = FALSE;

    if (PluginData_GetText(pluginText, TIME_TEXT_MAX_LEN)) {
        BOOL canUsePluginPaintCache = (wcsstr(pluginText, CATIME_OPEN_TAG) == NULL);

        if (canUsePluginPaintCache && g_pluginPaintCache.valid &&
            CachedWideTextEquals(g_pluginPaintCache.sourceText,
                                 _countof(g_pluginPaintCache.sourceText),
                                 g_pluginPaintCache.sourceTextLen,
                                 pluginText)) {
            wcscpy_s(timeText, TIME_TEXT_MAX_LEN, g_pluginPaintCache.renderedText);
            images = g_pluginPaintCache.images;
            imageCount = g_pluginPaintCache.imageCount;
            imagesOwnedByCache = TRUE;
        } else {
        wchar_t savedTime[256];
        wcsncpy(savedTime, timeText, _countof(savedTime) - 1);
        savedTime[_countof(savedTime) - 1] = L'\0';
        size_t savedTimeLen = wcslen(savedTime);

        ZeroMemory(stackImages, sizeof(stackImages));
        images = stackImages;
        imageCapacity = PLUGIN_IMAGE_STACK_CAPACITY;

        // Replace ALL <catime></catime> tags and extract ![](path) images in one pass.
        result[0] = L'\0';
        const wchar_t* src = pluginText;
        wchar_t* dst = result;
        size_t remaining = TIME_TEXT_MAX_LEN - 1;
