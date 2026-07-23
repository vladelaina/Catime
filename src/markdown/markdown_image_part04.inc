
static void RequestMarkdownImageDownloadCancel(void) {
    HINTERNET handles[MAX_ACTIVE_DOWNLOAD_HANDLES];
    int handleCount = 0;

    InterlockedExchange(&g_downloadShutdown, 1);

    WaitWhileLongEquals(&g_downloadCSInit, 1);

    if (InterlockedCompareExchange(&g_downloadCSInit, 0, 0) != 2) {
        return;
    }

    ZeroMemory(handles, sizeof(handles));

    EnterCriticalSection(&g_downloadCS);
    for (int i = 0; i < MAX_ACTIVE_DOWNLOAD_HANDLES; i++) {
        if (g_activeDownloadHandles[i]) {
            handles[handleCount++] = g_activeDownloadHandles[i];
            g_activeDownloadHandles[i] = NULL;
        }
    }
    LeaveCriticalSection(&g_downloadCS);

    for (int i = 0; i < handleCount; i++) {
        InternetCloseHandle(handles[i]);
    }
}

static BOOL IsUrlDownloading(const wchar_t* url) {
    if (!IsDownloadCSReady()) return FALSE;
    unsigned long long hash = HashUrl64(url);

    EnterCriticalSection(&g_downloadCS);
    BOOL found = FALSE;
    for (int i = 0; i < g_downloadingCount; i++) {
        if (g_downloadingHashes[i] == hash) {
            found = TRUE;
            break;
        }
    }
    LeaveCriticalSection(&g_downloadCS);
    return found;
}

BOOL IsMarkdownImageDownloadInProgress(const wchar_t* url) {
    if (!url || !*url) return FALSE;
    return IsUrlDownloading(url);
}

static void ScheduleImageDownloadRetry(MarkdownImage* image, DWORD delayMs) {
    if (!image) return;

    image->isDownloading = FALSE;
    image->downloadFailed = TRUE;
    image->downloadRetryScheduled = TRUE;
    image->downloadRetryTick = GetTickCount() + delayMs;
}

static BOOL TryAddDownloadingUrl(const wchar_t* url) {
    if (!EnsureDownloadCSInit()) return FALSE;

    unsigned long long hash = HashUrl64(url);
    BOOL added = FALSE;
    EnterCriticalSection(&g_downloadCS);
    for (int i = 0; i < g_downloadingCount; i++) {
        if (g_downloadingHashes[i] == hash) {
            LeaveCriticalSection(&g_downloadCS);
            return FALSE;
        }
    }
    if (g_downloadingCount < MAX_DOWNLOADING) {
        g_downloadingHashes[g_downloadingCount++] = hash;
        added = TRUE;
    }
    LeaveCriticalSection(&g_downloadCS);
    return added;
}

static void PruneFailedDownloadEntriesLocked(DWORD now) {
    int i = 0;
    while (i < g_failedDownloadCount) {
        if ((LONG)(g_failedDownloadRetryTicks[i] - now) <= 0) {
            g_failedDownloadHashes[i] = g_failedDownloadHashes[--g_failedDownloadCount];
            g_failedDownloadRetryTicks[i] = g_failedDownloadRetryTicks[g_failedDownloadCount];
            continue;
        }
        i++;
    }
}

BOOL GetMarkdownImageDownloadRetryTick(const wchar_t* url, DWORD* retryTick) {
    if (retryTick) *retryTick = 0;
    if (!url || !*url || !IsDownloadCSReady()) return FALSE;

    unsigned long long hash = HashUrl64(url);
    DWORD now = GetTickCount();
    BOOL found = FALSE;

    EnterCriticalSection(&g_downloadCS);
    PruneFailedDownloadEntriesLocked(now);
    for (int i = 0; i < g_failedDownloadCount; i++) {
        if (g_failedDownloadHashes[i] == hash) {
            if (retryTick) {
                *retryTick = g_failedDownloadRetryTicks[i];
            }
            found = TRUE;
            break;
        }
    }
    LeaveCriticalSection(&g_downloadCS);
    return found;
}

static void ClearUrlDownloadFailure(const wchar_t* url) {
    if (!IsDownloadCSReady()) return;

    unsigned long long hash = HashUrl64(url);
    EnterCriticalSection(&g_downloadCS);
    for (int i = 0; i < g_failedDownloadCount; i++) {
        if (g_failedDownloadHashes[i] == hash) {
            g_failedDownloadHashes[i] = g_failedDownloadHashes[--g_failedDownloadCount];
            g_failedDownloadRetryTicks[i] = g_failedDownloadRetryTicks[g_failedDownloadCount];
            break;
        }
    }
    LeaveCriticalSection(&g_downloadCS);
}

static void MarkUrlDownloadFailed(const wchar_t* url) {
    if (!EnsureDownloadCSInit()) return;

    unsigned long long hash = HashUrl64(url);
    DWORD now = GetTickCount();
    DWORD retryTick = now + IMAGE_DOWNLOAD_FAILURE_RETRY_MS;
    retryTick = retryTick ? retryTick : 1;

    EnterCriticalSection(&g_downloadCS);
    PruneFailedDownloadEntriesLocked(now);

    for (int i = 0; i < g_failedDownloadCount; i++) {
        if (g_failedDownloadHashes[i] == hash) {
            g_failedDownloadRetryTicks[i] = retryTick;
            LeaveCriticalSection(&g_downloadCS);
            return;
        }
    }

    if (g_failedDownloadCount < MAX_FAILED_DOWNLOADS) {
        int idx = g_failedDownloadCount++;
        g_failedDownloadHashes[idx] = hash;
        g_failedDownloadRetryTicks[idx] = retryTick;
    } else {
        int soonestRetry = 0;
        for (int i = 1; i < g_failedDownloadCount; i++) {
            if ((LONG)(g_failedDownloadRetryTicks[soonestRetry] -
                       g_failedDownloadRetryTicks[i]) > 0) {
                soonestRetry = i;
            }
        }
        g_failedDownloadHashes[soonestRetry] = hash;
        g_failedDownloadRetryTicks[soonestRetry] = retryTick;
    }

    LeaveCriticalSection(&g_downloadCS);
}

static void RemoveDownloadingUrl(const wchar_t* url) {
    if (!IsDownloadCSReady()) return;
    unsigned long long hash = HashUrl64(url);

    EnterCriticalSection(&g_downloadCS);
    for (int i = 0; i < g_downloadingCount; i++) {
        if (g_downloadingHashes[i] == hash) {
            g_downloadingHashes[i] = g_downloadingHashes[--g_downloadingCount];
            break;
        }
    }
    LeaveCriticalSection(&g_downloadCS);
}

static BOOL MarkDownloadStarted(void) {
    if (!EnsureDownloadCSInit()) return FALSE;
    InterlockedIncrement(&g_activeDownloadCount);
    if (g_downloadIdleEvent) {
        ResetEvent(g_downloadIdleEvent);
    }
    return TRUE;
}

static void MarkDownloadFinished(void) {
    if (InterlockedDecrement(&g_activeDownloadCount) == 0) {
        if (InterlockedExchange(&g_downloadRestartPending, 0) != 0) {
            InterlockedIncrement(&g_downloadGeneration);
            InterlockedExchange(&g_downloadShutdown, 0);
        }
        if (g_downloadIdleEvent) {
            SetEvent(g_downloadIdleEvent);
        }
    }
}

typedef struct {
    wchar_t url[2048];
    wchar_t cachePath[MAX_PATH];
    HWND hwnd;
    LONG generation;
} AsyncDownloadParams;

static DWORD WINAPI AsyncDownloadThread(LPVOID param) {
    AsyncDownloadParams* p = (AsyncDownloadParams*)param;

    /* Download synchronously in background */
    BOOL downloaded = DownloadImageToCacheForGeneration(p->url, p->cachePath,
                                                       p->generation);

    /* Remove from downloading list */
    RemoveDownloadingUrl(p->url);
    if (IsDownloadGenerationCurrent(p->generation)) {
        if (downloaded) {
            ClearUrlDownloadFailure(p->url);
        } else if (!IsDownloadShutdownRequested()) {
            MarkUrlDownloadFailed(p->url);
        }

        /* Trigger window repaint */
        if (IsValidMarkdownImageNotifyWindow(p->hwnd)) {
            InvalidateRect(p->hwnd, NULL, FALSE);
        }
    }

    free(p);
    MarkDownloadFinished();
    return 0;
}

void StartAsyncImageDownload(MarkdownImage* image, HWND hwnd) {
    if (!image || !image->imagePath) return;
    if (!IsNetworkUrl(image->imagePath)) return;
    if (wcslen(image->imagePath) > MARKDOWN_IMAGE_PATH_MAX_CHARS) {
        ScheduleImageDownloadRetry(image, IMAGE_DOWNLOAD_FAILURE_RETRY_MS);
        return;
    }

    AcquireSRWLockShared(&g_downloadLifecycleLock);
    if (IsDownloadShutdownRequested()) {
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    /* Check if already cached */
    wchar_t cachePath[MAX_PATH];
    if (IsImageCached(image->imagePath, cachePath)) {
        FreeMarkdownImageResolvedPath(image);
        image->resolvedPath = _wcsdup(cachePath);
        if (!image->resolvedPath) {
            ReleaseSRWLockShared(&g_downloadLifecycleLock);
            return;  /* Memory allocation failed */
        }
        if (!RefreshMarkdownImageResolvedFileState(image)) {
