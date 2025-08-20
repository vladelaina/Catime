/**
 * @file async_update_checker.c
 * @brief Asynchronous update checking to avoid blocking UI
 */
#include <windows.h>
#include <process.h>
#include "../include/async_update_checker.h"
#include "../include/update_checker.h"
#include "../include/log.h"

/** @brief Parameters passed to update check thread */
typedef struct {
    HWND hwnd;
    BOOL silentCheck;
} UpdateThreadParams;

/** @brief Thread state management */
static HANDLE g_hUpdateThread = NULL;
static BOOL g_bUpdateThreadRunning = FALSE;

/**
 * @brief Clean up update check thread resources with timeout
 * Waits for thread completion before forcefully closing handle
 */
void CleanupUpdateThread() {
    LOG_INFO("Cleaning up update check thread resources");
    if (g_hUpdateThread != NULL) {
        /** Wait with timeout to avoid indefinite blocking */
        DWORD waitResult = WaitForSingleObject(g_hUpdateThread, 1000);
        if (waitResult == WAIT_TIMEOUT) {
            LOG_WARNING("Wait for thread end timed out, forcibly closing thread handle");
        } else if (waitResult == WAIT_OBJECT_0) {
            LOG_INFO("Thread has ended normally");
        } else {
            LOG_WARNING("Wait for thread returned unexpected result: %lu", waitResult);
        }

        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
        g_bUpdateThreadRunning = FALSE;
        LOG_INFO("Thread resources have been cleaned up");
    } else {
        LOG_INFO("Update check thread not running, no cleanup needed");
    }
}

/**
 * @brief Update check thread procedure
 * @param param UpdateThreadParams structure containing window handle and mode
 * @return Thread exit code (0 on success)
 */
unsigned __stdcall UpdateCheckThreadProc(void* param) {
    LOG_INFO("Update check thread started");

    UpdateThreadParams* threadParams = (UpdateThreadParams*)param;
    if (!threadParams) {
        LOG_ERROR("Thread parameters are null, cannot perform update check");
        g_bUpdateThreadRunning = FALSE;
        _endthreadex(1);
        return 1;
    }

    HWND hwnd = threadParams->hwnd;
    BOOL silentCheck = threadParams->silentCheck;

    LOG_INFO("Thread parameters parsed successfully, window handle: 0x%p, silent check mode: %s",
             hwnd, silentCheck ? "yes" : "no");

    /** Free parameter memory early to prevent leaks */
    free(threadParams);
    LOG_INFO("Thread parameter memory freed");

    LOG_INFO("Starting update check");
    CheckForUpdateSilent(hwnd, silentCheck);
    LOG_INFO("Update check completed");

    g_bUpdateThreadRunning = FALSE;

    _endthreadex(0);
    return 0;
}

/**
 * @brief Start asynchronous update check in background thread
 * @param hwnd Main window handle for UI callbacks
 * @param silentCheck TRUE for background check, FALSE for user-initiated
 */
void CheckForUpdateAsync(HWND hwnd, BOOL silentCheck) {
    LOG_INFO("Asynchronous update check requested, window handle: 0x%p, silent mode: %s",
             hwnd, silentCheck ? "yes" : "no");

    /** Prevent concurrent update checks */
    if (g_bUpdateThreadRunning) {
        LOG_INFO("Update check thread already running, skipping this check request");
        return;
    }

    /** Clean up stale thread handle */
    if (g_hUpdateThread != NULL) {
        LOG_INFO("Found old thread handle, cleaning up...");
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
        LOG_INFO("Old thread handle closed");
    }

    LOG_INFO("Allocating memory for thread parameters");
    UpdateThreadParams* threadParams = (UpdateThreadParams*)malloc(sizeof(UpdateThreadParams));
    if (!threadParams) {
        LOG_ERROR("Thread parameter memory allocation failed, cannot start update check thread");
        return;
    }

    threadParams->hwnd = hwnd;
    threadParams->silentCheck = silentCheck;
    LOG_INFO("Thread parameters set up");

    g_bUpdateThreadRunning = TRUE;

    /** Create detached worker thread for update check */
    LOG_INFO("Preparing to create update check thread");
    HANDLE hThread = (HANDLE)_beginthreadex(
        NULL,
        0,
        UpdateCheckThreadProc,
        threadParams,
        0,
        NULL
    );

    if (hThread) {
        LOG_INFO("Update check thread created successfully, thread handle: 0x%p", hThread);
        g_hUpdateThread = hThread;
    } else {
        /** Handle thread creation failure gracefully */
        DWORD errorCode = GetLastError();
        char errorMsg[256] = {0};
        GetLastErrorDescription(errorCode, errorMsg, sizeof(errorMsg));
        LOG_ERROR("Update check thread creation failed, error code: %lu, error message: %s", errorCode, errorMsg);

        free(threadParams);
        g_bUpdateThreadRunning = FALSE;
    }
}