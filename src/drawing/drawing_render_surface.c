/**
 * @file drawing_render_surface.c
 * @brief Scale snapshots, DIB caching, and alpha repair.
 */

#include "drawing_render_internal.h"

void ReleaseScaleFrameSnapshot(void) {
    if (g_scaleFrameSnapshot.memDC && g_scaleFrameSnapshot.oldBitmap) {
        SelectObject(g_scaleFrameSnapshot.memDC,
                     g_scaleFrameSnapshot.oldBitmap);
    }
    if (g_scaleFrameSnapshot.memBitmap) {
        DeleteObject(g_scaleFrameSnapshot.memBitmap);
    }
    if (g_scaleFrameSnapshot.memDC) {
        DeleteDC(g_scaleFrameSnapshot.memDC);
    }
    ZeroMemory(&g_scaleFrameSnapshot, sizeof(g_scaleFrameSnapshot));
}

BOOL CreateScaleFrameSnapshotSurface(HDC referenceDC,
                                            int width,
                                            int height) {
    if (!referenceDC || width <= 0 || height <= 0) {
        return FALSE;
    }

    size_t pixelCount = 0;
    if (!CalculatePixelCount(width, height, &pixelCount) ||
        pixelCount > MAX_RENDER_DIB_PIXELS) {
        return FALSE;
    }

    HDC memDC = CreateCompatibleDC(referenceDC);
    if (!memDC) {
        return FALSE;
    }

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    HBITMAP bitmap = CreateDIBSection(referenceDC, &bmi, DIB_RGB_COLORS,
                                      &bits, NULL, 0);
    if (!bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        DeleteDC(memDC);
        return FALSE;
    }

    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, bitmap);
    if (!oldBitmap || oldBitmap == (HBITMAP)HGDI_ERROR) {
        DeleteObject(bitmap);
        DeleteDC(memDC);
        return FALSE;
    }

    g_scaleFrameSnapshot.memDC = memDC;
    g_scaleFrameSnapshot.memBitmap = bitmap;
    g_scaleFrameSnapshot.oldBitmap = oldBitmap;
    g_scaleFrameSnapshot.bits = bits;
    g_scaleFrameSnapshot.width = width;
    g_scaleFrameSnapshot.height = height;
    return TRUE;
}

BOOL TryCaptureScaleFrameSnapshot(HWND hwnd,
                                         HDC referenceDC,
                                         DWORD gestureSerial,
                                         const RECT* currentRect) {
    if (!hwnd || !referenceDC || gestureSerial == 0 || !currentRect) {
        return FALSE;
    }

    if (g_scaleFrameSnapshot.hwnd == hwnd &&
        g_scaleFrameSnapshot.gestureSerial == gestureSerial &&
        g_scaleFrameSnapshot.memDC && g_scaleFrameSnapshot.bits) {
        return TRUE;
    }

    ReleaseScaleFrameSnapshot();

    int width = currentRect->right - currentRect->left;
    int height = currentRect->bottom - currentRect->top;
    size_t pixelCount = 0;
    if (!CalculatePixelCount(width, height, &pixelCount) ||
        pixelCount < SCALE_SNAPSHOT_MIN_PIXELS ||
        !g_renderDibCache.frameValid ||
        g_renderDibCache.frameWasScaleComposite ||
        !g_renderDibCache.frameEditMode ||
        g_renderDibCache.frameHwnd != hwnd ||
        g_renderDibCache.frameWidth != width ||
        g_renderDibCache.frameHeight != height ||
        !g_renderDibCache.bits) {
        return FALSE;
    }

    if (!CreateScaleFrameSnapshotSurface(referenceDC, width, height)) {
        return FALSE;
    }

    memcpy(g_scaleFrameSnapshot.bits,
           g_renderDibCache.bits,
           pixelCount * sizeof(DWORD));
    g_scaleFrameSnapshot.hwnd = hwnd;
    g_scaleFrameSnapshot.gestureSerial = gestureSerial;
    return TRUE;
}

BOOL CompositeScaleFrameSnapshot(HWND hwnd,
                                        DWORD gestureSerial,
                                        HDC destDC,
                                        void* destBits,
                                        int destWidth,
                                        int destHeight) {
    if (!hwnd || gestureSerial == 0 || !destDC || !destBits ||
        destWidth <= 0 || destHeight <= 0 ||
        g_scaleFrameSnapshot.hwnd != hwnd ||
        g_scaleFrameSnapshot.gestureSerial != gestureSerial ||
        !g_scaleFrameSnapshot.memDC || !g_scaleFrameSnapshot.bits) {
        return FALSE;
    }

    size_t pixelCount = 0;
    if (!CalculatePixelCount(destWidth, destHeight, &pixelCount)) {
        return FALSE;
    }
    ZeroMemory(destBits, pixelCount * sizeof(DWORD));

    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    return AlphaBlend(destDC,
                      0, 0, destWidth, destHeight,
                      g_scaleFrameSnapshot.memDC,
                      0, 0,
                      g_scaleFrameSnapshot.width,
                      g_scaleFrameSnapshot.height,
                      blend);
}

void ReleaseRenderDibCache(void) {
    if (g_renderDibCache.memDC && g_renderDibCache.oldBitmap) {
        SelectObject(g_renderDibCache.memDC, g_renderDibCache.oldBitmap);
    }
    if (g_renderDibCache.memBitmap) {
        DeleteObject(g_renderDibCache.memBitmap);
    }
    if (g_renderDibCache.memDC) {
        DeleteDC(g_renderDibCache.memDC);
    }
    ZeroMemory(&g_renderDibCache, sizeof(g_renderDibCache));
}

BOOL ShouldReuseRenderDibCache(int width, int height, size_t requiredPixels) {
    size_t cachedPixels = 0;
    if (!g_renderDibCache.memDC || !g_renderDibCache.memBitmap || !g_renderDibCache.bits) {
        return FALSE;
    }
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
BOOL SetupDoubleBufferDIB(HDC hdc, const RECT* rect, HDC* memDC, HBITMAP* memBitmap, HBITMAP* oldBitmap, void** ppvBits) {
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
void FixAlphaChannel(void* bits, int width, int height) {
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
