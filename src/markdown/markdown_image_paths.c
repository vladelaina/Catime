/**
 * @file markdown_image_paths.c
 * @brief Markdown image directory, URL, and path resolution helpers.
 */

#include "markdown_image_internal.h"
#include "plugin/plugin_data.h"
#include "config.h"
#include "log.h"
#include <stdio.h>

BOOL GetPluginsDirectory(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }
    buffer[0] = L'\0';

    LONG initState = InterlockedCompareExchange(&g_pluginsDirInit, 0, 0);
    if (initState == IMAGE_CACHE_DIR_INITIALIZED) {
        if (wcslen(g_pluginsDir) >= bufferSize) {
            return FALSE;
        }
        wcscpy_s(buffer, bufferSize, g_pluginsDir);
        return TRUE;
    }

    if (InterlockedCompareExchange(&g_pluginsDirInit,
                                   IMAGE_CACHE_DIR_INITIALIZING,
                                   IMAGE_CACHE_DIR_UNINITIALIZED) !=
        IMAGE_CACHE_DIR_UNINITIALIZED) {
        WaitWhileLongEquals(&g_pluginsDirInit,
                            IMAGE_CACHE_DIR_INITIALIZING);
        if (InterlockedCompareExchange(&g_pluginsDirInit, 0, 0) !=
            IMAGE_CACHE_DIR_INITIALIZED) {
            return FALSE;
        }
        if (wcslen(g_pluginsDir) >= bufferSize) {
            return FALSE;
        }
        wcscpy_s(buffer, bufferSize, g_pluginsDir);
        return TRUE;
    }

    char pluginsDirUtf8[MAX_PATH] = {0};
    GetPluginsFolderPath(pluginsDirUtf8, MAX_PATH);
    if (pluginsDirUtf8[0] == L'\0' ||
        MultiByteToWideChar(CP_UTF8, 0, pluginsDirUtf8, -1,
                            g_pluginsDir, MAX_PATH) <= 0) {
        InterlockedExchange(&g_pluginsDirInit,
                            IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }

    InterlockedExchange(&g_pluginsDirInit, IMAGE_CACHE_DIR_INITIALIZED);
    if (wcslen(g_pluginsDir) >= bufferSize) {
        return FALSE;
    }
    wcscpy_s(buffer, bufferSize, g_pluginsDir);
    return TRUE;
}

BOOL GetImageCacheDirectory(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }
    buffer[0] = L'\0';

    LONG initState = InterlockedCompareExchange(&g_imageCacheDirInit, 0, 0);
    if (initState == IMAGE_CACHE_DIR_INITIALIZED) {
        if (wcslen(g_imageCacheDir) >= bufferSize) {
            return FALSE;
        }
        wcscpy_s(buffer, bufferSize, g_imageCacheDir);
        return TRUE;
    }

    if (InterlockedCompareExchange(&g_imageCacheDirInit,
                                   IMAGE_CACHE_DIR_INITIALIZING,
                                   IMAGE_CACHE_DIR_UNINITIALIZED) !=
        IMAGE_CACHE_DIR_UNINITIALIZED) {
        WaitWhileLongEquals(&g_imageCacheDirInit,
                            IMAGE_CACHE_DIR_INITIALIZING);
        if (InterlockedCompareExchange(&g_imageCacheDirInit, 0, 0) !=
            IMAGE_CACHE_DIR_INITIALIZED) {
            return FALSE;
        }
        if (wcslen(g_imageCacheDir) >= bufferSize) {
            return FALSE;
        }
        wcscpy_s(buffer, bufferSize, g_imageCacheDir);
        return TRUE;
    }

    wchar_t tempDir[MAX_PATH];
    DWORD result = GetTempPathW(MAX_PATH, tempDir);
    if (result == 0 || result >= MAX_PATH) {
        InterlockedExchange(&g_imageCacheDirInit,
                            IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }

    wchar_t catimeDir[MAX_PATH];
    int catimeWritten = _snwprintf_s(catimeDir, MAX_PATH, _TRUNCATE,
                                     L"%sCatime", tempDir);
    if (catimeWritten < 0 || !EnsureDirectoryExistsW(catimeDir)) {
        InterlockedExchange(&g_imageCacheDirInit,
                            IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }

    wchar_t imageDir[MAX_PATH];
    int imageDirWritten = _snwprintf_s(imageDir, MAX_PATH, _TRUNCATE,
                                       L"%sCatime\\images", tempDir);
    if (imageDirWritten < 0 || !EnsureDirectoryExistsW(imageDir)) {
        InterlockedExchange(&g_imageCacheDirInit,
                            IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }

    wcscpy_s(g_imageCacheDir, MAX_PATH, imageDir);
    InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_INITIALIZED);
    if (wcslen(g_imageCacheDir) >= bufferSize) {
        return FALSE;
    }
    wcscpy_s(buffer, bufferSize, g_imageCacheDir);
    return TRUE;
}

BOOL IsNetworkUrl(const wchar_t* path) {
    if (!path) {
        return FALSE;
    }
    return _wcsnicmp(path, L"http://", 7) == 0 ||
           _wcsnicmp(path, L"https://", 8) == 0;
}

BOOL IsAbsolutePath(const wchar_t* path) {
    if (!path || path[0] == L'\0' || path[1] == L'\0') {
        return FALSE;
    }
    return path[1] == L':' ||
           (path[0] == L'\\' && path[1] == L'\\');
}

BOOL GetPluginRelativeImageBaseDirectory(wchar_t* buffer,
                                         size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }
    buffer[0] = L'\0';

    wchar_t sourcePath[MAX_PATH];
    if (!PluginData_GetDisplaySourcePath(sourcePath, MAX_PATH)) {
        return FALSE;
    }

    wchar_t* lastBackslash = wcsrchr(sourcePath, L'\\');
    wchar_t* lastSlash = wcsrchr(sourcePath, L'/');
    wchar_t* lastSeparator = lastBackslash;
    if (lastSlash && (!lastSeparator || lastSlash > lastSeparator)) {
        lastSeparator = lastSlash;
    }
    if (lastSeparator && lastSeparator > sourcePath) {
        *lastSeparator = L'\0';
        if (wcslen(sourcePath) < bufferSize) {
            wcscpy_s(buffer, bufferSize, sourcePath);
            return TRUE;
        }
    }
    return FALSE;
}

unsigned long long HashUrl64(const wchar_t* url) {
    unsigned long long hash = 1469598103934665603ULL;
    const wchar_t* p = url;
    while (*p) {
        hash ^= (unsigned int)(*p);
        hash *= 1099511628211ULL;
        p++;
    }
    return hash == 0ULL ? 1ULL : hash;
}

void GenerateCacheFilename(const wchar_t* url, wchar_t* filename,
                           size_t size) {
    unsigned long long hash = HashUrl64(url);
    const wchar_t* ext = L".png";
    const wchar_t* lastDot = wcsrchr(url, L'.');
    if (lastDot) {
        if (_wcsnicmp(lastDot, L".jpg", 4) == 0 ||
            _wcsnicmp(lastDot, L".jpeg", 5) == 0) {
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

BOOL IsUsableCachedImageFileW(const wchar_t* path) {
    ULONGLONG fileSize = 0;
    if (!GetExistingNonEmptyFileInfoW(path, NULL, &fileSize)) {
        return FALSE;
    }
    return fileSize <= IMAGE_DOWNLOAD_MAX_BYTES;
}

void RemoveInvalidImageCacheEntryW(const wchar_t* path) {
    if (!path || !*path) {
        return;
    }
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

BOOL ResolveImagePath(MarkdownImage* image) {
    if (!image || !image->imagePath) {
        return FALSE;
    }
    FreeMarkdownImageResolvedPath(image);
    const wchar_t* path = image->imagePath;

    if (IsNetworkUrl(path)) {
        image->isNetworkImage = TRUE;
        wchar_t cachePath[MAX_PATH];
        if (IsImageCached(path, cachePath)) {
            image->resolvedPath = _wcsdup(cachePath);
            if (!image->resolvedPath ||
                !RefreshMarkdownImageResolvedFileState(image)) {
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
        return FALSE;
    }

    image->isNetworkImage = FALSE;
    if (IsAbsolutePath(path)) {
        WIN32_FILE_ATTRIBUTE_DATA attrs;
        ULONGLONG fileSize = 0;
        if (!GetExistingNonEmptyFileInfoW(path, &attrs, &fileSize)) {
            return FALSE;
        }
        image->resolvedPath = _wcsdup(path);
        if (!image->resolvedPath) {
            return FALSE;
        }
        StoreMarkdownImageResolvedFileState(image, &attrs, fileSize);
        return TRUE;
    }

    wchar_t baseDir[MAX_PATH];
    if (!GetPluginRelativeImageBaseDirectory(baseDir, MAX_PATH)) {
        return FALSE;
    }
    wchar_t fullPath[MAX_PATH];
    int written = _snwprintf_s(fullPath, MAX_PATH, _TRUNCATE,
                               L"%s\\%s", baseDir, path);
    if (written < 0) {
        return FALSE;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    ULONGLONG fileSize = 0;
    if (!GetExistingNonEmptyFileInfoW(fullPath, &attrs, &fileSize)) {
        return FALSE;
    }
    image->resolvedPath = _wcsdup(fullPath);
    if (!image->resolvedPath) {
        return FALSE;
    }
    StoreMarkdownImageResolvedFileState(image, &attrs, fileSize);
    return TRUE;
}
