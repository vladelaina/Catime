#ifndef UPDATE_UI_STATE_H
#define UPDATE_UI_STATE_H

#include "update/update_internal.h"
#include "dialog/dialog_common.h"
#ifdef CATIME_USE_WIN32_FLS
#include "utils/thread_local_buffer.h"
#endif

extern HWND g_hwndUpdateDialog;
extern HWND g_hwndNoUpdateDialog;
extern HWND g_hwndExitMsgDialog;
extern VersionInfo g_updateVersionInfo;
extern char g_dialogCurrentVersion[64];
extern char g_dialogLatestVersion[64];
extern char g_dialogDownloadUrl[URL_BUFFER_SIZE];
extern char g_dialogReleaseNotes[NOTES_BUFFER_SIZE];
extern char g_downloadUrlCopy[512];
extern SRWLOCK g_downloadUrlLock;
#ifdef CATIME_USE_WIN32_FLS
extern ThreadLocalBuffer g_downloadUrlSnapshotStorage;
#endif
extern BOOL g_shouldExitAfterDialog;
extern char g_noUpdateVersion[64];

void UpdateUi_CopyString(char* destination, size_t destinationSize,
                         const char* source);
BOOL UpdateUi_CopyPendingUrl(char* destination, size_t destinationSize);
void UpdateUi_ClearPendingUrl(void);
BOOL UpdateUi_IsValidParent(HWND hwnd);
HWND UpdateUi_GetDialogParent(HWND dialog);
void UpdateUi_CloseDialog(DialogInstanceType type);
void UpdateUi_InitializeDialog(HWND dialog, int dialogId);

INT_PTR CALLBACK ExitMsgDlgProc(HWND dialog, UINT message,
                                WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK UpdateDlgProc(HWND dialog, UINT message,
                               WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK UpdateErrorDlgProc(HWND dialog, UINT message,
                                    WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK NoUpdateDlgProc(HWND dialog, UINT message,
                                 WPARAM wParam, LPARAM lParam);

#endif
