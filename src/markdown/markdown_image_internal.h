/**
 * @file markdown_image_internal.h
 * @brief Private state and helpers shared by Markdown image modules.
 */

#ifndef MARKDOWN_IMAGE_INTERNAL_H
#define MARKDOWN_IMAGE_INTERNAL_H

#include "markdown/markdown_image.h"
#include <wininet.h>

#define IMAGE_DOWNLOAD_TIMEOUT_MS 10000
#define IMAGE_DOWNLOAD_MAX_BYTES (10u * 1024u * 1024u)
#define IMAGE_CACHE_MAX_BYTES (128ull * 1024ull * 1024ull)
#define IMAGE_CACHE_MAX_FILES 256
#define IMAGE_CACHE_PRUNE_SCAN_LIMIT 4096
#define IMAGE_SHUTDOWN_GRACE_MS 15000
#define IMAGE_DOWNLOAD_FAILURE_RETRY_MS (5u * 60u * 1000u)
#define IMAGE_DOWNLOAD_QUEUE_RETRY_MS 1000
#define IMAGE_DOWNLOAD_INIT_FAILURE_COOLDOWN_MS 2000
#define IMAGE_DOWNLOAD_READ_BUFFER_SIZE 8192u
#define IMAGE_CACHE_DIR_UNINITIALIZED 0
#define IMAGE_CACHE_DIR_INITIALIZING 1
#define IMAGE_CACHE_DIR_INITIALIZED 2
#define INIT_WAIT_SPIN_LIMIT 64
#define MARKDOWN_IMAGE_PATH_MAX_CHARS 2047
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

#define MAX_DOWNLOADING 16
#define MAX_FAILED_DOWNLOADS 256
#define MAX_ACTIVE_DOWNLOAD_HANDLES (MAX_DOWNLOADING * 2)

typedef struct {
    wchar_t path[MAX_PATH];
    FILETIME lastWriteTime;
    ULONGLONG size;
} ImageCachePruneEntry;

typedef struct {
    wchar_t url[2048];
    wchar_t cachePath[MAX_PATH];
    HWND hwnd;
    LONG generation;
} AsyncDownloadParams;

extern wchar_t g_imageCacheDir[MAX_PATH];
extern volatile LONG g_imageCacheDirInit;
extern wchar_t g_pluginsDir[MAX_PATH];
extern volatile LONG g_pluginsDirInit;

extern unsigned long long g_downloadingHashes[MAX_DOWNLOADING];
extern HINTERNET g_activeDownloadHandles[MAX_ACTIVE_DOWNLOAD_HANDLES];
extern unsigned long long g_failedDownloadHashes[MAX_FAILED_DOWNLOADS];
extern DWORD g_failedDownloadRetryTicks[MAX_FAILED_DOWNLOADS];
extern SRWLOCK g_downloadLifecycleLock;
extern int g_downloadingCount;
extern int g_failedDownloadCount;
extern CRITICAL_SECTION g_downloadCS;
extern volatile LONG g_downloadCSInit;
extern HANDLE g_downloadIdleEvent;
extern volatile LONG g_activeDownloadCount;
extern volatile LONG g_downloadShutdown;
extern volatile LONG g_downloadGeneration;
extern volatile LONG g_downloadRestartPending;
extern volatile LONG g_downloadInitFailureCooldownUntil;

BOOL IsValidMarkdownImageNotifyWindow(HWND hwnd);
void WaitWhileLongEquals(volatile LONG* value, LONG expected);
BOOL EnsureDirectoryExistsW(const wchar_t* path);
BOOL GetExistingNonEmptyFileInfoW(const wchar_t* path,
                                  WIN32_FILE_ATTRIBUTE_DATA* attrsOut,
                                  ULONGLONG* sizeOut);
void ClearMarkdownImageResolvedFileState(MarkdownImage* image);
void StoreMarkdownImageResolvedFileState(
    MarkdownImage* image, const WIN32_FILE_ATTRIBUTE_DATA* attrs,
    ULONGLONG fileSize);
void FreeMarkdownImageResolvedPath(MarkdownImage* image);
BOOL SetImageRenderRect(MarkdownImage* image, int x, int y,
                        int width, int height);
void ParseImageSize(const wchar_t* sizeStr, size_t len,
                    int* width, int* height);
BOOL ScaleIntToInt(int value, float scale, int* outValue);

BOOL IsNetworkUrl(const wchar_t* path);
BOOL IsAbsolutePath(const wchar_t* path);
BOOL GetPluginRelativeImageBaseDirectory(wchar_t* buffer,
                                         size_t bufferSize);
unsigned long long HashUrl64(const wchar_t* url);
void GenerateCacheFilename(const wchar_t* url, wchar_t* filename,
                           size_t size);
BOOL IsUsableCachedImageFileW(const wchar_t* path);
void RemoveInvalidImageCacheEntryW(const wchar_t* path);

void PruneImageCacheDirectory(const wchar_t* cacheDir,
                              const wchar_t* keepPath);
BOOL DownloadImageToCacheForGeneration(const wchar_t* url,
                                       wchar_t* localPath,
                                       LONG generation);

BOOL IsDownloadShutdownRequested(void);
LONG GetDownloadGeneration(void);
BOOL IsDownloadGenerationCurrent(LONG generation);
BOOL IsDownloadCanceled(LONG generation);
BOOL TrackDownloadHandle(HINTERNET handle, LONG generation);
void CloseTrackedDownloadHandle(HINTERNET* handlePtr, LONG generation);
void RequestMarkdownImageDownloadCancel(void);
BOOL IsDownloadCSReady(void);
BOOL EnsureDownloadCSInit(void);
BOOL IsUrlDownloading(const wchar_t* url);
void ScheduleImageDownloadRetry(MarkdownImage* image, DWORD delayMs);
BOOL TryAddDownloadingUrl(const wchar_t* url);
void ClearUrlDownloadFailure(const wchar_t* url);
void MarkUrlDownloadFailed(const wchar_t* url);
void RemoveDownloadingUrl(const wchar_t* url);
BOOL MarkDownloadStarted(void);
void MarkDownloadFinished(void);
void ClearDownloadInitFailure(void);

#endif /* MARKDOWN_IMAGE_INTERNAL_H */
