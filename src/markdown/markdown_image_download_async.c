/**
 * @file markdown_image_download_async.c
 * @brief Background image download worker and request scheduling.
 */

#include "markdown_image_internal.h"
#include "log.h"
#include <stdlib.h>

static DWORD WINAPI AsyncDownloadThread(LPVOID param) {
    AsyncDownloadParams* params = (AsyncDownloadParams*)param;
    BOOL downloaded = DownloadImageToCacheForGeneration(
        params->url, params->cachePath, params->generation);

    RemoveDownloadingUrl(params->url);
    if (IsDownloadGenerationCurrent(params->generation)) {
        if (downloaded) {
            ClearUrlDownloadFailure(params->url);
        } else if (!IsDownloadShutdownRequested()) {
            MarkUrlDownloadFailed(params->url);
        }

        if (IsValidMarkdownImageNotifyWindow(params->hwnd)) {
            InvalidateRect(params->hwnd, NULL, FALSE);
        }
    }

    free(params);
    MarkDownloadFinished();
    return 0;
}

void StartAsyncImageDownload(MarkdownImage* image, HWND hwnd) {
    if (!image || !image->imagePath ||
        !IsNetworkUrl(image->imagePath)) {
        return;
    }
    if (wcslen(image->imagePath) > MARKDOWN_IMAGE_PATH_MAX_CHARS) {
        ScheduleImageDownloadRetry(image,
                                   IMAGE_DOWNLOAD_FAILURE_RETRY_MS);
        return;
    }

    AcquireSRWLockShared(&g_downloadLifecycleLock);
    if (IsDownloadShutdownRequested()) {
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    wchar_t cachePath[MAX_PATH];
    if (IsImageCached(image->imagePath, cachePath)) {
        FreeMarkdownImageResolvedPath(image);
        image->resolvedPath = _wcsdup(cachePath);
        if (!image->resolvedPath) {
            ReleaseSRWLockShared(&g_downloadLifecycleLock);
            return;
        }
        if (!RefreshMarkdownImageResolvedFileState(image)) {
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
    if (GetMarkdownImageDownloadRetryTick(image->imagePath,
                                          &retryTick)) {
        image->isDownloading = FALSE;
        image->downloadFailed = TRUE;
        image->downloadRetryScheduled = TRUE;
        image->downloadRetryTick = retryTick;
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    if (IsUrlDownloading(image->imagePath)) {
        image->isDownloading = TRUE;
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    image->isDownloading = TRUE;
    if (!TryAddDownloadingUrl(image->imagePath)) {
        ScheduleImageDownloadRetry(image, IMAGE_DOWNLOAD_QUEUE_RETRY_MS);
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    AsyncDownloadParams* params =
        (AsyncDownloadParams*)malloc(sizeof(AsyncDownloadParams));
    if (!params) {
        RemoveDownloadingUrl(image->imagePath);
        ScheduleImageDownloadRetry(image,
                                   IMAGE_DOWNLOAD_FAILURE_RETRY_MS);
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    wcsncpy(params->url, image->imagePath, 2047);
    params->url[2047] = L'\0';
    params->cachePath[0] = L'\0';
    params->hwnd = IsValidMarkdownImageNotifyWindow(hwnd) ? hwnd : NULL;
    params->generation = GetDownloadGeneration();

    if (!MarkDownloadStarted()) {
        free(params);
        RemoveDownloadingUrl(image->imagePath);
        ScheduleImageDownloadRetry(image,
                                   IMAGE_DOWNLOAD_FAILURE_RETRY_MS);
        ReleaseSRWLockShared(&g_downloadLifecycleLock);
        return;
    }

    HANDLE hThread = CreateThread(NULL, 0, AsyncDownloadThread,
                                  params, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        MarkDownloadFinished();
        free(params);
        RemoveDownloadingUrl(image->imagePath);
        MarkUrlDownloadFailed(image->imagePath);
        ScheduleImageDownloadRetry(image,
                                   IMAGE_DOWNLOAD_FAILURE_RETRY_MS);
    }
    ReleaseSRWLockShared(&g_downloadLifecycleLock);
}
