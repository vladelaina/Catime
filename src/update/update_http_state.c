#include "update_checker.h"
#include "update/update_internal.h"

#define UPDATE_HTTP_CS_UNINITIALIZED 0
#define UPDATE_HTTP_CS_INITIALIZING 1
#define UPDATE_HTTP_CS_INITIALIZED 2
#define UPDATE_HTTP_WAIT_SPIN_LIMIT 64

static CRITICAL_SECTION g_httpCriticalSection;
static volatile LONG g_httpCriticalSectionState =
    UPDATE_HTTP_CS_UNINITIALIZED;
static volatile LONG g_cancelRequested;
static HINTERNET g_activeInternet;
static HINTERNET g_activeConnect;

static void WaitForCriticalSection(void) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(
               &g_httpCriticalSectionState, 0, 0) ==
           UPDATE_HTTP_CS_INITIALIZING) {
        Sleep(spins++ < UPDATE_HTTP_WAIT_SPIN_LIMIT ? 0 : 1);
    }
}

static void EnsureCriticalSection(void) {
    if (InterlockedCompareExchange(
            &g_httpCriticalSectionState,
            UPDATE_HTTP_CS_INITIALIZING,
            UPDATE_HTTP_CS_UNINITIALIZED) == UPDATE_HTTP_CS_UNINITIALIZED) {
        InitializeCriticalSection(&g_httpCriticalSection);
        InterlockedExchange(&g_httpCriticalSectionState,
                            UPDATE_HTTP_CS_INITIALIZED);
    }
    WaitForCriticalSection();
}

BOOL UpdateHttp_IsCancelRequested(void) {
    return InterlockedCompareExchange(&g_cancelRequested, 0, 0) != 0;
}

void UpdateHttp_TrackInternet(HINTERNET handle) {
    EnsureCriticalSection();
    EnterCriticalSection(&g_httpCriticalSection);
    g_activeInternet = handle;
    LeaveCriticalSection(&g_httpCriticalSection);
}

void UpdateHttp_TrackConnect(HINTERNET handle) {
    EnsureCriticalSection();
    EnterCriticalSection(&g_httpCriticalSection);
    g_activeConnect = handle;
    LeaveCriticalSection(&g_httpCriticalSection);
}

void UpdateHttp_CloseTracked(HINTERNET* handle) {
    if (!handle || !*handle) return;
    HINTERNET trackedHandle = *handle;
    BOOL shouldClose = FALSE;

    EnsureCriticalSection();
    EnterCriticalSection(&g_httpCriticalSection);
    if (g_activeConnect == trackedHandle) {
        g_activeConnect = NULL;
        shouldClose = TRUE;
    } else if (g_activeInternet == trackedHandle) {
        g_activeInternet = NULL;
        shouldClose = TRUE;
    }
    LeaveCriticalSection(&g_httpCriticalSection);
    if (shouldClose) InternetCloseHandle(trackedHandle);
    *handle = NULL;
}

static void CancelAndCloseActiveHandles(void) {
    HINTERNET connect = NULL;
    HINTERNET internet = NULL;
    InterlockedExchange(&g_cancelRequested, 1);
    EnsureCriticalSection();
    EnterCriticalSection(&g_httpCriticalSection);
    connect = g_activeConnect;
    internet = g_activeInternet;
    g_activeConnect = NULL;
    g_activeInternet = NULL;
    LeaveCriticalSection(&g_httpCriticalSection);
    if (connect) InternetCloseHandle(connect);
    if (internet) InternetCloseHandle(internet);
}

void RequestUpdateCheckCancel(void) {
    CancelAndCloseActiveHandles();
}

void ResetUpdateCheckCancel(void) {
    InterlockedExchange(&g_cancelRequested, 0);
}

void CleanupUpdateCheckResources(void) {
    InterlockedExchange(&g_cancelRequested, 1);
    WaitForCriticalSection();
    if (InterlockedCompareExchange(
            &g_httpCriticalSectionState, 0, 0) !=
        UPDATE_HTTP_CS_INITIALIZED) {
        return;
    }
    CancelAndCloseActiveHandles();
}
