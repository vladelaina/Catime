#ifndef HTTP_DOWNLOADER_H
#define HTTP_DOWNLOADER_H

#include <windows.h>

/**
 * @brief Check if a string is a URL (starts with http:// or https://)
 */
BOOL IsHttpUrl(const wchar_t* path);

/**
 * @brief Generate a local cache path for a given URL inside TEMP folder
 * @param url The URL to hash
 * @param buffer Output buffer for the local path
 * @param maxLen Size of buffer
 */
void GetLocalCachePath(const wchar_t* url, wchar_t* buffer, size_t maxLen);

/**
 * @brief Download a file asynchronously
 * @param url Source URL
 * @param localPath Destination path
 * @param hwndNotify Optional window to notify (via InvalidateRect) when done
 */
void DownloadFileAsync(const wchar_t* url, const wchar_t* localPath, HWND hwndNotify);

#endif
