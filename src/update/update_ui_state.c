#include "update_ui_state.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_language.h"
#include "dialog/dialog_procedure.h"

#include <string.h>
#include <wchar.h>

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

HWND g_hwndUpdateDialog;
HWND g_hwndNoUpdateDialog;
HWND g_hwndExitMsgDialog;
VersionInfo g_updateVersionInfo;
char g_dialogCurrentVersion[64];
char g_dialogLatestVersion[64];
char g_dialogDownloadUrl[URL_BUFFER_SIZE];
char g_dialogReleaseNotes[NOTES_BUFFER_SIZE];
char g_downloadUrlCopy[512];
SRWLOCK g_downloadUrlLock = SRWLOCK_INIT;
#ifdef CATIME_USE_WIN32_FLS
ThreadLocalBuffer g_downloadUrlSnapshotStorage =
    THREAD_LOCAL_BUFFER_STATIC_INIT(sizeof(g_downloadUrlCopy));
#endif
BOOL g_shouldExitAfterDialog;
char g_noUpdateVersion[64];

void UpdateUi_CopyString(char* destination, size_t destinationSize,
                         const char* source) {
    if (!destination || destinationSize == 0) return;
    if (!source) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, source, destinationSize - 1);
    destination[destinationSize - 1] = '\0';
}

BOOL UpdateUi_CopyPendingUrl(char* destination, size_t destinationSize) {
    if (!destination || destinationSize == 0) return FALSE;
    AcquireSRWLockShared(&g_downloadUrlLock);
    UpdateUi_CopyString(destination, destinationSize, g_downloadUrlCopy);
    ReleaseSRWLockShared(&g_downloadUrlLock);
    return destination[0] != '\0';
}

void UpdateUi_ClearPendingUrl(void) {
    AcquireSRWLockExclusive(&g_downloadUrlLock);
    g_downloadUrlCopy[0] = '\0';
    ReleaseSRWLockExclusive(&g_downloadUrlLock);
}

BOOL UpdateUi_IsValidParent(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return FALSE;
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) return FALSE;
    wchar_t className[64] = {0};
    return GetClassNameW(hwnd, className, _countof(className)) > 0 &&
           wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

HWND UpdateUi_GetDialogParent(HWND dialog) {
    HWND parent = Dialog_GetOwnerWindow(dialog);
    return UpdateUi_IsValidParent(parent) ? parent : NULL;
}

void UpdateUi_CloseDialog(DialogInstanceType type) {
    HWND dialog = Dialog_GetInstance(type);
    if (dialog && IsWindow(dialog)) DestroyWindow(dialog);
}

void UpdateUi_InitializeDialog(HWND dialog, int dialogId) {
    ApplyDialogLanguage(dialog, dialogId);
    MoveDialogToPrimaryScreen(dialog);
}
