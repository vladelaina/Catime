/**
 * @file async_update_checker.c
 * @brief Non-blocking update checks via background thread
 * 
 * Thread handle retention prevents concurrent checks and lets final teardown
 * wait for the worker before global resources are released.
 */
#include <windows.h>
#include <process.h>
#include <stdlib.h>
#include "async_update_checker.h"
#include "update_checker.h"
#include "update/update_internal.h"
#include "log.h"

/* Short wait keeps WM_DESTROY responsive; final teardown gets a longer budget. */
#define THREAD_WAIT_TIMEOUT_MS 1000
#define FINAL_THREAD_WAIT_TIMEOUT_MS 3000
#define ERROR_MSG_BUFFER_SIZE 256
#define UPDATE_THREAD_START_FAILURE_COOLDOWN_MS 2000

typedef struct {
    HWND hwnd;
    BOOL silentCheck;
} UpdateThreadParams;

static HANDLE g_hUpdateThread = NULL;
static volatile LONG g_updateThreadRunning = 0;
static SRWLOCK g_updateThreadLock = SRWLOCK_INIT;
static DWORD g_updateStartFailureCooldownUntil = 0;

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

static void ResetThreadStateLocked(void) {
    InterlockedExchange(&g_updateThreadRunning, 0);
    if (g_hUpdateThread) {
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
    }
}

static void ReleaseUpdateThreadHandleLocked(void) {
    if (g_hUpdateThread) {
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
    }
}

static BOOL IsUpdateThreadStartFailureCoolingDown(DWORD now) {
    return g_updateStartFailureCooldownUntil != 0 &&
           (LONG)(g_updateStartFailureCooldownUntil - now) > 0;
}

static void MarkUpdateThreadStartFailure(DWORD now) {
    DWORD cooldownUntil = now + UPDATE_THREAD_START_FAILURE_COOLDOWN_MS;
    g_updateStartFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
}

static void CleanupCompletedUpdateThreadLocked(void) {
    if (!g_hUpdateThread) {
        return;
    }

    DWORD waitResult = WaitForSingleObject(g_hUpdateThread, 0);
    if (waitResult == WAIT_OBJECT_0) {
        ResetThreadStateLocked();
    } else if (waitResult == WAIT_FAILED) {
        LOG_WARNING("Update thread status check failed: %lu", GetLastError());
    }
}

static UpdateThreadParams* PrepareThreadParams(HWND hwnd, BOOL silentCheck) {
    UpdateThreadParams* params = (UpdateThreadParams*)malloc(sizeof(UpdateThreadParams));
    if (!params) {
        LOG_ERROR("Failed to allocate thread parameters");
        return NULL;
    }
    
    params->hwnd = hwnd;
    params->silentCheck = silentCheck;
    return params;
}

static void HandleThreadCreationFailure(UpdateThreadParams* params) {
    DWORD errorCode = GetLastError();
    char errorMsg[ERROR_MSG_BUFFER_SIZE] = {0};
    GetLastErrorDescription(errorCode, errorMsg, sizeof(errorMsg));
    LOG_ERROR("Thread creation failed (error %lu): %s", errorCode, errorMsg);
    
    free(params);
    MarkUpdateThreadStartFailure(GetTickCount());
    ResetThreadStateLocked();
}

/* ============================================================================
 * Thread procedure
 * ============================================================================ */

unsigned __stdcall UpdateCheckThreadProc(void* param) {
    UpdateThreadParams* threadParams = (UpdateThreadParams*)param;
    if (!threadParams) {
        LOG_ERROR("Thread parameters are null");
        InterlockedExchange(&g_updateThreadRunning, 0);
        _endthreadex(1);
        return 1;
    }
    
    HWND hwnd = threadParams->hwnd;
    BOOL silentCheck = threadParams->silentCheck;
    free(threadParams);
    
    CheckForUpdateInternal(hwnd, silentCheck);

    InterlockedExchange(&g_updateThreadRunning, 0);
    _endthreadex(0);
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void CleanupUpdateThread(void) {
    AcquireSRWLockExclusive(&g_updateThreadLock);
    CleanupCompletedUpdateThreadLocked();
    if (!g_hUpdateThread) {
        ReleaseSRWLockExclusive(&g_updateThreadLock);
        return;
    }

    RequestUpdateCheckCancel();

    DWORD waitResult = WaitForSingleObject(g_hUpdateThread, THREAD_WAIT_TIMEOUT_MS);

    switch (waitResult) {
        case WAIT_TIMEOUT:
            LOG_WARNING("Update thread cleanup timeout, keeping handle for final teardown");
            ReleaseSRWLockExclusive(&g_updateThreadLock);
            return;
        case WAIT_OBJECT_0:
            break;
        default:
            LOG_WARNING("Unexpected wait result: %lu", waitResult);
            break;
    }

    ResetThreadStateLocked();
    ReleaseSRWLockExclusive(&g_updateThreadLock);
}

void CleanupUpdateThreadBlocking(void) {
    AcquireSRWLockExclusive(&g_updateThreadLock);
    CleanupCompletedUpdateThreadLocked();
    if (!g_hUpdateThread) {
        ReleaseSRWLockExclusive(&g_updateThreadLock);
        return;
    }

    RequestUpdateCheckCancel();

    DWORD waitResult = WaitForSingleObject(g_hUpdateThread, FINAL_THREAD_WAIT_TIMEOUT_MS);
    if (waitResult == WAIT_TIMEOUT) {
        LOG_WARNING("Final update thread cleanup timeout, abandoning worker during process teardown");
        ReleaseUpdateThreadHandleLocked();
        ReleaseSRWLockExclusive(&g_updateThreadLock);
        return;
    }
    if (waitResult != WAIT_OBJECT_0) {
        LOG_WARNING("Unexpected final update thread wait result: %lu", waitResult);
    }

    ResetThreadStateLocked();
    ReleaseSRWLockExclusive(&g_updateThreadLock);
}

void CheckForUpdateAsync(HWND hwnd, BOOL silentCheck) {
    AcquireSRWLockExclusive(&g_updateThreadLock);
    CleanupCompletedUpdateThreadLocked();
    DWORD now = GetTickCount();
    if (IsUpdateThreadStartFailureCoolingDown(now)) {
        ReleaseSRWLockExclusive(&g_updateThreadLock);
        return;
    }

    if (InterlockedCompareExchange(&g_updateThreadRunning, 1, 0) != 0) {
        ReleaseSRWLockExclusive(&g_updateThreadLock);
        return;
    }

    if (g_hUpdateThread) {
        InterlockedExchange(&g_updateThreadRunning, 0);
        ReleaseSRWLockExclusive(&g_updateThreadLock);
        return;
    }

    UpdateThreadParams* threadParams = PrepareThreadParams(hwnd, silentCheck);
    if (!threadParams) {
        MarkUpdateThreadStartFailure(now);
        InterlockedExchange(&g_updateThreadRunning, 0);
        ReleaseSRWLockExclusive(&g_updateThreadLock);
        return;
    }
    ResetUpdateCheckCancel();
    HANDLE hThread = (HANDLE)_beginthreadex(
        NULL,
        0,
        UpdateCheckThreadProc,
        threadParams,
        0,
        NULL
    );
    
    if (hThread) {
        g_hUpdateThread = hThread;
        g_updateStartFailureCooldownUntil = 0;
    } else {
        HandleThreadCreationFailure(threadParams);
    }
    ReleaseSRWLockExclusive(&g_updateThreadLock);
}
