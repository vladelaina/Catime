/**
 * @file async_update_checker.c
 * @brief Implementation of simplified asynchronous application update checking functionality
 * 
 * This file implements the functionality for asynchronously checking for application updates,
 * ensuring that update checks do not block the main thread.
 */

#include <windows.h>
#include <process.h>
#include "../include/async_update_checker.h"
#include "../include/update_checker.h"
#include "../include/log.h"

// Thread parameter structure
typedef struct {
    HWND hwnd;
    BOOL silentCheck;
} UpdateThreadParams;

// Handle for the currently running update check thread
static HANDLE g_hUpdateThread = NULL;
static BOOL g_bUpdateThreadRunning = FALSE;

/**
 * @brief Clean up update check thread resources
 * 
 * Close thread handle and release related resources.
 */
void CleanupUpdateThread() {
    LOG_INFO("Cleaning up update check thread resources");
    if (g_hUpdateThread != NULL) {
        LOG_INFO("Waiting for update check thread to end, timeout set to 1 second");
        // Wait for thread to end, but no more than 1 second
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
 * @brief Update check thread function
 * @param param Thread parameters (window handle)
 * 
 * Performs update check in a separate thread, without blocking the main thread.
 */
unsigned __stdcall UpdateCheckThreadProc(void* param) {
    LOG_INFO("Update check thread started");
    
    // Parse thread parameters
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
    
    // Free thread parameter memory
    free(threadParams);
    LOG_INFO("Thread parameter memory freed");
    
    // Call the original update check function, passing the silent check parameter
    LOG_INFO("Starting update check");
    CheckForUpdateSilent(hwnd, silentCheck);
    LOG_INFO("Update check completed");
    
    // Mark thread as ended
    g_bUpdateThreadRunning = FALSE;
    
    // End thread
    _endthreadex(0);
    return 0;
}

/**
 * @brief Check for application updates asynchronously
 * @param hwnd Window handle
 * @param silentCheck Whether to perform a silent check (only show prompt if updates are available)
 * 
 * Connects to GitHub in a separate thread to check for new versions.
 * If available, prompts the user whether to download in the browser.
 * This function returns immediately, without blocking the main thread.
 */
void CheckForUpdateAsync(HWND hwnd, BOOL silentCheck) {
    LOG_INFO("Asynchronous update check requested, window handle: 0x%p, silent mode: %s", 
             hwnd, silentCheck ? "yes" : "no");
    
    // If an update check thread is already running, don't start a new one
    if (g_bUpdateThreadRunning) {
        LOG_INFO("Update check thread already running, skipping this check request");
        return;
    }
    
    // Clean up previous thread handle (if any)
    if (g_hUpdateThread != NULL) {
        LOG_INFO("Found old thread handle, cleaning up...");
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
        LOG_INFO("Old thread handle closed");
    }
    
    // Allocate memory for thread parameters
    LOG_INFO("Allocating memory for thread parameters");
    UpdateThreadParams* threadParams = (UpdateThreadParams*)malloc(sizeof(UpdateThreadParams));
    if (!threadParams) {
        // Memory allocation failed
        LOG_ERROR("Thread parameter memory allocation failed, cannot start update check thread");
        return;
    }
    
    // Set thread parameters
    threadParams->hwnd = hwnd;
    threadParams->silentCheck = silentCheck;
    LOG_INFO("Thread parameters set up");
    
    // Mark thread as about to run
    g_bUpdateThreadRunning = TRUE;
    
    LOG_INFO("Preparing to create update check thread");
    // Create thread to perform update check
    HANDLE hThread = (HANDLE)_beginthreadex(
        NULL,               // Default security attributes
        0,                  // Default stack size
        UpdateCheckThreadProc, // Thread function
        threadParams,       // Thread parameters
        0,                  // Run thread immediately
        NULL                // Don't need thread ID
    );
    
    if (hThread) {
        // Save thread handle for later checking
        LOG_INFO("Update check thread created successfully, thread handle: 0x%p", hThread);
        g_hUpdateThread = hThread;
    } else {
        // Thread creation failed, free parameter memory
        DWORD errorCode = GetLastError();
        char errorMsg[256] = {0};
        GetLastErrorDescription(errorCode, errorMsg, sizeof(errorMsg));
        LOG_ERROR("Update check thread creation failed, error code: %lu, error message: %s", errorCode, errorMsg);
        
        free(threadParams);
        g_bUpdateThreadRunning = FALSE;
    }
}