    /* Must start with ![ */
    if (*p != L'!' || *(p + 1) != L'[') return FALSE;

    p += 2;  /* Skip ![ */

    /* Find ] */
    const wchar_t* bracketEnd = wcschr(p, L']');
    if (!bracketEnd || *(bracketEnd + 1) != L'(') return FALSE;

    /* Extract size string (between [ and ]) */
    size_t sizeLen = bracketEnd - p;
    int specWidth = 0, specHeight = 0;
    if (sizeLen > 0) {
        ParseImageSize(p, sizeLen, &specWidth, &specHeight);
    }

    /* Find path between ( and ) */
    const wchar_t* pathStart = bracketEnd + 2;
    const wchar_t* pathEnd = wcschr(pathStart, L')');
    if (!pathEnd || pathEnd == pathStart) return FALSE;

    size_t pathLen = pathEnd - pathStart;
    if (pathLen == 0 || pathLen > MARKDOWN_IMAGE_PATH_MAX_CHARS) {
        return FALSE;
    }

    /* Allocate and copy path */
    wchar_t* imagePath = (wchar_t*)malloc((pathLen + 1) * sizeof(wchar_t));
    if (!imagePath) return FALSE;

    wcsncpy(imagePath, pathStart, pathLen);
    imagePath[pathLen] = L'\0';

    /* Fill image structure */
    MarkdownImage* img = &images[*imageCount];
    memset(img, 0, sizeof(MarkdownImage));
    img->imagePath = imagePath;
    img->specifiedWidth = specWidth;
    img->specifiedHeight = specHeight;
    img->startPos = currentPos;
    img->endPos = currentPos;  /* Images don't occupy text space */

    /* Check if network image */
    img->isNetworkImage = IsNetworkUrl(imagePath);

    (*imageCount)++;
    *src = pathEnd + 1;

    return TRUE;
}

/* ============================================================================
 * Rendering
 * ============================================================================ */

/* External scale factors */
extern float CLOCK_FONT_SCALE_FACTOR;
extern float PLUGIN_FONT_SCALE_FACTOR;

BOOL CalculateImageRenderSize(MarkdownImage* image, int maxWidth, int maxHeight,
                              int* outWidth, int* outHeight) {
    if (!image || !outWidth || !outHeight) return FALSE;
    if (maxWidth <= 0 || maxHeight <= 0) return FALSE;

    *outWidth = 0;
    *outHeight = 0;

    /* Resolve path if not already done */
    if (!image->resolvedPath) {
        if (!ResolveImagePath(image)) return FALSE;
    }

    /* Get actual image dimensions */
    int imgW = image->intrinsicWidth;
    int imgH = image->intrinsicHeight;
    if (imgW <= 0 || imgH <= 0) {
        if (!GetImageDimensions(image->resolvedPath, &imgW, &imgH) || imgW <= 0 || imgH <= 0) {
            return FALSE;
        }
        image->intrinsicWidth = imgW;
        image->intrinsicHeight = imgH;
    }

    /* Apply scale factor based on current mode */
    float scale = PluginData_IsActive() ? PLUGIN_FONT_SCALE_FACTOR : CLOCK_FONT_SCALE_FACTOR;
    if (scale < 0.1f) scale = 1.0f;

    /* Determine target size */
    int targetW, targetH;
    if (image->specifiedWidth > 0 && image->specifiedHeight > 0) {
        /* User specified both dimensions */
        if (!ScaleIntToInt(image->specifiedWidth, scale, &targetW) ||
            !ScaleIntToInt(image->specifiedHeight, scale, &targetH)) {
            return FALSE;
        }
    } else if (image->specifiedWidth > 0) {
        /* User specified width only - calculate height from aspect ratio */
        double computedH;
        if (!ScaleIntToInt(image->specifiedWidth, scale, &targetW)) return FALSE;
        computedH = (double)imgH * ((double)targetW / (double)imgW);
        if (computedH <= 0.0 || computedH > (double)INT_MAX) return FALSE;
        targetH = (int)computedH;
    } else if (image->specifiedHeight > 0) {
        /* User specified height only - calculate width from aspect ratio */
        double computedW;
        if (!ScaleIntToInt(image->specifiedHeight, scale, &targetH)) return FALSE;
        computedW = (double)imgW * ((double)targetH / (double)imgH);
        if (computedW <= 0.0 || computedW > (double)INT_MAX) return FALSE;
        targetW = (int)computedW;
    } else {
        /* No size specified - use original image size with scale */
        if (!ScaleIntToInt(imgW, scale, &targetW) ||
            !ScaleIntToInt(imgH, scale, &targetH)) {
            return FALSE;
        }
    }

    /* Clamp to max bounds */
    if (targetW > maxWidth) targetW = maxWidth;
    if (targetH > maxHeight) targetH = maxHeight;

    /* Calculate actual size maintaining aspect ratio */
    float scaleX = (float)targetW / imgW;
    float scaleY = (float)targetH / imgH;
    float fitScale = (scaleX < scaleY) ? scaleX : scaleY;

    if (!ScaleIntToInt(imgW, fitScale, outWidth) ||
        !ScaleIntToInt(imgH, fitScale, outHeight)) {
        return FALSE;
    }

    return TRUE;
}

int RenderMarkdownImageSized(HDC hdc, MarkdownImage* image, int x, int y,
                             int actualWidth, int actualHeight) {
    if (!hdc || !image || !image->resolvedPath || actualWidth <= 0 || actualHeight <= 0) {
        return 0;
    }

    if (RenderImageGDIPlus(hdc, x, y, actualWidth, actualHeight, image->resolvedPath)) {
        if (!SetImageRenderRect(image, x, y, actualWidth, actualHeight)) {
            return 0;
        }
        return actualHeight;
    }

    return 0;
}

int RenderMarkdownImageSizedWithContext(ImageRenderContext* renderCtx,
                                        MarkdownImage* image, int x, int y,
                                        int actualWidth, int actualHeight) {
    if (!renderCtx || !image || !image->resolvedPath ||
        actualWidth <= 0 || actualHeight <= 0) {
        return 0;
    }

    if (!RenderImageGDIPlusWithContext(renderCtx, x, y, actualWidth, actualHeight,
                                       image->resolvedPath)) {
        return 0;
    }

    if (!SetImageRenderRect(image, x, y, actualWidth, actualHeight)) {
        return 0;
    }

    return actualHeight;
}

int RenderMarkdownImage(HDC hdc, MarkdownImage* image, int x, int y,
                        int maxWidth, int maxHeight) {
    if (!hdc || !image) return 0;

    int actualWidth = 0;
    int actualHeight = 0;
    if (!CalculateImageRenderSize(image, maxWidth, maxHeight, &actualWidth, &actualHeight)) {
        return 0;
    }

    return RenderMarkdownImageSized(hdc, image, x, y, actualWidth, actualHeight);
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void InitializeMarkdownImage(void) {
    if (InterlockedCompareExchange(&g_activeDownloadCount, 0, 0) == 0) {
        InterlockedIncrement(&g_downloadGeneration);
        InterlockedExchange(&g_downloadShutdown, 0);
    } else {
        InterlockedExchange(&g_downloadRestartPending, 1);
    }
}

void ShutdownMarkdownImage(void) {
    AcquireSRWLockExclusive(&g_downloadLifecycleLock);
    InterlockedIncrement(&g_downloadGeneration);
    RequestMarkdownImageDownloadCancel();

    if (g_downloadIdleEvent) {
        DWORD waitResult = WaitForSingleObject(g_downloadIdleEvent, IMAGE_SHUTDOWN_GRACE_MS);
        if (waitResult != WAIT_OBJECT_0) {
            LOG_WARNING("Timed out waiting for markdown image downloads after cancellation; leaving shutdown state for late cleanup");
            ReleaseSRWLockExclusive(&g_downloadLifecycleLock);
            return;
        }
    }

    if (IsDownloadCSReady()) {
        DeleteCriticalSection(&g_downloadCS);
        InterlockedExchange(&g_downloadCSInit, 0);
    }

    if (g_downloadIdleEvent) {
        CloseHandle(g_downloadIdleEvent);
        g_downloadIdleEvent = NULL;
    }

    ZeroMemory(g_downloadingHashes, sizeof(g_downloadingHashes));
    ZeroMemory(g_activeDownloadHandles, sizeof(g_activeDownloadHandles));
    ZeroMemory(g_failedDownloadHashes, sizeof(g_failedDownloadHashes));
    ZeroMemory(g_failedDownloadRetryTicks, sizeof(g_failedDownloadRetryTicks));
    g_downloadingCount = 0;
    g_failedDownloadCount = 0;
    ClearDownloadInitFailure();
    InterlockedExchange(&g_downloadRestartPending, 0);
    InterlockedExchange(&g_activeDownloadCount, 0);
    ReleaseSRWLockExclusive(&g_downloadLifecycleLock);
}

void FreeMarkdownImageEntries(MarkdownImage* images, int imageCount) {
    if (!images) return;

    for (int i = 0; i < imageCount; i++) {
        if (images[i].imagePath) {
            free(images[i].imagePath);
            images[i].imagePath = NULL;
        }
        if (images[i].resolvedPath) {
            FreeMarkdownImageResolvedPath(&images[i]);
        }
    }
}

void FreeMarkdownImages(MarkdownImage* images, int imageCount) {
    if (!images) return;

    FreeMarkdownImageEntries(images, imageCount);
    free(images);
}
