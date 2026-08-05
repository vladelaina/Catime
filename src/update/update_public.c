#include "update_ui_state.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_procedure.h"
#include "language.h"
#include "log.h"
#include "utils/string_convert.h"
#include "utils/url_safety.h"
#ifdef CATIME_USE_WIN32_FLS
#include "utils/thread_local_buffer.h"
#endif
#include "../../resource/resource.h"

#include <shellapi.h>
#include <stdlib.h>

void ShowExitMessageDialog(HWND hwnd) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_EXIT_MSG)) {
        SetForegroundWindow(
            Dialog_GetInstance(DIALOG_INSTANCE_EXIT_MSG));
        return;
    }
    if (!UpdateUi_IsValidParent(hwnd)) return;
    HWND dialog = CreateDialogW(
        GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_EXIT_DIALOG),
        hwnd, ExitMsgDlgProc);
    if (dialog) ShowWindow(dialog, SW_SHOW);
}

int ShowUpdateNotification(HWND hwnd, const char* currentVersion,
                           const char* latestVersion,
                           const char* downloadUrl,
                           const char* releaseNotes) {
    if (!UpdateUi_IsValidParent(hwnd)) {
        LOG_WARNING("Update dialog not shown: invalid parent window");
        return IDNO;
    }
    UpdateUi_CloseDialog(DIALOG_INSTANCE_NO_UPDATE);
    UpdateUi_CloseDialog(DIALOG_INSTANCE_UPDATE);
    UpdateUi_CopyString(g_dialogCurrentVersion,
                        sizeof(g_dialogCurrentVersion), currentVersion);
    UpdateUi_CopyString(g_dialogLatestVersion,
                        sizeof(g_dialogLatestVersion), latestVersion);
    UpdateUi_CopyString(g_dialogDownloadUrl,
                        sizeof(g_dialogDownloadUrl), downloadUrl);
    UpdateUi_CopyString(g_dialogReleaseNotes,
                        sizeof(g_dialogReleaseNotes), releaseNotes);
    g_updateVersionInfo.currentVersion = g_dialogCurrentVersion;
    g_updateVersionInfo.latestVersion = g_dialogLatestVersion;
    g_updateVersionInfo.downloadUrl = g_dialogDownloadUrl;
    g_updateVersionInfo.releaseNotes = g_dialogReleaseNotes;

    AcquireSRWLockExclusive(&g_downloadUrlLock);
    UpdateUi_CopyString(g_downloadUrlCopy, sizeof(g_downloadUrlCopy),
                        downloadUrl);
    ReleaseSRWLockExclusive(&g_downloadUrlLock);

    HWND dialog = CreateDialogW(
        GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_UPDATE_DIALOG),
        hwnd, UpdateDlgProc);
    if (dialog) {
        ShowWindow(dialog, SW_SHOW);
    } else {
        LOG_WARNING("Update dialog creation failed (error=%lu)",
                    GetLastError());
    }
    return IDNO;
}

void ShowUpdateErrorDialog(HWND hwnd, const wchar_t* errorMsg) {
    UNREFERENCED_PARAMETER(hwnd);
    char message[512] = {0};
    if (errorMsg &&
        WideToUtf8(errorMsg, message, sizeof(message))) {
        LOG_WARNING("Update check failed: %s", message);
    } else {
        LOG_WARNING("Update check failed");
    }
}

void ShowNoUpdateDialog(HWND hwnd, const char* currentVersion) {
    UpdateUi_CloseDialog(DIALOG_INSTANCE_UPDATE);
    UpdateUi_ClearPendingUrl();
    if (Dialog_IsOpen(DIALOG_INSTANCE_NO_UPDATE)) {
        SetForegroundWindow(
            Dialog_GetInstance(DIALOG_INSTANCE_NO_UPDATE));
        return;
    }
    if (!UpdateUi_IsValidParent(hwnd)) {
        LOG_WARNING("No-update dialog not shown: invalid parent window");
        return;
    }
    UpdateUi_CopyString(g_noUpdateVersion, sizeof(g_noUpdateVersion),
                        currentVersion);
    HWND dialog = CreateDialogW(
        GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_NO_UPDATE_DIALOG),
        hwnd, NoUpdateDlgProc);
    if (dialog) {
        ShowWindow(dialog, SW_SHOW);
    } else {
        LOG_WARNING("No-update dialog creation failed (error=%lu)",
                    GetLastError());
    }
}

const char* GetPendingUpdateDownloadUrl(void) {
#if defined(CATIME_USE_WIN32_FLS)
    char* snapshot = (char*)ThreadLocalBuffer_Get(
        &g_downloadUrlSnapshotStorage);
    if (!snapshot) return NULL;
#elif defined(_MSC_VER)
    __declspec(thread) static char snapshot[sizeof(g_downloadUrlCopy)];
#elif defined(__GNUC__)
    static __thread char snapshot[sizeof(g_downloadUrlCopy)];
#else
    static char snapshot[sizeof(g_downloadUrlCopy)];
#endif
    return UpdateUi_CopyPendingUrl(
        snapshot, sizeof(g_downloadUrlCopy)) ? snapshot : NULL;
}

void TriggerUpdateDownload(HWND hwnd) {
    char url[sizeof(g_downloadUrlCopy)] = {0};
    if (!UpdateUi_CopyPendingUrl(url, sizeof(url))) return;
    if (!IsSafeUpdateDownloadUrlA(url)) {
        LOG_ERROR("Blocked unsafe update URL: %s", url);
        ShowUpdateErrorDialog(
            hwnd, GetLocalizedString(NULL, L"Unsafe download URL was blocked"));
        return;
    }

    wchar_t* wideUrl = Utf8ToWideAlloc(url);
    if (wideUrl) {
        ShellExecuteW(NULL, L"open", wideUrl, NULL, NULL, SW_SHOWNORMAL);
        free(wideUrl);
    }
    g_shouldExitAfterDialog = TRUE;
    ShowExitMessageDialog(hwnd);
}

static char g_storedCurrentVersion[64];
static char g_storedLatestVersion[64];
static char g_storedDownloadUrl[URL_BUFFER_SIZE];
static char g_storedReleaseNotes[NOTES_BUFFER_SIZE];
static BOOL g_storedHasUpdate;
static SRWLOCK g_storedUpdateLock = SRWLOCK_INIT;

void StoreUpdateResult(BOOL hasUpdate, const char* currentVersion,
                       const char* latestVersion, const char* downloadUrl,
                       const char* releaseNotes) {
    AcquireSRWLockExclusive(&g_storedUpdateLock);
    g_storedHasUpdate = hasUpdate;
    UpdateUi_CopyString(g_storedCurrentVersion,
                        sizeof(g_storedCurrentVersion), currentVersion);
    UpdateUi_CopyString(g_storedLatestVersion,
                        sizeof(g_storedLatestVersion), latestVersion);
    UpdateUi_CopyString(g_storedDownloadUrl,
                        sizeof(g_storedDownloadUrl), downloadUrl);
    UpdateUi_CopyString(g_storedReleaseNotes,
                        sizeof(g_storedReleaseNotes), releaseNotes);
    ReleaseSRWLockExclusive(&g_storedUpdateLock);
}

void ShowStoredUpdateDialog(HWND hwnd) {
    char current[64];
    char latest[64];
    char url[URL_BUFFER_SIZE];
    char* notes = (char*)malloc(NOTES_BUFFER_SIZE);
    AcquireSRWLockShared(&g_storedUpdateLock);
    BOOL hasUpdate = g_storedHasUpdate;
    UpdateUi_CopyString(current, sizeof(current), g_storedCurrentVersion);
    UpdateUi_CopyString(latest, sizeof(latest), g_storedLatestVersion);
    UpdateUi_CopyString(url, sizeof(url), g_storedDownloadUrl);
    if (notes) {
        UpdateUi_CopyString(notes, NOTES_BUFFER_SIZE, g_storedReleaseNotes);
    }
    ReleaseSRWLockShared(&g_storedUpdateLock);
    if (!hasUpdate || !latest[0]) {
        LOG_WARNING("Stored update dialog requested without update data");
        free(notes);
        return;
    }
    ShowUpdateNotification(hwnd, current, latest, url, notes ? notes : "");
    free(notes);
}

void ShowStoredNoUpdateDialog(HWND hwnd) {
    char current[64];
    AcquireSRWLockShared(&g_storedUpdateLock);
    UpdateUi_CopyString(current, sizeof(current), g_storedCurrentVersion);
    ReleaseSRWLockShared(&g_storedUpdateLock);
    ShowNoUpdateDialog(hwnd, current);
}
