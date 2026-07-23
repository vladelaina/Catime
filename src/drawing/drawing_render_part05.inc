                    images[i].downloadRetryScheduled = FALSE;
                    images[i].downloadRetryTick = 0;
                    images[i].downloadFailed = FALSE;
                } else if (!images[i].downloadRetryScheduled) {
                    images[i].downloadFailed = FALSE;
                }
            }
        }
    }
}

static BOOL EnsurePaintMarkdownImageCapacity(MarkdownImage** images,
                                             int* imageCapacity,
                                             BOOL* heapAllocated,
                                             MarkdownImage* stackImages) {
    if (!images || !*images || !imageCapacity || !heapAllocated || !stackImages) {
        return FALSE;
    }

    int oldCapacity = *imageCapacity;
    if (oldCapacity <= 0 ||
        (size_t)oldCapacity > ((size_t)-1) / 2u / sizeof(MarkdownImage)) {
        return FALSE;
    }

    int newCapacity = oldCapacity * 2;
    MarkdownImage* newImages = NULL;

    if (*heapAllocated) {
        newImages = (MarkdownImage*)realloc(*images, (size_t)newCapacity * sizeof(MarkdownImage));
        if (!newImages) return FALSE;
        ZeroMemory(newImages + oldCapacity, (size_t)(newCapacity - oldCapacity) * sizeof(MarkdownImage));
    } else {
        newImages = (MarkdownImage*)calloc((size_t)newCapacity, sizeof(MarkdownImage));
        if (!newImages) return FALSE;
        memcpy(newImages, stackImages, (size_t)oldCapacity * sizeof(MarkdownImage));
        ZeroMemory(stackImages, (size_t)oldCapacity * sizeof(MarkdownImage));
        *heapAllocated = TRUE;
    }

    *images = newImages;
    *imageCapacity = newCapacity;
    return TRUE;
}

static MarkdownImage* MovePaintMarkdownImagesToHeap(MarkdownImage* images,
                                                    int imageCount,
                                                    BOOL* heapAllocated,
                                                    MarkdownImage* stackImages) {
    if (!images || imageCount <= 0 || !heapAllocated) {
        return NULL;
    }

    if (*heapAllocated) {
        *heapAllocated = FALSE;
        return images;
    }

    MarkdownImage* heapImages =
        (MarkdownImage*)calloc((size_t)imageCount, sizeof(MarkdownImage));
    if (!heapImages) {
        return NULL;
    }

    memcpy(heapImages, images, (size_t)imageCount * sizeof(MarkdownImage));
    if (images == stackImages && stackImages) {
        ZeroMemory(stackImages, (size_t)imageCount * sizeof(MarkdownImage));
    }
    return heapImages;
}

typedef struct {
    HDC memDC;
    HBITMAP memBitmap;
    HBITMAP oldBitmap;
    void* bits;
    int width;
    int height;
    BOOL frameValid;
    BOOL frameWasScaleComposite;
    BOOL frameEditMode;
    HWND frameHwnd;
    int frameWidth;
    int frameHeight;
} RenderDibCache;

static RenderDibCache g_renderDibCache = {0};

typedef struct {
    HDC memDC;
    HBITMAP memBitmap;
    HBITMAP oldBitmap;
    void* bits;
    int width;
    int height;
    HWND hwnd;
    DWORD gestureSerial;
} ScaleFrameSnapshot;

static ScaleFrameSnapshot g_scaleFrameSnapshot = {0};

static void ReleaseScaleFrameSnapshot(void) {
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

static BOOL CreateScaleFrameSnapshotSurface(HDC referenceDC,
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

static BOOL TryCaptureScaleFrameSnapshot(HWND hwnd,
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

static BOOL CompositeScaleFrameSnapshot(HWND hwnd,
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

static void ReleaseRenderDibCache(void) {
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

static BOOL ShouldReuseRenderDibCache(int width, int height, size_t requiredPixels) {
    size_t cachedPixels = 0;
    if (!g_renderDibCache.memDC || !g_renderDibCache.memBitmap || !g_renderDibCache.bits) {
        return FALSE;
    }
