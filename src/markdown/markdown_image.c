/**
 * @file markdown_image.c
 * @brief Markdown image parsing and rendering
 */

#include "markdown/markdown_image.h"
#include "drawing/drawing_image.h"
#include "plugin/plugin_data.h"
#include "config.h"
#include "log.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wininet.h>

#ifdef _MSC_VER
#pragma comment(lib, "wininet.lib")
#endif

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

static BOOL IsDownloadShutdownRequested(void);
static BOOL IsDownloadCanceled(LONG generation);
static LONG GetDownloadGeneration(void);
static BOOL TrackDownloadHandle(HINTERNET handle, LONG generation);
static void CloseTrackedDownloadHandle(HINTERNET* handlePtr, LONG generation);
static void RequestMarkdownImageDownloadCancel(void);

static BOOL IsValidMarkdownImageNotifyWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static wchar_t g_imageCacheDir[MAX_PATH] = {0};
static volatile LONG g_imageCacheDirInit = IMAGE_CACHE_DIR_UNINITIALIZED;
static wchar_t g_pluginsDir[MAX_PATH] = {0};
static volatile LONG g_pluginsDirInit = IMAGE_CACHE_DIR_UNINITIALIZED;

static void WaitWhileLongEquals(volatile LONG* value, LONG expected) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(value, 0, 0) == expected) {
        Sleep(spins++ < INIT_WAIT_SPIN_LIMIT ? 0 : 1);
    }
}

static BOOL EnsureDirectoryExistsW(const wchar_t* path) {
    if (!path || !*path) return FALSE;

    if (CreateDirectoryW(path, NULL)) {
        return TRUE;
    }

    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        return FALSE;
    }

    DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL GetExistingNonEmptyFileInfoW(const wchar_t* path,
                                         WIN32_FILE_ATTRIBUTE_DATA* attrsOut,
                                         ULONGLONG* sizeOut) {
    if (!path || !*path) return FALSE;

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attrs)) {
        return FALSE;
    }
    if (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return FALSE;
    }

    ULONGLONG fileSize = ((ULONGLONG)attrs.nFileSizeHigh << 32) |
                         (ULONGLONG)attrs.nFileSizeLow;
    if (fileSize == 0) {
        return FALSE;
    }

    if (attrsOut) *attrsOut = attrs;
    if (sizeOut) *sizeOut = fileSize;
    return TRUE;
}

BOOL IsMarkdownImageFileUsable(const wchar_t* path) {
    return GetExistingNonEmptyFileInfoW(path, NULL, NULL);
}

static void ClearMarkdownImageResolvedFileState(MarkdownImage* image) {
    if (!image) return;

    image->intrinsicWidth = 0;
    image->intrinsicHeight = 0;
    ZeroMemory(&image->resolvedLastWriteTime, sizeof(image->resolvedLastWriteTime));
    image->resolvedFileSize = 0;
    image->resolvedFileInfoValid = FALSE;
}

static void StoreMarkdownImageResolvedFileState(MarkdownImage* image,
                                                const WIN32_FILE_ATTRIBUTE_DATA* attrs,
                                                ULONGLONG fileSize) {
    if (!image || !attrs) return;

    image->resolvedLastWriteTime = attrs->ftLastWriteTime;
    image->resolvedFileSize = fileSize;
    image->resolvedFileInfoValid = TRUE;
}

static void FreeMarkdownImageResolvedPath(MarkdownImage* image) {
    if (!image) return;

    if (image->resolvedPath) {
        free(image->resolvedPath);
        image->resolvedPath = NULL;
    }
    ClearMarkdownImageResolvedFileState(image);
}

BOOL RefreshMarkdownImageResolvedFileState(MarkdownImage* image) {
    if (!image || !image->resolvedPath) return FALSE;

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    ULONGLONG fileSize = 0;
    if (!GetExistingNonEmptyFileInfoW(image->resolvedPath, &attrs, &fileSize)) {
        ClearMarkdownImageResolvedFileState(image);
        return FALSE;
    }

    if (image->resolvedFileInfoValid &&
        (CompareFileTime(&image->resolvedLastWriteTime, &attrs.ftLastWriteTime) != 0 ||
         image->resolvedFileSize != fileSize)) {
        image->intrinsicWidth = 0;
        image->intrinsicHeight = 0;
    }

    StoreMarkdownImageResolvedFileState(image, &attrs, fileSize);
    return TRUE;
}

static BOOL IsUsableCachedImageFileW(const wchar_t* path) {
    ULONGLONG fileSize = 0;
    if (!GetExistingNonEmptyFileInfoW(path, NULL, &fileSize)) {
        return FALSE;
    }
    return fileSize <= IMAGE_DOWNLOAD_MAX_BYTES;
}

static void RemoveInvalidImageCacheEntryW(const wchar_t* path) {
    if (!path || !*path) return;

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attrs)) {
        return;
    }

    if (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        RemoveDirectoryW(path);
    } else {
        DeleteFileW(path);
    }
#include "markdown_image_part01.inc"
#include "markdown_image_part02.inc"
#include "markdown_image_part03.inc"
#include "markdown_image_part04.inc"
#include "markdown_image_part05.inc"
#include "markdown_image_part06.inc"
