/**
 * @file async_update_checker.c
 * @brief Non-blocking update checks via background thread
 * 
 * Thread detachment prevents concurrent checks (idempotent).
 * 1s timeout balances clean exit vs shutdown responsiveness.
 */
#include <windows.h>
#include <process.h>
#include "async_update_checker.h"
#include "update_checker.h"
#include "log.h"

/* 1s timeout balances clean shutdown with responsiveness (mentioned in file header) */
#define THREAD_WAIT_TIMEOUT_MS 1000
#define ERROR_MSG_BUFFER_SIZE 256

typedef struct {
    HWND hwnd;
    BOOL silentCheck;
} UpdateThreadParams;

static HANDLE g_hUpdateThread = NULL;
static volatile LONG g_updateThreadRunning = 0;

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

static void ResetThreadState(void) {
    InterlockedExchange(&g_updateThreadRunning, 0);
    if (g_hUpdateThread) {
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
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
    ResetThreadState();
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
    
    LOG_INFO("Update check started (silent: %s)", silentCheck ? "yes" : "no");
    CheckForUpdateSilent(hwnd, silentCheck);
    LOG_INFO("Update check completed");

    InterlockedExchange(&g_updateThreadRunning, 0);
    _endthreadex(0);
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void CleanupUpdateThread(void) {
    if (!g_hUpdateThread) {
        return;
    }
    
    DWORD waitResult = WaitForSingleObject(g_hUpdateThread, THREAD_WAIT_TIMEOUT_MS);
    
    switch (waitResult) {
        case WAIT_TIMEOUT:
            LOG_WARNING("Thread cleanup timeout, closing handle while thread finishes in background");
            CloseHandle(g_hUpdateThread);
            g_hUpdateThread = NULL;
            LOG_INFO("Thread handle cleaned up");
            return;
        case WAIT_OBJECT_0:
            LOG_INFO("Thread ended normally");
            break;
        default:
            LOG_WARNING("Unexpected wait result: %lu", waitResult);
            break;
    }

    ResetThreadState();
    LOG_INFO("Thread resources cleaned up");
}

void CheckForUpdateAsync(HWND hwnd, BOOL silentCheck) {
    if (InterlockedCompareExchange(&g_updateThreadRunning, 1, 0) != 0) {
        LOG_INFO("Update check already running, skipping request");
        return;
    }

    if (g_hUpdateThread) {
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
    }

    UpdateThreadParams* threadParams = PrepareThreadParams(hwnd, silentCheck);
    if (!threadParams) {
        InterlockedExchange(&g_updateThreadRunning, 0);
        return;
    }
    HANDLE hThread = (HANDLE)_beginthreadex(
        NULL,
        0,
        UpdateCheckThreadProc,
        threadParams,
        0,
        NULL
    );
    
    if (hThread) {
        LOG_INFO("Update check thread created (handle: 0x%p)", hThread);
        g_hUpdateThread = hThread;
    } else {
        HandleThreadCreationFailure(threadParams);
    }
}
