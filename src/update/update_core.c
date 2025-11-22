/**
 * @file update_core.c
 * @brief Core update logic and orchestration
 */
#include "update_checker.h"
#include "update/update_internal.h"
#include "log.h"
#include "language.h"
#include "utils/string_convert.h"
#include "../../resource/resource.h"
#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "wininet.lib")

/* Thin wrappers using utils/string_convert.h */
static inline wchar_t* LocalUtf8ToWideAlloc(const char* utf8Str) {
    return Utf8ToWideAlloc(utf8Str);
}

static inline BOOL LocalUtf8ToWideFixed(const char* utf8Str, wchar_t* wideBuf, int bufSize) {
    return Utf8ToWide(utf8Str, wideBuf, (size_t)bufSize);
}

/** @brief Initialize HTTP session */
static BOOL InitHttpResources(HttpResources* res) {
    memset(res, 0, sizeof(HttpResources));
    
    wchar_t wUserAgent[256];
    LocalUtf8ToWideFixed(USER_AGENT, wUserAgent, 256);
    
    res->hInternet = InternetOpenW(wUserAgent, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!res->hInternet) {
        LOG_ERROR("Failed to create Internet session (error code: %lu)", GetLastError());
        return FALSE;
    }
    
    LOG_INFO("Internet session created successfully");
    return TRUE;
}

/** @brief Connect to GitHub API */
static BOOL ConnectToGitHub(HttpResources* res) {
    wchar_t wUrl[URL_BUFFER_SIZE];
    LocalUtf8ToWideFixed(GITHUB_API_URL, wUrl, URL_BUFFER_SIZE);
    
    res->hConnect = InternetOpenUrlW(res->hInternet, wUrl, NULL, 0,
                                     INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!res->hConnect) {
        LOG_ERROR("Failed to connect to GitHub API (error code: %lu)", GetLastError());
        return FALSE;
    }
    
    LOG_INFO("Successfully connected to GitHub API");
    return TRUE;
}

/** @brief Read HTTP response into dynamic buffer */
static BOOL ReadHttpResponse(HttpResources* res) {
    size_t bufferSize = INITIAL_HTTP_BUFFER_SIZE;
    res->buffer = (char*)malloc(bufferSize);
    if (!res->buffer) {
        LOG_ERROR("Memory allocation failed");
        return FALSE;
    }
    
    DWORD totalBytes = 0;
    DWORD bytesRead;
    
    while (InternetReadFile(res->hConnect, res->buffer + totalBytes,
                           bufferSize - totalBytes - 1, &bytesRead) && bytesRead > 0) {
        totalBytes += bytesRead;
        
        if (totalBytes >= bufferSize - 256) {
            size_t newSize = bufferSize * 2;
            char* newBuffer = (char*)realloc(res->buffer, newSize);
            if (!newBuffer) {
                LOG_ERROR("Buffer expansion failed (current size: %zu)", bufferSize);
                return FALSE;
            }
            res->buffer = newBuffer;
            bufferSize = newSize;
        }
    }
    
    res->buffer[totalBytes] = '\0';
    LOG_INFO("Successfully read API response (%lu bytes)", totalBytes);
    return TRUE;
}

/** @brief Clean up HTTP resources */
static void CleanupHttpResources(HttpResources* res) {
    if (res->buffer) {
        free(res->buffer);
        res->buffer = NULL;
    }
    if (res->hConnect) {
        InternetCloseHandle(res->hConnect);
        res->hConnect = NULL;
    }
    if (res->hInternet) {
        InternetCloseHandle(res->hInternet);
        res->hInternet = NULL;
    }
}

/**
 * @brief Open browser to download URL and exit app
 * @return TRUE if browser opened successfully
 */
static BOOL OpenBrowserAndExit(const char* url, HWND hwnd) {
    wchar_t* urlW = LocalUtf8ToWideAlloc(url);
    if (!urlW) return FALSE;
    
    HINSTANCE hInstance = ShellExecuteW(hwnd, L"open", urlW, NULL, NULL, SW_SHOWNORMAL);
    free(urlW);
    
    if ((INT_PTR)hInstance <= 32) {
        ShowUpdateErrorDialog(hwnd, 
            GetLocalizedString(NULL, L"Could not open browser to download update"));
        return FALSE;
    }
    
    LOG_INFO("Successfully opened browser, application will exit");
    ShowExitMessageDialog(hwnd);
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

/**
 * @brief Perform update check
 * @param silentCheck TRUE=only show if update found, FALSE=show all results
 */
void CheckForUpdateInternal(HWND hwnd, BOOL silentCheck) {
    LOG_INFO("Starting update check (silent mode: %s)", silentCheck ? "yes" : "no");
    
    HttpResources res;
    
    if (!InitHttpResources(&res)) {
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, 
                GetLocalizedString(NULL, L"Could not create Internet connection"));
        }
        return;
    }
    
    if (!ConnectToGitHub(&res)) {
        CleanupHttpResources(&res);
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, 
                GetLocalizedString(NULL, L"Could not connect to update server"));
        }
        return;
    }
    
    if (!ReadHttpResponse(&res)) {
        CleanupHttpResources(&res);
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, 
                GetLocalizedString(NULL, L"Failed to read server response"));
        }
        return;
    }
    
    char latestVersion[VERSION_BUFFER_SIZE] = {0};
    char downloadUrl[URL_BUFFER_SIZE] = {0};
    char releaseNotes[NOTES_BUFFER_SIZE] = {0};
    
    if (!ParseGitHubRelease(res.buffer, latestVersion, sizeof(latestVersion),
                           downloadUrl, sizeof(downloadUrl), releaseNotes, sizeof(releaseNotes))) {
        CleanupHttpResources(&res);
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, 
                GetLocalizedString(NULL, L"Could not parse version information"));
        }
        return;
    }
    
    CleanupHttpResources(&res);
    
    LOG_INFO("GitHub latest version: %s, download URL: %s", latestVersion, downloadUrl);
    
    const char* currentVersion = CATIME_VERSION;
    LOG_INFO("Current version: %s", currentVersion);
    
    int versionCompare = CompareVersions(latestVersion, currentVersion);
    if (versionCompare > 0) {
        LOG_INFO("New version found! Current: %s, Latest: %s", currentVersion, latestVersion);
        int response = ShowUpdateNotification(hwnd, currentVersion, latestVersion, downloadUrl, releaseNotes);
        
        if (response == IDYES) {
            LOG_INFO("User chose to update now");
            OpenBrowserAndExit(downloadUrl, hwnd);
        } else {
            LOG_INFO("User chose to update later");
        }
    } else {
        LOG_INFO("Already using latest version: %s", currentVersion);
        if (!silentCheck) {
            ShowNoUpdateDialog(hwnd, currentVersion);
        }
    }
    
    LOG_INFO("Update check completed");
}

void CheckForUpdate(HWND hwnd) {
    CheckForUpdateInternal(hwnd, FALSE);
}

void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck) {
    CheckForUpdateInternal(hwnd, silentCheck);
}
