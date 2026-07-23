#include "update_checker.h"
#include "update/update_internal.h"
#include "language.h"
#include "log.h"
#include "utils/url_safety.h"
#include "../../resource/resource.h"

#include <stdlib.h>
#include <string.h>

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

static char g_newVersion[VERSION_BUFFER_SIZE];
static BOOL g_newVersionAvailable;
static SRWLOCK g_newVersionLock = SRWLOCK_INIT;

static BOOL IsValidNotifyWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return FALSE;
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) return FALSE;

    wchar_t className[64] = {0};
    return GetClassNameW(hwnd, className, _countof(className)) > 0 &&
           wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static BOOL PostCheckResult(HWND hwnd, WPARAM result, LPARAM flags) {
    return IsValidNotifyWindow(hwnd) &&
           PostMessageW(hwnd, WM_UPDATE_CHECK_RESULT, result, flags) != 0;
}

static void SetNewVersionStatus(BOOL available, const char* version) {
    AcquireSRWLockExclusive(&g_newVersionLock);
    g_newVersionAvailable = available;
    if (available && version) {
        strncpy_s(g_newVersion, sizeof(g_newVersion), version, _TRUNCATE);
    } else {
        g_newVersion[0] = '\0';
    }
    ReleaseSRWLockExclusive(&g_newVersionLock);
}

BOOL GetNewVersionStatus(char* versionBuffer, size_t bufferSize) {
    AcquireSRWLockShared(&g_newVersionLock);
    BOOL available = g_newVersionAvailable;
    if (versionBuffer && bufferSize > 0) {
        if (available) {
            strncpy_s(versionBuffer, bufferSize, g_newVersion, _TRUNCATE);
        } else {
            versionBuffer[0] = '\0';
        }
    }
    ReleaseSRWLockShared(&g_newVersionLock);
    return available;
}

static const wchar_t* GetHttpErrorMessage(UpdateHttpResult result) {
    switch (result) {
        case UPDATE_HTTP_INIT_FAILED:
            return GetLocalizedString(
                NULL, L"Could not create Internet connection");
        case UPDATE_HTTP_CONNECT_FAILED:
            return GetLocalizedString(
                NULL, L"Could not connect to update server");
        case UPDATE_HTTP_READ_FAILED:
            return GetLocalizedString(NULL, L"Failed to read server response");
        default:
            return NULL;
    }
}

static BOOL ParseReleaseResponse(char* response, char* latestVersion,
                                 char* downloadUrl, char* releaseNotes) {
    BOOL parsed = ParseGitHubRelease(
        response, latestVersion, VERSION_BUFFER_SIZE,
        downloadUrl, URL_BUFFER_SIZE, releaseNotes, NOTES_BUFFER_SIZE);
    free(response);
    return parsed && IsSafeUpdateDownloadUrlA(downloadUrl);
}

void CheckForUpdateInternal(HWND hwnd, BOOL silentCheck) {
    if (!IsValidNotifyWindow(hwnd)) {
        LOG_WARNING("Update check skipped: invalid notification window");
        return;
    }

    char* response = NULL;
    UpdateHttpResult fetchResult = UpdateHttp_FetchRelease(&response);
    if (fetchResult != UPDATE_HTTP_OK) {
        const wchar_t* message = GetHttpErrorMessage(fetchResult);
        if (!silentCheck && message && !UpdateHttp_IsCancelRequested()) {
            ShowUpdateErrorDialog(hwnd, message);
        }
        return;
    }

    char latestVersion[VERSION_BUFFER_SIZE] = {0};
    char downloadUrl[URL_BUFFER_SIZE] = {0};
    char* releaseNotes = (char*)malloc(NOTES_BUFFER_SIZE);
    if (!releaseNotes) {
        free(response);
        LOG_ERROR("Failed to allocate release notes buffer");
        if (!silentCheck && !UpdateHttp_IsCancelRequested()) {
            ShowUpdateErrorDialog(
                hwnd, GetLocalizedString(
                    NULL, L"Could not parse version information"));
        }
        return;
    }
    releaseNotes[0] = '\0';

    if (!ParseReleaseResponse(response, latestVersion,
                              downloadUrl, releaseNotes)) {
        LOG_WARNING("Update response validation failed (silent=%d)",
                    silentCheck);
        free(releaseNotes);
        if (!silentCheck && !UpdateHttp_IsCancelRequested()) {
            ShowUpdateErrorDialog(
                hwnd, GetLocalizedString(
                    NULL, L"Could not parse version information"));
        }
        return;
    }
    if (UpdateHttp_IsCancelRequested()) {
        free(releaseNotes);
        return;
    }

    const char* currentVersion = CATIME_VERSION;
    if (CompareVersions(latestVersion, currentVersion) > 0) {
        StoreUpdateResult(TRUE, currentVersion, latestVersion,
                          downloadUrl, releaseNotes);
        SetNewVersionStatus(TRUE, latestVersion);
        if (!PostCheckResult(hwnd, 1, silentCheck ? 1 : 0)) {
            LOG_WARNING("Failed to post update-available result");
        }
    } else {
        SetNewVersionStatus(FALSE, NULL);
        if (!silentCheck) {
            StoreUpdateResult(FALSE, currentVersion, NULL, NULL, NULL);
            if (!PostCheckResult(hwnd, 0, 0)) {
                LOG_WARNING("Failed to post no-update result");
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
