/**
 * @file async_update_checker.c
 * @brief Asynchronous update checking to avoid blocking UI
 * @version 2.0 - Refactored for better maintainability
 */
#include <windows.h>
#include <process.h>
#include "../include/async_update_checker.h"
#include "../include/update_checker.h"
#include "../include/log.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Thread wait timeout in milliseconds */
#define THREAD_WAIT_TIMEOUT_MS 1000

/** @brief Error message buffer size */
#define ERROR_MSG_BUFFER_SIZE 256

/* ============================================================================
 * Type definitions
 * ============================================================================ */

/** @brief Parameters passed to update check thread */
typedef struct {
    HWND hwnd;
    BOOL silentCheck;
} UpdateThreadParams;

/* ============================================================================
 * Global state
 * ============================================================================ */

/** @brief Thread state management */
static HANDLE g_hUpdateThread = NULL;
static BOOL g_bUpdateThreadRunning = FALSE;

/* ============================================================================
 * Internal helper functions
 * ============================================================================ */

/**
 * @brief Reset thread state and clean up handle
 */
static void ResetThreadState(void) {
    g_bUpdateThreadRunning = FALSE;
    if (g_hUpdateThread) {
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
    }
}

/**
 * @brief Allocate and initialize thread parameters
 * @return Allocated parameters or NULL on failure
 */
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

/**
 * @brief Handle thread creation failure
 */
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

/**
 * @brief Update check thread procedure
 * @param param UpdateThreadParams structure containing window handle and mode
 * @return Thread exit code (0 on success)
 */
unsigned __stdcall UpdateCheckThreadProc(void* param) {
    UpdateThreadParams* threadParams = (UpdateThreadParams*)param;
    if (!threadParams) {
        LOG_ERROR("Thread parameters are null");
        g_bUpdateThreadRunning = FALSE;
        _endthreadex(1);
        return 1;
    }
    
    HWND hwnd = threadParams->hwnd;
    BOOL silentCheck = threadParams->silentCheck;
    free(threadParams);
    
    LOG_INFO("Update check started (silent: %s)", silentCheck ? "yes" : "no");
    CheckForUpdateSilent(hwnd, silentCheck);
    LOG_INFO("Update check completed");
    
    g_bUpdateThreadRunning = FALSE;
    _endthreadex(0);
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Clean up update check thread resources with timeout
 */
void CleanupUpdateThread(void) {
    if (!g_hUpdateThread) {
        return;
    }
    
    DWORD waitResult = WaitForSingleObject(g_hUpdateThread, THREAD_WAIT_TIMEOUT_MS);
    
    switch (waitResult) {
        case WAIT_TIMEOUT:
            LOG_WARNING("Thread cleanup timeout, forcibly closing handle");
            break;
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

/**
 * @brief Start asynchronous update check in background thread
 * @param hwnd Main window handle for UI callbacks
 * @param silentCheck TRUE for background check, FALSE for user-initiated
 */
void CheckForUpdateAsync(HWND hwnd, BOOL silentCheck) {
    // Prevent concurrent update checks
    if (g_bUpdateThreadRunning) {
        LOG_INFO("Update check already running, skipping request");
        return;
    }
    
    // Clean up stale thread handle
    if (g_hUpdateThread) {
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
    }
    
    // Prepare thread parameters
    UpdateThreadParams* threadParams = PrepareThreadParams(hwnd, silentCheck);
    if (!threadParams) {
        return;
    }
    
    // Create update check thread
    g_bUpdateThreadRunning = TRUE;
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
