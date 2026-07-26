/**
 * @file markdown_image_download_lifecycle.c
 * @brief Download cancellation, synchronization, and handle tracking.
 */

#include "markdown_image_internal.h"

BOOL IsDownloadShutdownRequested(void) {
    return InterlockedCompareExchange(&g_downloadShutdown, 0, 0) != 0;
}

LONG GetDownloadGeneration(void) {
    return InterlockedCompareExchange(&g_downloadGeneration, 0, 0);
}

BOOL IsDownloadGenerationCurrent(LONG generation) {
    return GetDownloadGeneration() == generation;
}

BOOL IsDownloadCanceled(LONG generation) {
    return IsDownloadShutdownRequested() ||
           !IsDownloadGenerationCurrent(generation);
}

static BOOL IsDownloadInitFailureCoolingDown(DWORD now) {
    DWORD cooldownUntil = (DWORD)InterlockedCompareExchange(
        &g_downloadInitFailureCooldownUntil, 0, 0);
    return cooldownUntil != 0 && (LONG)(cooldownUntil - now) > 0;
}

static void MarkDownloadInitFailure(DWORD now) {
    DWORD cooldownUntil = now + IMAGE_DOWNLOAD_INIT_FAILURE_COOLDOWN_MS;
    InterlockedExchange(&g_downloadInitFailureCooldownUntil,
                        (LONG)(cooldownUntil ? cooldownUntil : 1));
}

void ClearDownloadInitFailure(void) {
    InterlockedExchange(&g_downloadInitFailureCooldownUntil, 0);
}

static LONG GetDownloadCSInitState(void) {
    return InterlockedCompareExchange(&g_downloadCSInit, 0, 0);
}

BOOL IsDownloadCSReady(void) {
    return GetDownloadCSInitState() == 2;
}

BOOL EnsureDownloadCSInit(void) {
    if (IsDownloadShutdownRequested()) {
        return FALSE;
    }
    if (IsDownloadCSReady()) {
        return TRUE;
    }

    DWORD now = GetTickCount();
    if (IsDownloadInitFailureCoolingDown(now)) {
        return FALSE;
    }

    if (InterlockedCompareExchange(&g_downloadCSInit, 1, 0) == 0) {
        InitializeCriticalSection(&g_downloadCS);
        g_downloadIdleEvent = CreateEventW(NULL, TRUE, TRUE, NULL);
        if (!g_downloadIdleEvent) {
            DeleteCriticalSection(&g_downloadCS);
            InterlockedExchange(&g_downloadCSInit, 0);
            MarkDownloadInitFailure(now);
            return FALSE;
        }
        ClearDownloadInitFailure();
        InterlockedExchange(&g_downloadCSInit, 2);
    }
    WaitWhileLongEquals(&g_downloadCSInit, 1);
    return !IsDownloadShutdownRequested() && IsDownloadCSReady() &&
           g_downloadIdleEvent != NULL;
}

BOOL TrackDownloadHandle(HINTERNET handle, LONG generation) {
    if (!handle || IsDownloadCanceled(generation) ||
        !EnsureDownloadCSInit()) {
        return FALSE;
    }

    EnterCriticalSection(&g_downloadCS);
    if (IsDownloadCanceled(generation)) {
        LeaveCriticalSection(&g_downloadCS);
        return FALSE;
    }
    for (int i = 0; i < MAX_ACTIVE_DOWNLOAD_HANDLES; i++) {
        if (!g_activeDownloadHandles[i]) {
            g_activeDownloadHandles[i] = handle;
            LeaveCriticalSection(&g_downloadCS);
            return TRUE;
        }
    }
    LeaveCriticalSection(&g_downloadCS);
    return FALSE;
}

void CloseTrackedDownloadHandle(HINTERNET* handlePtr, LONG generation) {
    if (!handlePtr || !*handlePtr) {
        return;
    }

    HINTERNET handle = *handlePtr;
    BOOL found = FALSE;
    if (IsDownloadCSReady()) {
        EnterCriticalSection(&g_downloadCS);
        for (int i = 0; i < MAX_ACTIVE_DOWNLOAD_HANDLES; i++) {
            if (g_activeDownloadHandles[i] == handle) {
                g_activeDownloadHandles[i] = NULL;
                found = TRUE;
                break;
            }
        }
        LeaveCriticalSection(&g_downloadCS);
    }

    if (found || !IsDownloadCanceled(generation)) {
        InternetCloseHandle(handle);
    }
    *handlePtr = NULL;
}

void RequestMarkdownImageDownloadCancel(void) {
    HINTERNET handles[MAX_ACTIVE_DOWNLOAD_HANDLES];
    int handleCount = 0;
    InterlockedExchange(&g_downloadShutdown, 1);
    WaitWhileLongEquals(&g_downloadCSInit, 1);
    if (InterlockedCompareExchange(&g_downloadCSInit, 0, 0) != 2) {
        return;
    }

    ZeroMemory(handles, sizeof(handles));
    EnterCriticalSection(&g_downloadCS);
    for (int i = 0; i < MAX_ACTIVE_DOWNLOAD_HANDLES; i++) {
        if (g_activeDownloadHandles[i]) {
            handles[handleCount++] = g_activeDownloadHandles[i];
            g_activeDownloadHandles[i] = NULL;
        }
    }
    LeaveCriticalSection(&g_downloadCS);

    for (int i = 0; i < handleCount; i++) {
        InternetCloseHandle(handles[i]);
    }
}
