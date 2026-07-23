            FreeMarkdownImageResolvedPath(image);
            ReleaseSRWLockShared(&g_downloadLifecycleLock);
            return;
        }
        image->isDownloaded = TRUE;
        image->isDownloading = FALSE;
        image->downloadFailed = FALSE;
        image->downloadRetryScheduled = FALSE;
        image->downloadRetryTick = 0;
        ClearUrlDownloadFailure(image->imagePath);
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    DWORD retryTick = 0;
    if (GetMarkdownImageDownloadRetryTick(image->imagePath, &retryTick)) {
        image->isDownloading = FALSE;
        image->downloadFailed = TRUE;
        image->downloadRetryScheduled = TRUE;
        image->downloadRetryTick = retryTick;
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    /* Check if already downloading */
    if (IsUrlDownloading(image->imagePath)) {
        image->isDownloading = TRUE;
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    /* Mark as downloading */
    image->isDownloading = TRUE;
    if (!TryAddDownloadingUrl(image->imagePath)) {
        ScheduleImageDownloadRetry(image, IMAGE_DOWNLOAD_QUEUE_RETRY_MS);
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    /* Prepare params */
    AsyncDownloadParams* params = (AsyncDownloadParams*)malloc(sizeof(AsyncDownloadParams));
    if (!params) {
        RemoveDownloadingUrl(image->imagePath);
        ScheduleImageDownloadRetry(image, IMAGE_DOWNLOAD_FAILURE_RETRY_MS);
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    wcsncpy(params->url, image->imagePath, 2047);
    params->url[2047] = L'\0';
    params->cachePath[0] = L'\0';
    params->hwnd = IsValidMarkdownImageNotifyWindow(hwnd) ? hwnd : NULL;
    params->generation = GetDownloadGeneration();

    /* Start background thread */
    if (!MarkDownloadStarted()) {
        free(params);
        RemoveDownloadingUrl(image->imagePath);
        ScheduleImageDownloadRetry(image, IMAGE_DOWNLOAD_FAILURE_RETRY_MS);
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    HANDLE hThread = CreateThread(NULL, 0, AsyncDownloadThread, params, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        MarkDownloadFinished();
        free(params);
        RemoveDownloadingUrl(image->imagePath);
        MarkUrlDownloadFailed(image->imagePath);
        ScheduleImageDownloadRetry(image, IMAGE_DOWNLOAD_FAILURE_RETRY_MS);
    }
    ReleaseSRWLockShared(&g_downloadLifecycleLock);
}

/* ============================================================================
 * Path Resolution
 * ============================================================================ */

BOOL ResolveImagePath(MarkdownImage* image) {
    if (!image || !image->imagePath) return FALSE;

    /* Free previous resolved path */
    FreeMarkdownImageResolvedPath(image);

    const wchar_t* path = image->imagePath;

    /* Network URL - check cache only, don't block */
    if (IsNetworkUrl(path)) {
        image->isNetworkImage = TRUE;

        /* Check if already cached */
        wchar_t cachePath[MAX_PATH];
        if (IsImageCached(path, cachePath)) {
            image->resolvedPath = _wcsdup(cachePath);
            if (!image->resolvedPath) return FALSE;
            if (!RefreshMarkdownImageResolvedFileState(image)) {
                FreeMarkdownImageResolvedPath(image);
                return FALSE;
            }
            image->isDownloaded = TRUE;
            image->isDownloading = FALSE;
            image->downloadFailed = FALSE;
            image->downloadRetryScheduled = FALSE;
            image->downloadRetryTick = 0;
            return TRUE;
        }

        /* Not cached - caller should use StartAsyncImageDownload */
        return FALSE;
    }

    image->isNetworkImage = FALSE;

    /* Absolute path */
    if (IsAbsolutePath(path)) {
        WIN32_FILE_ATTRIBUTE_DATA attrs;
        ULONGLONG fileSize = 0;
        if (!GetExistingNonEmptyFileInfoW(path, &attrs, &fileSize)) {
            return FALSE;
        }
        image->resolvedPath = _wcsdup(path);
        if (!image->resolvedPath) return FALSE;
        StoreMarkdownImageResolvedFileState(image, &attrs, fileSize);
        return TRUE;
    }

    /* Relative path - resolve relative to the active plugin output directory. */
    wchar_t baseDir[MAX_PATH];
    if (!GetPluginRelativeImageBaseDirectory(baseDir, MAX_PATH)) {
        return FALSE;
    }

    wchar_t fullPath[MAX_PATH];
    int written = _snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", baseDir, path);
    if (written < 0) {
        return FALSE;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    ULONGLONG fileSize = 0;
    if (!GetExistingNonEmptyFileInfoW(fullPath, &attrs, &fileSize)) {
        return FALSE;
    }

    image->resolvedPath = _wcsdup(fullPath);
    if (!image->resolvedPath) return FALSE;
    StoreMarkdownImageResolvedFileState(image, &attrs, fileSize);
    return TRUE;
}

/* ============================================================================
 * Parsing
 * ============================================================================ */

int CountMarkdownImages(const wchar_t* input) {
    if (!input) return 0;

    int count = 0;
    const wchar_t* p = input;

    while (*p) {
        if (*p == L'!' && *(p + 1) == L'[') {
            /* Find ]( */
            const wchar_t* bracketEnd = wcschr(p + 2, L']');
            if (!bracketEnd) {
                break;
            }
            if (*(bracketEnd + 1) != L'(') {
                p = bracketEnd + 1;
                continue;
            }
            const wchar_t* parenEnd = wcschr(bracketEnd + 2, L')');
            if (!parenEnd) {
                break;
            }
            if (count < INT_MAX) {
                count++;
            }
            p = parenEnd + 1;
            continue;
        }
        p++;
    }

    return count;
}

/**
 * @brief Parse size from ![WxH] or ![W] format
 */
static BOOL ParsePositiveIntLimited(const wchar_t* text, size_t len, int* value);

static void ParseImageSize(const wchar_t* sizeStr, size_t len, int* width, int* height) {
    *width = 0;
    *height = 0;

    if (len == 0) return;

    /* Find 'x' separator */
    const wchar_t* xPos = NULL;
    for (size_t i = 0; i < len; i++) {
        if (sizeStr[i] == L'x' || sizeStr[i] == L'X') {
            xPos = &sizeStr[i];
            break;
        }
    }

    if (xPos) {
        /* WxH format */
        size_t wLen = xPos - sizeStr;
        if (wLen > 0 && wLen < 16) {
            ParsePositiveIntLimited(sizeStr, wLen, width);
        }

        size_t hLen = len - wLen - 1;
        if (hLen > 0 && hLen < 16) {
            ParsePositiveIntLimited(xPos + 1, hLen, height);
        }
    } else if (len < 16) {
        /* Single number - treat as width, height auto */
        ParsePositiveIntLimited(sizeStr, len, width);
    }
}

static BOOL ParsePositiveIntLimited(const wchar_t* text, size_t len, int* value) {
    int result = 0;
    if (!text || !value || len == 0) return FALSE;

    for (size_t i = 0; i < len; i++) {
        int digit;
        if (text[i] < L'0' || text[i] > L'9') return FALSE;
        digit = (int)(text[i] - L'0');
        if (result > (INT_MAX - digit) / 10) return FALSE;
        result = result * 10 + digit;
    }

    *value = result;
    return result > 0;
}

static BOOL ScaleIntToInt(int value, float scale, int* outValue) {
    double scaled;
    if (!outValue || value <= 0 || scale <= 0.0f) return FALSE;

    scaled = (double)value * (double)scale;
    if (!(scaled > 0.0) || scaled > (double)INT_MAX) return FALSE;

    *outValue = (scaled < 1.0) ? 1 : (int)scaled;
    return TRUE;
}

BOOL ExtractMarkdownImage(const wchar_t** src, MarkdownImage* images,
                          int* imageCount, int imageCapacity, int currentPos) {
    if (!src || !*src || !images || !imageCount) return FALSE;
    if (*imageCount >= imageCapacity) return FALSE;

    const wchar_t* p = *src;
