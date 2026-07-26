/**
 * @file markdown_image_download_sync.c
 * @brief Bounded synchronous download into the Markdown image cache.
 */

#include "markdown_image_internal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma comment(lib, "wininet.lib")
#endif

BOOL DownloadImageToCacheForGeneration(const wchar_t* url,
                                       wchar_t* localPath,
                                       LONG generation) {
    if (!url || !localPath) {
        return FALSE;
    }
    localPath[0] = L'\0';
    if (IsDownloadCanceled(generation)) {
        return FALSE;
    }

    wchar_t cacheDir[MAX_PATH];
    if (!GetImageCacheDirectory(cacheDir, MAX_PATH)) {
        LOG_ERROR("Failed to get image cache directory");
        return FALSE;
    }

    wchar_t filename[64];
    GenerateCacheFilename(url, filename, 64);
    int pathWritten = _snwprintf_s(localPath, MAX_PATH, _TRUNCATE,
                                   L"%s\\%s", cacheDir, filename);
    if (pathWritten < 0) {
        localPath[0] = L'\0';
        return FALSE;
    }

    if (IsUsableCachedImageFileW(localPath)) {
        return TRUE;
    }
    RemoveInvalidImageCacheEntryW(localPath);
    if (GetFileAttributesW(localPath) != INVALID_FILE_ATTRIBUTES) {
        localPath[0] = L'\0';
        return FALSE;
    }

    HINTERNET hInternet = InternetOpenW(
        L"Catime/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
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
    InternetSetOptionW(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT,
                       &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(hInternet, INTERNET_OPTION_SEND_TIMEOUT,
                       &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT,
                       &timeoutMs, sizeof(timeoutMs));

    HINTERNET hUrl = InternetOpenUrlW(
        hInternet, url, NULL, 0,
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
    if (!HttpQueryInfoW(hUrl,
                        HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
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
            LOG_ERROR("Failed to create temporary image cache file in: %ls",
                      cacheDir);
        }
        CloseTrackedDownloadHandle(&hUrl, generation);
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }

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

    BYTE* buffer = (BYTE*)malloc(IMAGE_DOWNLOAD_READ_BUFFER_SIZE);
    if (!buffer) {
        LOG_ERROR("Failed to allocate image download buffer");
        CloseHandle(hFile);
        DeleteFileW(tempPath);
        CloseTrackedDownloadHandle(&hUrl, generation);
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }

    DWORD bytesRead;
    DWORD bytesWritten;
    DWORD totalBytes = 0;
    BOOL success = TRUE;
    while (!IsDownloadCanceled(generation)) {
        bytesRead = 0;
        if (!InternetReadFile(hUrl, buffer,
                              IMAGE_DOWNLOAD_READ_BUFFER_SIZE,
                              &bytesRead)) {
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
        MoveFileExW(tempPath, localPath,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        PruneImageCacheDirectory(cacheDir, localPath);
        return TRUE;
    }

    DeleteFileW(tempPath);
    localPath[0] = L'\0';
    return FALSE;
}

BOOL DownloadImageToCache(const wchar_t* url, wchar_t* localPath) {
    return DownloadImageToCacheForGeneration(
        url, localPath, GetDownloadGeneration());
}

BOOL IsImageCached(const wchar_t* url, wchar_t* localPath) {
    if (!url || !localPath) {
        return FALSE;
    }
    localPath[0] = L'\0';

    wchar_t cacheDir[MAX_PATH];
    if (!GetImageCacheDirectory(cacheDir, MAX_PATH)) {
        return FALSE;
    }
    wchar_t filename[64];
    GenerateCacheFilename(url, filename, 64);
    int pathWritten = _snwprintf_s(localPath, MAX_PATH, _TRUNCATE,
                                   L"%s\\%s", cacheDir, filename);
    if (pathWritten < 0 || !IsUsableCachedImageFileW(localPath)) {
        localPath[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}
