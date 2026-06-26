/**
 * @file update_core.c
 * @brief Core update logic and orchestration
 */
#include "update_checker.h"
#include "update/update_internal.h"
#include "log.h"
#include "language.h"
#include "utils/string_convert.h"
#include "utils/url_safety.h"
#include "../../resource/resource.h"
#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma comment(lib, "wininet.lib")
#endif

#define MAX_HTTP_RESPONSE_SIZE (1024 * 1024)
#define HTTP_TIMEOUT_MS 10000
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

#define UPDATE_HTTP_CS_UNINITIALIZED 0
#define UPDATE_HTTP_CS_INITIALIZING  1
#define UPDATE_HTTP_CS_INITIALIZED   2
#define UPDATE_HTTP_WAIT_SPIN_LIMIT 64

static CRITICAL_SECTION g_updateHttpCS;
static volatile LONG g_updateHttpCSInitialized = UPDATE_HTTP_CS_UNINITIALIZED;
static volatile LONG g_updateCancelRequested = 0;
static HINTERNET g_activeInternet = NULL;
static HINTERNET g_activeConnect = NULL;

static BOOL IsValidUpdateNotifyWindow(HWND hwnd) {
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

static BOOL PostUpdateCheckResult(HWND hwnd, WPARAM result, LPARAM flags) {
    if (!IsValidUpdateNotifyWindow(hwnd)) {
        return FALSE;
    }

    return PostMessage(hwnd, WM_UPDATE_CHECK_RESULT, result, flags) != 0;
}

static void WaitWhileUpdateHttpCSInitializing(void) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(&g_updateHttpCSInitialized, 0, 0) ==
           UPDATE_HTTP_CS_INITIALIZING) {
        Sleep(spins++ < UPDATE_HTTP_WAIT_SPIN_LIMIT ? 0 : 1);
    }
}

static void EnsureUpdateHttpCSInitialized(void) {
    if (InterlockedCompareExchange(&g_updateHttpCSInitialized,
                                   UPDATE_HTTP_CS_INITIALIZING,
                                   UPDATE_HTTP_CS_UNINITIALIZED) == UPDATE_HTTP_CS_UNINITIALIZED) {
        InitializeCriticalSection(&g_updateHttpCS);
        InterlockedExchange(&g_updateHttpCSInitialized, UPDATE_HTTP_CS_INITIALIZED);
    }

    WaitWhileUpdateHttpCSInitializing();
}

static BOOL IsUpdateCancelRequested(void) {
    return InterlockedCompareExchange(&g_updateCancelRequested, 0, 0) != 0;
}

static void TrackInternetHandle(HINTERNET handle) {
    EnsureUpdateHttpCSInitialized();
    EnterCriticalSection(&g_updateHttpCS);
    g_activeInternet = handle;
    LeaveCriticalSection(&g_updateHttpCS);
}

static void TrackConnectHandle(HINTERNET handle) {
    EnsureUpdateHttpCSInitialized();
    EnterCriticalSection(&g_updateHttpCS);
    g_activeConnect = handle;
    LeaveCriticalSection(&g_updateHttpCS);
}

static void CloseTrackedInternetHandle(HINTERNET* handlePtr) {
    if (!handlePtr || !*handlePtr) return;

    HINTERNET handle = *handlePtr;
    BOOL shouldClose = FALSE;

    EnsureUpdateHttpCSInitialized();
    EnterCriticalSection(&g_updateHttpCS);
    if (g_activeConnect == handle) {
        g_activeConnect = NULL;
        shouldClose = TRUE;
    } else if (g_activeInternet == handle) {
        g_activeInternet = NULL;
        shouldClose = TRUE;
    }
    LeaveCriticalSection(&g_updateHttpCS);

    if (shouldClose) {
        InternetCloseHandle(handle);
    }
    *handlePtr = NULL;
}

void RequestUpdateCheckCancel(void) {
    HINTERNET connectHandle = NULL;
    HINTERNET internetHandle = NULL;

    InterlockedExchange(&g_updateCancelRequested, 1);

    EnsureUpdateHttpCSInitialized();
    EnterCriticalSection(&g_updateHttpCS);
    connectHandle = g_activeConnect;
    internetHandle = g_activeInternet;
    g_activeConnect = NULL;
    g_activeInternet = NULL;
    LeaveCriticalSection(&g_updateHttpCS);

    if (connectHandle) {
        InternetCloseHandle(connectHandle);
    }
    if (internetHandle) {
        InternetCloseHandle(internetHandle);
    }
}

void ResetUpdateCheckCancel(void) {
    InterlockedExchange(&g_updateCancelRequested, 0);
}

void CleanupUpdateCheckResources(void) {
    HINTERNET connectHandle = NULL;
    HINTERNET internetHandle = NULL;

    InterlockedExchange(&g_updateCancelRequested, 1);
    WaitWhileUpdateHttpCSInitializing();

    if (InterlockedCompareExchange(&g_updateHttpCSInitialized, 0, 0) !=
        UPDATE_HTTP_CS_INITIALIZED) {
        return;
    }

    EnterCriticalSection(&g_updateHttpCS);
    connectHandle = g_activeConnect;
    internetHandle = g_activeInternet;
    g_activeConnect = NULL;
    g_activeInternet = NULL;
    LeaveCriticalSection(&g_updateHttpCS);

    if (connectHandle) {
        InternetCloseHandle(connectHandle);
    }
    if (internetHandle) {
        InternetCloseHandle(internetHandle);
    }

    /*
     * Keep the HTTP critical section alive until process exit. The final
     * update-thread cleanup uses a finite wait; if WinINet does not return in
     * time, a late worker may still run its normal handle cleanup path.
     */
}

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
    TrackInternetHandle(res->hInternet);

    if (IsUpdateCancelRequested()) {
        CloseTrackedInternetHandle(&res->hInternet);
        return FALSE;
    }

    DWORD timeoutMs = HTTP_TIMEOUT_MS;
    InternetSetOptionW(res->hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(res->hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(res->hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    return TRUE;
}

/** @brief Connect to GitHub API */
static BOOL ConnectToGitHub(HttpResources* res) {
    wchar_t wUrl[URL_BUFFER_SIZE];
    LocalUtf8ToWideFixed(GITHUB_API_URL, wUrl, URL_BUFFER_SIZE);

    res->hConnect = InternetOpenUrlW(res->hInternet, wUrl, NULL, 0,
                                     INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!res->hConnect) {
        if (!IsUpdateCancelRequested()) {
            LOG_ERROR("Failed to connect to GitHub API (error code: %lu)", GetLastError());
        }
        return FALSE;
    }
    TrackConnectHandle(res->hConnect);

    if (IsUpdateCancelRequested()) {
        CloseTrackedInternetHandle(&res->hConnect);
        return FALSE;
    }

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

    size_t totalBytes = 0;
    const size_t maxBufferSize = (size_t)MAX_HTTP_RESPONSE_SIZE + 1;

    for (;;) {
        if (IsUpdateCancelRequested()) {
            return FALSE;
        }

        if (totalBytes >= MAX_HTTP_RESPONSE_SIZE) {
            char overflowByte = '\0';
            DWORD overflowRead = 0;
            if (!InternetReadFile(res->hConnect, &overflowByte, 1, &overflowRead)) {
                if (!IsUpdateCancelRequested()) {
                    LOG_ERROR("Failed while reading HTTP response (error code: %lu)", GetLastError());
                }
                return FALSE;
            }
            if (overflowRead > 0) {
                LOG_ERROR("HTTP response exceeded maximum allowed size (%d bytes)", MAX_HTTP_RESPONSE_SIZE);
                return FALSE;
            }
            break;
        }

        size_t writableBytes = bufferSize - totalBytes - 1;
        if (writableBytes == 0) {
            size_t newSize = bufferSize * 2;
            if (newSize > maxBufferSize) {
                newSize = maxBufferSize;
            }
            if (newSize <= bufferSize) {
                LOG_ERROR("HTTP response exceeded maximum allowed size (%d bytes)", MAX_HTTP_RESPONSE_SIZE);
                return FALSE;
            }

            char* newBuffer = (char*)realloc(res->buffer, newSize);
            if (!newBuffer) {
                LOG_ERROR("Buffer expansion failed (current size: %zu)", bufferSize);
                return FALSE;
            }
            res->buffer = newBuffer;
            bufferSize = newSize;
            writableBytes = bufferSize - totalBytes - 1;
        }

        size_t remainingAllowed = (size_t)MAX_HTTP_RESPONSE_SIZE - totalBytes;
        if (writableBytes > remainingAllowed) {
            writableBytes = remainingAllowed;
        }

        DWORD bytesRead = 0;
        if (!InternetReadFile(res->hConnect, res->buffer + totalBytes,
                              (DWORD)writableBytes, &bytesRead)) {
            if (!IsUpdateCancelRequested()) {
                LOG_ERROR("Failed while reading HTTP response (error code: %lu)", GetLastError());
            }
            return FALSE;
        }
        if (bytesRead == 0) {
            break;
        }

        totalBytes += bytesRead;
    }

    if (IsUpdateCancelRequested()) {
        return FALSE;
    }

    res->buffer[totalBytes] = '\0';
    return TRUE;
}

/** @brief Clean up HTTP resources */
static void CleanupHttpResources(HttpResources* res) {
    if (res->buffer) {
        free(res->buffer);
        res->buffer = NULL;
    }
    if (res->hConnect) {
        CloseTrackedInternetHandle(&res->hConnect);
    }
    if (res->hInternet) {
        CloseTrackedInternetHandle(&res->hInternet);
    }
}

static char g_newVersionString[32] = {0};
static BOOL g_isNewVersionAvailable = FALSE;
static SRWLOCK g_newVersionLock = SRWLOCK_INIT;

static void SetNewVersionStatus(BOOL available, const char* version) {
    AcquireSRWLockExclusive(&g_newVersionLock);
    g_isNewVersionAvailable = available;
    if (available && version) {
        strncpy_s(g_newVersionString, sizeof(g_newVersionString), version, _TRUNCATE);
    } else {
        g_newVersionString[0] = '\0';
    }
    ReleaseSRWLockExclusive(&g_newVersionLock);
}

BOOL GetNewVersionStatus(char* versionBuffer, size_t bufferSize) {
    AcquireSRWLockShared(&g_newVersionLock);
    BOOL available = g_isNewVersionAvailable;
    if (available && versionBuffer && bufferSize > 0) {
        strncpy_s(versionBuffer, bufferSize, g_newVersionString, _TRUNCATE);
    } else if (versionBuffer && bufferSize > 0) {
        versionBuffer[0] = '\0';
    }
    ReleaseSRWLockShared(&g_newVersionLock);
    return available;
}

/**
 * @brief Perform update check
 * @param silentCheck TRUE=suppress automatic check dialogs, FALSE=show manual results
 */
void CheckForUpdateInternal(HWND hwnd, BOOL silentCheck) {
    if (!IsValidUpdateNotifyWindow(hwnd)) {
        LOG_WARNING("Update check skipped: invalid notification window");
        return;
    }

    HttpResources res;

    if (!InitHttpResources(&res)) {
        if (!silentCheck && !IsUpdateCancelRequested()) {
            ShowUpdateErrorDialog(hwnd,
                GetLocalizedString(NULL, L"Could not create Internet connection"));
        }
        return;
    }

    if (!ConnectToGitHub(&res)) {
        CleanupHttpResources(&res);
        if (!silentCheck && !IsUpdateCancelRequested()) {
            ShowUpdateErrorDialog(hwnd,
                GetLocalizedString(NULL, L"Could not connect to update server"));
        }
        return;
    }

    if (!ReadHttpResponse(&res)) {
        CleanupHttpResources(&res);
        if (!silentCheck && !IsUpdateCancelRequested()) {
            ShowUpdateErrorDialog(hwnd,
                GetLocalizedString(NULL, L"Failed to read server response"));
        }
        return;
    }

    char latestVersion[VERSION_BUFFER_SIZE] = {0};
    char downloadUrl[URL_BUFFER_SIZE] = {0};
    char* releaseNotes = (char*)malloc(NOTES_BUFFER_SIZE);
    if (!releaseNotes) {
        CleanupHttpResources(&res);
        LOG_ERROR("Failed to allocate release notes buffer");
        if (!silentCheck && !IsUpdateCancelRequested()) {
            ShowUpdateErrorDialog(hwnd,
                GetLocalizedString(NULL, L"Could not parse version information"));
        }
        return;
    }
    releaseNotes[0] = '\0';

    if (!ParseGitHubRelease(res.buffer, latestVersion, sizeof(latestVersion),
                           downloadUrl, sizeof(downloadUrl), releaseNotes, NOTES_BUFFER_SIZE)) {
        LOG_WARNING("Update check parse failed (silent=%d)", silentCheck);
        free(releaseNotes);
        CleanupHttpResources(&res);
        if (!silentCheck && !IsUpdateCancelRequested()) {
            ShowUpdateErrorDialog(hwnd,
                GetLocalizedString(NULL, L"Could not parse version information"));
        }
        return;
    }

    if (!IsSafeUpdateDownloadUrlA(downloadUrl)) {
        LOG_WARNING("Update check rejected unsafe download URL: %s", downloadUrl);
        free(releaseNotes);
        CleanupHttpResources(&res);
        if (!silentCheck && !IsUpdateCancelRequested()) {
            ShowUpdateErrorDialog(hwnd,
                GetLocalizedString(NULL, L"Could not parse version information"));
        }
        return;
    }

    CleanupHttpResources(&res);

    if (IsUpdateCancelRequested()) {
        free(releaseNotes);
        return;
    }

    const char* currentVersion = CATIME_VERSION;

    int versionCompare = CompareVersions(latestVersion, currentVersion);
    if (versionCompare > 0) {
        StoreUpdateResult(TRUE, currentVersion, latestVersion, downloadUrl, releaseNotes);
        SetNewVersionStatus(TRUE, latestVersion);
        BOOL posted = PostUpdateCheckResult(hwnd, 1, silentCheck ? 1 : 0);
        if (!posted) {
            LOG_WARNING("Update check failed to post update-available result (silent=%d)", silentCheck);
        }
    } else {
        SetNewVersionStatus(FALSE, NULL);
        if (!silentCheck) {
            StoreUpdateResult(FALSE, currentVersion, NULL, NULL, NULL);
            BOOL posted = PostUpdateCheckResult(hwnd, 0, 0);
            if (!posted) {
                LOG_WARNING("Update check failed to post no-update result");
            }
        }
    }

    free(releaseNotes);
}

void CheckForUpdate(HWND hwnd) {
    ResetUpdateCheckCancel();
    CheckForUpdateInternal(hwnd, FALSE);
}

void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck) {
    ResetUpdateCheckCancel();
    CheckForUpdateInternal(hwnd, silentCheck);
}
