
    wchar_t tempPath[MAX_PATH] = {0};
    if (GetTempFileNameW(cacheDir, L"cti", 0, tempPath) == 0) {
        if (!IsDownloadCanceled(generation)) {
            LOG_ERROR("Failed to create temporary image cache file in: %ls", cacheDir);
        }
        CloseTrackedDownloadHandle(&hUrl, generation);
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }

    /* Create temporary file; publish to final cache path only after success. */
    HANDLE hFile = CreateFileW(tempPath, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (!IsDownloadCanceled(generation)) {
            LOG_ERROR("Failed to create temporary cache file: %ls", tempPath);
        }
        DeleteFileW(tempPath);
        CloseTrackedDownloadHandle(&hUrl, generation);
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }

    /* Download and write */
    BYTE* buffer = (BYTE*)malloc(IMAGE_DOWNLOAD_READ_BUFFER_SIZE);
    if (!buffer) {
        LOG_ERROR("Failed to allocate image download buffer");
        CloseHandle(hFile);
        DeleteFileW(tempPath);
        CloseTrackedDownloadHandle(&hUrl, generation);
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }
    DWORD bytesRead, bytesWritten;
    DWORD totalBytes = 0;
    BOOL success = TRUE;

    while (!IsDownloadCanceled(generation)) {
        bytesRead = 0;
        if (!InternetReadFile(hUrl, buffer, IMAGE_DOWNLOAD_READ_BUFFER_SIZE, &bytesRead)) {
            if (!IsDownloadCanceled(generation)) {
                LOG_ERROR("Failed while reading image URL: %ls", url);
            }
            success = FALSE;
            break;
        }
        if (bytesRead == 0) {
            break;
        }

        if (bytesRead > IMAGE_DOWNLOAD_MAX_BYTES - totalBytes) {
            LOG_ERROR("Image too large (>10MB)");
            success = FALSE;
            break;
        }

        if (!WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL) ||
            bytesWritten != bytesRead) {
            success = FALSE;
            break;
        }
        totalBytes += bytesRead;
    }
    if (IsDownloadCanceled(generation)) {
        success = FALSE;
    }

    if (success && !FlushFileBuffers(hFile)) {
        LOG_ERROR("Failed to flush temporary image cache file: %ls", tempPath);
        success = FALSE;
    }
    if (!CloseHandle(hFile)) {
        LOG_ERROR("Failed to close temporary image cache file: %ls", tempPath);
        success = FALSE;
    }
    CloseTrackedDownloadHandle(&hUrl, generation);
    CloseTrackedDownloadHandle(&hInternet, generation);
    free(buffer);

    if (success && totalBytes > 0 &&
        MoveFileExW(tempPath, localPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        PruneImageCacheDirectory(cacheDir, localPath);
        return TRUE;
    } else {
        /* Delete incomplete file */
        DeleteFileW(tempPath);
        localPath[0] = L'\0';
        return FALSE;
    }
}

BOOL DownloadImageToCache(const wchar_t* url, wchar_t* localPath) {
    LONG generation = GetDownloadGeneration();
    return DownloadImageToCacheForGeneration(url, localPath, generation);
}

/**
 * @brief Check if image is already cached
 */
BOOL IsImageCached(const wchar_t* url, wchar_t* localPath) {
    if (!url || !localPath) return FALSE;
    localPath[0] = L'\0';

    wchar_t cacheDir[MAX_PATH];
    if (!GetImageCacheDirectory(cacheDir, MAX_PATH)) return FALSE;

    wchar_t filename[64];
    GenerateCacheFilename(url, filename, 64);

    int pathWritten = _snwprintf_s(localPath, MAX_PATH, _TRUNCATE, L"%s\\%s", cacheDir, filename);
    if (pathWritten < 0) {
        localPath[0] = L'\0';
        return FALSE;
    }

    if (!IsUsableCachedImageFileW(localPath)) {
        localPath[0] = L'\0';
        return FALSE;
    }

    return TRUE;
}

/* ============================================================================
 * Async Download
 * ============================================================================ */

/* Track URLs currently being downloaded (compact URL hash set) */
#define MAX_DOWNLOADING 16
#define MAX_FAILED_DOWNLOADS 256
#define MAX_ACTIVE_DOWNLOAD_HANDLES (MAX_DOWNLOADING * 2)
static unsigned long long g_downloadingHashes[MAX_DOWNLOADING] = {0};
static HINTERNET g_activeDownloadHandles[MAX_ACTIVE_DOWNLOAD_HANDLES] = {0};
static unsigned long long g_failedDownloadHashes[MAX_FAILED_DOWNLOADS] = {0};
static DWORD g_failedDownloadRetryTicks[MAX_FAILED_DOWNLOADS] = {0};
static SRWLOCK g_downloadLifecycleLock = SRWLOCK_INIT;
static int g_downloadingCount = 0;
static int g_failedDownloadCount = 0;
static CRITICAL_SECTION g_downloadCS;
static volatile LONG g_downloadCSInit = 0;
static HANDLE g_downloadIdleEvent = NULL;
static volatile LONG g_activeDownloadCount = 0;
static volatile LONG g_downloadShutdown = 0;
static volatile LONG g_downloadGeneration = 0;
static volatile LONG g_downloadRestartPending = 0;
static volatile LONG g_downloadInitFailureCooldownUntil = 0;

static BOOL IsDownloadShutdownRequested(void) {
    return InterlockedCompareExchange(&g_downloadShutdown, 0, 0) != 0;
}

static LONG GetDownloadGeneration(void) {
    return InterlockedCompareExchange(&g_downloadGeneration, 0, 0);
}

static BOOL IsDownloadGenerationCurrent(LONG generation) {
    return GetDownloadGeneration() == generation;
}

static BOOL IsDownloadCanceled(LONG generation) {
    return IsDownloadShutdownRequested() || !IsDownloadGenerationCurrent(generation);
}

static BOOL IsDownloadInitFailureCoolingDown(DWORD now) {
    DWORD cooldownUntil =
        (DWORD)InterlockedCompareExchange(&g_downloadInitFailureCooldownUntil, 0, 0);
    return cooldownUntil != 0 && (LONG)(cooldownUntil - now) > 0;
}

static void MarkDownloadInitFailure(DWORD now) {
    DWORD cooldownUntil = now + IMAGE_DOWNLOAD_INIT_FAILURE_COOLDOWN_MS;
    InterlockedExchange(&g_downloadInitFailureCooldownUntil,
                        (LONG)(cooldownUntil ? cooldownUntil : 1));
}

static void ClearDownloadInitFailure(void) {
    InterlockedExchange(&g_downloadInitFailureCooldownUntil, 0);
}

static LONG GetDownloadCSInitState(void) {
    return InterlockedCompareExchange(&g_downloadCSInit, 0, 0);
}

static BOOL IsDownloadCSReady(void) {
    return GetDownloadCSInitState() == 2;
}

static BOOL EnsureDownloadCSInit(void) {
    if (IsDownloadShutdownRequested()) return FALSE;

    if (IsDownloadCSReady()) {
        return TRUE;
    }

    DWORD now = GetTickCount();
    if (IsDownloadInitFailureCoolingDown(now)) {
        return FALSE;
    }

    if (InterlockedCompareExchange(&g_downloadCSInit, 1, 0) == 0) {
        InitializeCriticalSection(&g_downloadCS);
        g_downloadIdleEvent = CreateEventW(NULL, TRUE, TRUE, NULL);
        if (!g_downloadIdleEvent) {
            DeleteCriticalSection(&g_downloadCS);
            InterlockedExchange(&g_downloadCSInit, 0);
            MarkDownloadInitFailure(now);
            return FALSE;
        }
        ClearDownloadInitFailure();
        InterlockedExchange(&g_downloadCSInit, 2);
    }
    /* Wait for initialization to complete */
    WaitWhileLongEquals(&g_downloadCSInit, 1);
    return !IsDownloadShutdownRequested() && IsDownloadCSReady() && g_downloadIdleEvent != NULL;
}

static BOOL TrackDownloadHandle(HINTERNET handle, LONG generation) {
    if (!handle) return FALSE;

    if (IsDownloadCanceled(generation) || !EnsureDownloadCSInit()) return FALSE;
    EnterCriticalSection(&g_downloadCS);
    if (IsDownloadCanceled(generation)) {
        LeaveCriticalSection(&g_downloadCS);
        return FALSE;
    }
    for (int i = 0; i < MAX_ACTIVE_DOWNLOAD_HANDLES; i++) {
        if (!g_activeDownloadHandles[i]) {
            g_activeDownloadHandles[i] = handle;
            LeaveCriticalSection(&g_downloadCS);
            return TRUE;
        }
    }
    LeaveCriticalSection(&g_downloadCS);
    return FALSE;
}

static void CloseTrackedDownloadHandle(HINTERNET* handlePtr, LONG generation) {
    if (!handlePtr || !*handlePtr) return;

    HINTERNET handle = *handlePtr;
    BOOL found = FALSE;

    if (IsDownloadCSReady()) {
        EnterCriticalSection(&g_downloadCS);
        for (int i = 0; i < MAX_ACTIVE_DOWNLOAD_HANDLES; i++) {
            if (g_activeDownloadHandles[i] == handle) {
                g_activeDownloadHandles[i] = NULL;
                found = TRUE;
                break;
            }
        }
        LeaveCriticalSection(&g_downloadCS);
    }

    if (found || !IsDownloadCanceled(generation)) {
        InternetCloseHandle(handle);
    }
    *handlePtr = NULL;
}
