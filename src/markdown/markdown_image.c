/**
 * @file markdown_image.c
 * @brief Markdown image parsing and rendering
 */

#include "markdown/markdown_image.h"
#include "drawing/drawing_image.h"
#include "plugin/plugin_data.h"
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
}

static BOOL SetImageRenderRect(MarkdownImage* image, int x, int y,
                               int width, int height) {
    if (!image || width <= 0 || height <= 0) return FALSE;
    if (x > INT_MAX - width || y > INT_MAX - height) return FALSE;

    image->imageRect.left = x;
    image->imageRect.top = y;
    image->imageRect.right = x + width;
    image->imageRect.bottom = y + height;
    return TRUE;
}

/* ============================================================================
 * Path Resolution
 * ============================================================================ */

BOOL GetPluginsDirectory(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return FALSE;
    buffer[0] = L'\0';

    LONG initState = InterlockedCompareExchange(&g_pluginsDirInit, 0, 0);
    if (initState == IMAGE_CACHE_DIR_INITIALIZED) {
        if (wcslen(g_pluginsDir) >= bufferSize) return FALSE;
        wcscpy_s(buffer, bufferSize, g_pluginsDir);
        return TRUE;
    }

    if (InterlockedCompareExchange(&g_pluginsDirInit,
                                   IMAGE_CACHE_DIR_INITIALIZING,
                                   IMAGE_CACHE_DIR_UNINITIALIZED) !=
        IMAGE_CACHE_DIR_UNINITIALIZED) {
        WaitWhileLongEquals(&g_pluginsDirInit, IMAGE_CACHE_DIR_INITIALIZING);

        if (InterlockedCompareExchange(&g_pluginsDirInit, 0, 0) !=
            IMAGE_CACHE_DIR_INITIALIZED) {
            return FALSE;
        }

        if (wcslen(g_pluginsDir) >= bufferSize) return FALSE;
        wcscpy_s(buffer, bufferSize, g_pluginsDir);
        return TRUE;
    }

    wchar_t pluginsDir[MAX_PATH];
    DWORD result = ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Catime\\resources\\plugins",
        pluginsDir, MAX_PATH);
    if (result == 0 || result >= MAX_PATH) {
        InterlockedExchange(&g_pluginsDirInit, IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }

    wcscpy_s(g_pluginsDir, MAX_PATH, pluginsDir);
    InterlockedExchange(&g_pluginsDirInit, IMAGE_CACHE_DIR_INITIALIZED);

    if (wcslen(g_pluginsDir) >= bufferSize) return FALSE;
    wcscpy_s(buffer, bufferSize, g_pluginsDir);
    return TRUE;
}

BOOL GetImageCacheDirectory(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return FALSE;
    buffer[0] = L'\0';

    LONG initState = InterlockedCompareExchange(&g_imageCacheDirInit, 0, 0);
    if (initState == IMAGE_CACHE_DIR_INITIALIZED) {
        if (wcslen(g_imageCacheDir) >= bufferSize) return FALSE;
        wcscpy_s(buffer, bufferSize, g_imageCacheDir);
        return TRUE;
    }

    if (InterlockedCompareExchange(&g_imageCacheDirInit,
                                   IMAGE_CACHE_DIR_INITIALIZING,
                                   IMAGE_CACHE_DIR_UNINITIALIZED) !=
        IMAGE_CACHE_DIR_UNINITIALIZED) {
        WaitWhileLongEquals(&g_imageCacheDirInit, IMAGE_CACHE_DIR_INITIALIZING);

        if (InterlockedCompareExchange(&g_imageCacheDirInit, 0, 0) !=
            IMAGE_CACHE_DIR_INITIALIZED) {
            return FALSE;
        }

        if (wcslen(g_imageCacheDir) >= bufferSize) return FALSE;
        wcscpy_s(buffer, bufferSize, g_imageCacheDir);
        return TRUE;
    }

    /* Use system temp directory: %TEMP%\Catime\images */
    wchar_t tempDir[MAX_PATH];
    DWORD result = GetTempPathW(MAX_PATH, tempDir);
    if (result == 0 || result >= MAX_PATH) {
        InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }

    /* Build path: %TEMP%\Catime\images */
    wchar_t catimeDir[MAX_PATH];
    int catimeWritten = _snwprintf_s(catimeDir, MAX_PATH, _TRUNCATE, L"%sCatime", tempDir);
    if (catimeWritten < 0) {
        InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }
    if (!EnsureDirectoryExistsW(catimeDir)) {
        InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }

    wchar_t imageDir[MAX_PATH];
    int imageDirWritten = _snwprintf_s(imageDir, MAX_PATH, _TRUNCATE, L"%sCatime\\images", tempDir);
    if (imageDirWritten < 0) {
        InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }
    if (!EnsureDirectoryExistsW(imageDir)) {
        InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }

    wcscpy_s(g_imageCacheDir, MAX_PATH, imageDir);
    InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_INITIALIZED);

    if (wcslen(g_imageCacheDir) >= bufferSize) return FALSE;
    wcscpy_s(buffer, bufferSize, g_imageCacheDir);

    return TRUE;
}

/**
 * @brief Check if path is a network URL
 */
static BOOL IsNetworkUrl(const wchar_t* path) {
    if (!path) return FALSE;
    return (_wcsnicmp(path, L"http://", 7) == 0 ||
            _wcsnicmp(path, L"https://", 8) == 0);
}

/**
 * @brief Check if path is absolute
 */
static BOOL IsAbsolutePath(const wchar_t* path) {
    if (!path || path[0] == L'\0' || path[1] == L'\0') return FALSE;
    /* Check for drive letter (C:\...) or UNC path (\\...) */
    return ((path[1] == L':') || (path[0] == L'\\' && path[1] == L'\\'));
}

/**
 * @brief Generate cache filename from URL
 */
static unsigned long long HashUrl64(const wchar_t* url) {
    /* 64-bit FNV-1a keeps cache names compact while avoiding easy 32-bit collisions. */
    unsigned long long hash = 1469598103934665603ULL;
    const wchar_t* p = url;
    while (*p) {
        hash ^= (unsigned int)(*p);
        hash *= 1099511628211ULL;
        p++;
    }
    if (hash == 0ULL) hash = 1ULL;
    return hash;
}

static void GenerateCacheFilename(const wchar_t* url, wchar_t* filename, size_t size) {
    unsigned long long hash = HashUrl64(url);

    /* Extract extension from URL */
    const wchar_t* ext = L".png";  /* Default */
    const wchar_t* lastDot = wcsrchr(url, L'.');
    if (lastDot) {
        if (_wcsnicmp(lastDot, L".jpg", 4) == 0 || _wcsnicmp(lastDot, L".jpeg", 5) == 0) {
            ext = L".jpg";
        } else if (_wcsnicmp(lastDot, L".gif", 4) == 0) {
            ext = L".gif";
        } else if (_wcsnicmp(lastDot, L".bmp", 4) == 0) {
            ext = L".bmp";
        } else if (_wcsnicmp(lastDot, L".webp", 5) == 0) {
            ext = L".webp";
        }
    }

    _snwprintf_s(filename, size, _TRUNCATE, L"%016llX%s", hash, ext);
}

typedef struct {
    wchar_t path[MAX_PATH];
    FILETIME lastWriteTime;
    ULONGLONG size;
} ImageCachePruneEntry;

static BOOL IsHexDigitW(wchar_t ch) {
    return (ch >= L'0' && ch <= L'9') ||
           (ch >= L'a' && ch <= L'f') ||
           (ch >= L'A' && ch <= L'F');
}

static BOOL IsGeneratedImageCacheFileName(const wchar_t* name) {
    if (!name) return FALSE;

    for (int i = 0; i < 16; i++) {
        if (!IsHexDigitW(name[i])) {
            return FALSE;
        }
    }

    const wchar_t* ext = name + 16;
    return _wcsicmp(ext, L".png") == 0 ||
           _wcsicmp(ext, L".jpg") == 0 ||
           _wcsicmp(ext, L".gif") == 0 ||
           _wcsicmp(ext, L".bmp") == 0 ||
           _wcsicmp(ext, L".webp") == 0;
}

static int CompareImageCachePruneEntryByAge(const void* lhs, const void* rhs) {
    const ImageCachePruneEntry* a = (const ImageCachePruneEntry*)lhs;
    const ImageCachePruneEntry* b = (const ImageCachePruneEntry*)rhs;
    return CompareFileTime(&a->lastWriteTime, &b->lastWriteTime);
}

static void PruneImageCacheDirectory(const wchar_t* cacheDir, const wchar_t* keepPath) {
    if (!cacheDir || !*cacheDir) return;

    wchar_t searchPath[MAX_PATH];
    if (_snwprintf_s(searchPath, MAX_PATH, _TRUNCATE, L"%s\\*", cacheDir) < 0) {
        return;
    }

    ImageCachePruneEntry* entries = NULL;
    int entryCount = 0;
    int entryCapacity = 0;
    int cacheFileCount = 0;
    ULONGLONG cacheBytes = 0;

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        if (!IsGeneratedImageCacheFileName(findData.cFileName)) {
            continue;
        }

        wchar_t fullPath[MAX_PATH];
        if (_snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s",
                         cacheDir, findData.cFileName) < 0) {
            continue;
        }

        ULONGLONG fileSize = ((ULONGLONG)findData.nFileSizeHigh << 32) |
                             (ULONGLONG)findData.nFileSizeLow;
        cacheFileCount++;
        cacheBytes += fileSize;

        if (keepPath && _wcsicmp(fullPath, keepPath) == 0) {
            continue;
        }
        if (entryCount >= IMAGE_CACHE_PRUNE_SCAN_LIMIT) {
            continue;
        }

        if (entryCount == entryCapacity) {
            int newCapacity = entryCapacity == 0 ? 64 : entryCapacity * 2;
            if (newCapacity > IMAGE_CACHE_PRUNE_SCAN_LIMIT) {
                newCapacity = IMAGE_CACHE_PRUNE_SCAN_LIMIT;
            }
            ImageCachePruneEntry* newEntries =
                (ImageCachePruneEntry*)realloc(entries,
                    (size_t)newCapacity * sizeof(ImageCachePruneEntry));
            if (!newEntries) {
                break;
            }
            entries = newEntries;
            entryCapacity = newCapacity;
        }

        ImageCachePruneEntry* entry = &entries[entryCount++];
        wcscpy_s(entry->path, MAX_PATH, fullPath);
        entry->lastWriteTime = findData.ftLastWriteTime;
        entry->size = fileSize;
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);

    if ((cacheBytes <= IMAGE_CACHE_MAX_BYTES &&
         cacheFileCount <= IMAGE_CACHE_MAX_FILES) ||
        !entries || entryCount == 0) {
        free(entries);
        return;
    }

    qsort(entries, (size_t)entryCount, sizeof(entries[0]),
          CompareImageCachePruneEntryByAge);

    int removedCount = 0;
    for (int i = 0;
         i < entryCount &&
         (cacheBytes > IMAGE_CACHE_MAX_BYTES ||
          cacheFileCount > IMAGE_CACHE_MAX_FILES);
         i++) {
        if (DeleteFileW(entries[i].path)) {
            if (entries[i].size <= cacheBytes) {
                cacheBytes -= entries[i].size;
            } else {
                cacheBytes = 0;
            }
            if (cacheFileCount > 0) {
                cacheFileCount--;
            }
            removedCount++;
        }
    }

    if (removedCount > 0) {
        LOG_INFO("Pruned %d cached markdown image file(s)", removedCount);
    }

    free(entries);
}

/* ============================================================================
 * Network Image Download
 * ============================================================================ */

static BOOL DownloadImageToCacheForGeneration(const wchar_t* url, wchar_t* localPath,
                                              LONG generation) {
    if (!url || !localPath) return FALSE;
    localPath[0] = L'\0';
    if (IsDownloadCanceled(generation)) return FALSE;

    /* Get cache directory */
    wchar_t cacheDir[MAX_PATH];
    if (!GetImageCacheDirectory(cacheDir, MAX_PATH)) {
        LOG_ERROR("Failed to get image cache directory");
        return FALSE;
    }

    /* Generate cache filename */
    wchar_t filename[64];
    GenerateCacheFilename(url, filename, 64);

    /* Build full cache path */
    int pathWritten = _snwprintf_s(localPath, MAX_PATH, _TRUNCATE, L"%s\\%s", cacheDir, filename);
    if (pathWritten < 0) {
        localPath[0] = L'\0';
        return FALSE;
    }

    /* Check if already cached */
    if (IsUsableCachedImageFileW(localPath)) {
        return TRUE;
    }
    RemoveInvalidImageCacheEntryW(localPath);
    if (GetFileAttributesW(localPath) != INVALID_FILE_ATTRIBUTES) {
        localPath[0] = L'\0';
        return FALSE;
    }

    /* Open Internet session */
    HINTERNET hInternet = InternetOpenW(L"Catime/1.0", INTERNET_OPEN_TYPE_DIRECT,
                                         NULL, NULL, 0);
    if (!hInternet) {
        if (!IsDownloadCanceled(generation)) {
            LOG_ERROR("Failed to open Internet session");
        }
        return FALSE;
    }
    if (!TrackDownloadHandle(hInternet, generation)) {
        InternetCloseHandle(hInternet);
        return FALSE;
    }
    if (IsDownloadCanceled(generation)) {
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }

    DWORD timeoutMs = IMAGE_DOWNLOAD_TIMEOUT_MS;
    InternetSetOptionW(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    /* Open URL */
    HINTERNET hUrl = InternetOpenUrlW(hInternet, url, NULL, 0,
                                       INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hUrl) {
        if (!IsDownloadCanceled(generation)) {
            LOG_ERROR("Failed to open URL: %ls", url);
        }
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }
    if (!TrackDownloadHandle(hUrl, generation)) {
        InternetCloseHandle(hUrl);
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                        &statusCode, &statusCodeSize, NULL) ||
        statusCode < 200 || statusCode >= 300) {
        if (!IsDownloadCanceled(generation)) {
            LOG_WARNING("Image download returned HTTP status %lu: %ls",
                        statusCode, url);
        }
        CloseTrackedDownloadHandle(&hUrl, generation);
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }

    if (IsDownloadCanceled(generation)) {
        CloseTrackedDownloadHandle(&hUrl, generation);
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }

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
static DWORD g_failedDownloadTicks[MAX_FAILED_DOWNLOADS] = {0};
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
static volatile LONG g_downloadInitLastFailureTick = 0;

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
    DWORD lastFailureTick =
        (DWORD)InterlockedCompareExchange(&g_downloadInitLastFailureTick, 0, 0);
    return lastFailureTick != 0 &&
           (DWORD)(now - lastFailureTick) <
               IMAGE_DOWNLOAD_INIT_FAILURE_COOLDOWN_MS;
}

static void MarkDownloadInitFailure(DWORD now) {
    InterlockedExchange(&g_downloadInitLastFailureTick, (LONG)(now ? now : 1));
}

static void ClearDownloadInitFailure(void) {
    InterlockedExchange(&g_downloadInitLastFailureTick, 0);
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
        if ((DWORD)(now - g_failedDownloadTicks[i]) >= IMAGE_DOWNLOAD_FAILURE_RETRY_MS) {
            g_failedDownloadHashes[i] = g_failedDownloadHashes[--g_failedDownloadCount];
            g_failedDownloadTicks[i] = g_failedDownloadTicks[g_failedDownloadCount];
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
                *retryTick = g_failedDownloadTicks[i] + IMAGE_DOWNLOAD_FAILURE_RETRY_MS;
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
            g_failedDownloadTicks[i] = g_failedDownloadTicks[g_failedDownloadCount];
            break;
        }
    }
    LeaveCriticalSection(&g_downloadCS);
}

static void MarkUrlDownloadFailed(const wchar_t* url) {
    if (!EnsureDownloadCSInit()) return;

    unsigned long long hash = HashUrl64(url);
    DWORD now = GetTickCount();

    EnterCriticalSection(&g_downloadCS);
    PruneFailedDownloadEntriesLocked(now);

    for (int i = 0; i < g_failedDownloadCount; i++) {
        if (g_failedDownloadHashes[i] == hash) {
            g_failedDownloadTicks[i] = now;
            LeaveCriticalSection(&g_downloadCS);
            return;
        }
    }

    if (g_failedDownloadCount < MAX_FAILED_DOWNLOADS) {
        int idx = g_failedDownloadCount++;
        g_failedDownloadHashes[idx] = hash;
        g_failedDownloadTicks[idx] = now;
    } else {
        int oldest = 0;
        for (int i = 1; i < g_failedDownloadCount; i++) {
            if ((DWORD)(now - g_failedDownloadTicks[i]) >
                (DWORD)(now - g_failedDownloadTicks[oldest])) {
                oldest = i;
            }
        }
        g_failedDownloadHashes[oldest] = hash;
        g_failedDownloadTicks[oldest] = now;
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

    /* Relative path - resolve relative to plugins directory */
    wchar_t pluginsDir[MAX_PATH];
    if (!GetPluginsDirectory(pluginsDir, MAX_PATH)) {
        return FALSE;
    }

    wchar_t fullPath[MAX_PATH];
    int written = _snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", pluginsDir, path);
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
    ZeroMemory(g_failedDownloadTicks, sizeof(g_failedDownloadTicks));
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
