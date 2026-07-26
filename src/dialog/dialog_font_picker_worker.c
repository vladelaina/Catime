/**
 * @file dialog_font_picker_worker.c
 * @brief Asynchronous system-font enumeration and lifecycle management.
 */

#include "dialog_font_picker_internal.h"
#include "dialog/dialog_common.h"
#include "font.h"
#include "language.h"
#include "log.h"
#include "window/window_core.h"
#include "../../resource/resource.h"
#include <stdlib.h>

HANDLE g_fontEnumThread = NULL;
HANDLE g_fontEnumStopEvent = NULL;
BOOL g_fontEnumRestartAfterCleanup = FALSE;
volatile LONG g_fontEnumGeneration = 0;

BOOL DialogFontPickerInternal_ShouldStopEnumeration(HANDLE stopEvent) {
    return stopEvent &&
           WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0;
}

BOOL DialogFontPickerInternal_CleanupCompletedEnumeration(void) {
    if (!g_fontEnumThread) {
        if (g_fontEnumStopEvent) {
            CloseHandle(g_fontEnumStopEvent);
            g_fontEnumStopEvent = NULL;
        }
        return TRUE;
    }

    DWORD waitResult = WaitForSingleObject(g_fontEnumThread, 0);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_FAILED) {
            LOG_WARNING("FontPicker: Failed to query font enumeration thread (error=%lu)",
                        GetLastError());
        }
        return FALSE;
    }

    CloseHandle(g_fontEnumThread);
    g_fontEnumThread = NULL;
    if (g_fontEnumStopEvent) {
        CloseHandle(g_fontEnumStopEvent);
        g_fontEnumStopEvent = NULL;
    }
    return TRUE;
}

BOOL DialogFontPickerInternal_StopEnumeration(DWORD timeoutMs) {
    if (g_fontEnumThread || g_fontEnumStopEvent) {
        InterlockedIncrement(&g_fontEnumGeneration);
    }
    if (g_fontEnumStopEvent) {
        SetEvent(g_fontEnumStopEvent);
    }
    if (!g_fontEnumThread) {
        if (g_fontEnumStopEvent) {
            CloseHandle(g_fontEnumStopEvent);
            g_fontEnumStopEvent = NULL;
        }
        return TRUE;
    }

    DWORD waitResult = WaitForSingleObject(g_fontEnumThread, timeoutMs);
    if (waitResult == WAIT_OBJECT_0) {
        CloseHandle(g_fontEnumThread);
        g_fontEnumThread = NULL;
        if (g_fontEnumStopEvent) {
            CloseHandle(g_fontEnumStopEvent);
            g_fontEnumStopEvent = NULL;
        }
        return TRUE;
    }
    if (waitResult == WAIT_TIMEOUT) {
        LOG_WARNING("FontPicker: Font enumeration stop timed out after %lu ms",
                    timeoutMs);
    } else {
        LOG_WARNING("FontPicker: Font enumeration stop wait failed (wait=%lu, error=%lu)",
                    waitResult, GetLastError());
    }
    if (g_fontEnumStopEvent) {
        CloseHandle(g_fontEnumStopEvent);
        g_fontEnumStopEvent = NULL;
    }
    return FALSE;
}

BOOL DialogFontPickerInternal_StartPollTimer(HWND hdlg) {
    if (!SetTimer(hdlg, FONT_ENUM_POLL_TIMER_ID,
                  FONT_ENUM_POLL_INTERVAL_MS, NULL)) {
        LOG_WARNING("FontPicker: Failed to start enumeration poll timer (error=%lu)",
                    GetLastError());
        return FALSE;
    }
    return TRUE;
}

static BOOL StartFontEnumRetryTimer(HWND hdlg) {
    if (!SetTimer(hdlg, FONT_ENUM_START_RETRY_TIMER_ID,
                  FONT_ENUM_START_RETRY_INTERVAL_MS, NULL)) {
        LOG_WARNING("FontPicker: Failed to start enumeration retry timer (error=%lu)",
                    GetLastError());
        return FALSE;
    }
    return TRUE;
}

static VOID CALLBACK FontEnumDeferredCleanupTimerProc(HWND hwnd, UINT msg,
                                                      UINT_PTR idEvent,
                                                      DWORD time) {
    (void)msg;
    (void)time;
    if (idEvent != FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID) {
        return;
    }
    if (Dialog_IsOpen(DIALOG_INSTANCE_FONT_PICKER)) {
        KillTimer(hwnd, FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID);
        return;
    }
    if (!DialogFontPickerInternal_CleanupCompletedEnumeration()) {
        return;
    }

    KillTimer(hwnd, FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID);
    DialogFontPickerInternal_ResetFontMap();
    g_currentFontIndex = -1;
    g_previewFontIndex = -1;
    g_fontListReady = FALSE;
    g_fontEnumRestartAfterCleanup = FALSE;
}

void DialogFontPickerInternal_ScheduleDeferredCleanup(void) {
    HWND hwndMain = FindCurrentProcessMainWindow();
    if (!hwndMain) {
        return;
    }
    if (!SetTimer(hwndMain, FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID,
                  FONT_ENUM_DEFERRED_CLEANUP_INTERVAL_MS,
                  FontEnumDeferredCleanupTimerProc)) {
        LOG_WARNING("FontPicker: Failed to schedule deferred enumeration cleanup (error=%lu)",
                    GetLastError());
    }
}

void CleanupSystemFontDialogResources(void) {
    HWND hwndMain = FindCurrentProcessMainWindow();
    if (hwndMain) {
        KillTimer(hwndMain, FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID);
    }

    BOOL enumStopped = DialogFontPickerInternal_StopEnumeration(
        FONT_ENUM_SHUTDOWN_WAIT_MS);
    if (!enumStopped) {
        LOG_WARNING("FontPicker: Font enumeration still running during shutdown; leaving resources to process teardown");
        return;
    }

    DialogFontPickerInternal_ResetFontMap();
    g_currentFontIndex = -1;
    g_previewFontIndex = -1;
    g_fontListReady = FALSE;
    g_fontEnumRestartAfterCleanup = FALSE;
}

static DWORD WINAPI FontEnumerationThread(LPVOID param) {
    FontEnumerationThreadParams* params =
        (FontEnumerationThreadParams*)param;
    HWND hdlg = params ? params->hdlg : NULL;
    HANDLE stopEvent = params ? params->stopEvent : NULL;
    LONG generation = params ? params->generation : 0;
    free(params);

    DialogFontPickerInternal_BuildFontMap(stopEvent);
    if (!DialogFontPickerInternal_ShouldStopEnumeration(stopEvent) &&
        InterlockedCompareExchange(&g_fontEnumGeneration, 0, 0) == generation &&
        hdlg && IsWindow(hdlg) &&
        Dialog_GetInstance(DIALOG_INSTANCE_FONT_PICKER) == hdlg) {
        PostMessageW(hdlg, WM_APP_FONT_ENUM_COMPLETE,
                     (WPARAM)generation, 0);
    }

    if (stopEvent) {
        CloseHandle(stopEvent);
    }
    return 0;
}

static HANDLE StartFontEnumerationThread(HWND hdlg) {
    FontEnumerationThreadParams* params =
        (FontEnumerationThreadParams*)malloc(sizeof(FontEnumerationThreadParams));
    if (!params) {
        return NULL;
    }

    params->hdlg = hdlg;
    params->stopEvent = NULL;
    params->generation = InterlockedIncrement(&g_fontEnumGeneration);
    if (g_fontEnumStopEvent &&
        !DuplicateHandle(GetCurrentProcess(), g_fontEnumStopEvent,
                         GetCurrentProcess(), &params->stopEvent, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
        LOG_WARNING("FontPicker: Failed to duplicate enumeration stop event (error=%lu)",
                    GetLastError());
        free(params);
        return NULL;
    }

    HANDLE hThread = CreateThread(NULL, 0, FontEnumerationThread,
                                  params, 0, NULL);
    if (!hThread) {
        if (params->stopEvent) {
            CloseHandle(params->stopEvent);
        }
        free(params);
    }
    return hThread;
}

static void CloseFontEnumStopEventIfIdle(void) {
    if (!g_fontEnumThread && g_fontEnumStopEvent) {
        CloseHandle(g_fontEnumStopEvent);
        g_fontEnumStopEvent = NULL;
    }
}

static void ShowFontEnumerationUnavailable(HWND hdlg) {
    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
    if (hwndList) {
        SendMessageW(hwndList, LB_RESETCONTENT, 0, 0);
        EnableWindow(hwndList, FALSE);
    }
    EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);
    SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                    GetLocalizedString(NULL, L"Loading..."));
    g_fontListReady = FALSE;
}

BOOL DialogFontPickerInternal_StartEnumeration(HWND hdlg) {
    CloseFontEnumStopEventIfIdle();
    g_fontEnumStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_fontEnumStopEvent) {
        LOG_WARNING("FontPicker: Failed to create enumeration stop event (error=%lu)",
                    GetLastError());
        ShowFontEnumerationUnavailable(hdlg);
        StartFontEnumRetryTimer(hdlg);
        return FALSE;
    }

    g_fontEnumThread = StartFontEnumerationThread(hdlg);
    if (!g_fontEnumThread) {
        LOG_WARNING("FontPicker: Background enumeration unavailable; retrying asynchronously");
        CloseFontEnumStopEventIfIdle();
        ShowFontEnumerationUnavailable(hdlg);
        StartFontEnumRetryTimer(hdlg);
        return FALSE;
    }

    KillTimer(hdlg, FONT_ENUM_START_RETRY_TIMER_ID);
    DialogFontPickerInternal_StartPollTimer(hdlg);
    return TRUE;
}
