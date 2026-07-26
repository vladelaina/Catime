/**
 * @file markdown_image.c
 * @brief Shared state and lifecycle for Markdown image support.
 */

#include "markdown_image_internal.h"
#include "log.h"
#include <stdlib.h>

wchar_t g_imageCacheDir[MAX_PATH] = {0};
volatile LONG g_imageCacheDirInit = IMAGE_CACHE_DIR_UNINITIALIZED;
wchar_t g_pluginsDir[MAX_PATH] = {0};
volatile LONG g_pluginsDirInit = IMAGE_CACHE_DIR_UNINITIALIZED;

unsigned long long g_downloadingHashes[MAX_DOWNLOADING] = {0};
HINTERNET g_activeDownloadHandles[MAX_ACTIVE_DOWNLOAD_HANDLES] = {0};
unsigned long long g_failedDownloadHashes[MAX_FAILED_DOWNLOADS] = {0};
DWORD g_failedDownloadRetryTicks[MAX_FAILED_DOWNLOADS] = {0};
SRWLOCK g_downloadLifecycleLock = SRWLOCK_INIT;
int g_downloadingCount = 0;
int g_failedDownloadCount = 0;
CRITICAL_SECTION g_downloadCS;
volatile LONG g_downloadCSInit = 0;
HANDLE g_downloadIdleEvent = NULL;
volatile LONG g_activeDownloadCount = 0;
volatile LONG g_downloadShutdown = 0;
volatile LONG g_downloadGeneration = 0;
volatile LONG g_downloadRestartPending = 0;
volatile LONG g_downloadInitFailureCooldownUntil = 0;

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
        DWORD waitResult = WaitForSingleObject(g_downloadIdleEvent,
                                               IMAGE_SHUTDOWN_GRACE_MS);
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
    ZeroMemory(g_failedDownloadRetryTicks,
               sizeof(g_failedDownloadRetryTicks));
    g_downloadingCount = 0;
    g_failedDownloadCount = 0;
    ClearDownloadInitFailure();
    InterlockedExchange(&g_downloadRestartPending, 0);
    InterlockedExchange(&g_activeDownloadCount, 0);
    ReleaseSRWLockExclusive(&g_downloadLifecycleLock);
}

void FreeMarkdownImageEntries(MarkdownImage* images, int imageCount) {
    if (!images) {
        return;
    }

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
    if (!images) {
        return;
    }
    FreeMarkdownImageEntries(images, imageCount);
    free(images);
}
