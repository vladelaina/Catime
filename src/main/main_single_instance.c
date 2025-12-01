/**
 * @file main_single_instance.c
 * @brief Single instance detection and window finding implementation
 */

#include <windows.h>
#include <stdio.h>
#include "main/main_single_instance.h"
#include "main/main_cli_routing.h"
#include "log.h"
#include "utils/string_convert.h"

/** Global mutex handle for emergency cleanup in crash scenarios */
static HANDLE g_GlobalMutex = NULL;

/** Search desktop wallpaper layer for timer window */
static HWND FindInDesktopLayer(void) {
    HWND hProgman = FindWindowW(L"Progman", NULL);
    if (!hProgman) return NULL;

    HWND hWorkerW = FindWindowExW(NULL, NULL, L"WorkerW", NULL);
    while (hWorkerW != NULL) {
        HWND hView = FindWindowExW(hWorkerW, NULL, L"SHELLDLL_DefView", NULL);
        if (hView != NULL) {
            return FindWindowExW(hWorkerW, NULL, L"CatimeWindowClass", NULL);
        }
        hWorkerW = FindWindowExW(NULL, hWorkerW, L"WorkerW", NULL);
    }

    return FindWindowExW(hProgman, NULL, L"CatimeWindowClass", NULL);
}

HWND FindExistingInstanceWindow(void) {
    HWND hwnd = FindWindowW(L"CatimeWindowClass", L"Catime");
    if (hwnd) return hwnd;

    return FindInDesktopLayer();
}

BOOL HandleSingleInstance(LPWSTR lpCmdLine, HANDLE* outMutex) {
    LOG_INFO("Checking if another instance is running...");
    
    HANDLE hMutex = CreateMutex(NULL, TRUE, L"CatimeMutex");
    *outMutex = hMutex;
    g_GlobalMutex = hMutex;  /* Store globally for crash cleanup */
    
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        Sleep(50);  /* Brief delay for mutex acquisition to stabilize */
        return TRUE;
    }
    
    LOG_INFO("Detected another instance is running");
    HWND hwndExisting = FindExistingInstanceWindow();
    
    if (!hwndExisting) {
        LOG_WARNING("Could not find window handle of existing instance, but mutex exists");
        LOG_WARNING("Possible zombie process or race condition detected");
        
        /* Wait for the mutex to be released by the zombie process */
        LOG_INFO("Waiting for mutex to be released (max 3 seconds)...");
        DWORD waitResult = WaitForSingleObject(hMutex, 3000);
        
        if (waitResult == WAIT_OBJECT_0) {
            /* Successfully acquired mutex ownership */
            LOG_INFO("Mutex acquired successfully after waiting");
            *outMutex = hMutex;
            return TRUE;
        } else if (waitResult == WAIT_ABANDONED) {
            /* Previous owner terminated abnormally - we now own the mutex */
            LOG_WARNING("Mutex was abandoned by zombie process. Acquired ownership.");
            *outMutex = hMutex;
            return TRUE;
        } else if (waitResult == WAIT_TIMEOUT) {
            /* Timeout - zombie process still holds mutex */
            LOG_ERROR("Timeout waiting for mutex release. Force creating new instance.");
            CloseHandle(hMutex);
            g_GlobalMutex = NULL;
            
            /* Use unique mutex name to avoid conflict with zombie */
            wchar_t uniqueMutexName[128];
            swprintf(uniqueMutexName, 128, L"CatimeMutex_%lu", GetTickCount());
            *outMutex = CreateMutex(NULL, TRUE, uniqueMutexName);
            g_GlobalMutex = *outMutex;
            LOG_WARNING("Created unique mutex due to zombie process: %ls", uniqueMutexName);
            return TRUE;
        } else {
            /* Wait failed */
            LOG_ERROR("Failed to wait for mutex (error: %lu, result: 0x%08X)", GetLastError(), waitResult);
            CloseHandle(hMutex);
            g_GlobalMutex = NULL;
            
            /* Use unique mutex name */
            wchar_t uniqueMutexName[128];
            swprintf(uniqueMutexName, 128, L"CatimeMutex_%lu", GetTickCount());
            *outMutex = CreateMutex(NULL, TRUE, uniqueMutexName);
            g_GlobalMutex = *outMutex;
            LOG_WARNING("Created unique mutex due to wait failure: %ls", uniqueMutexName);
            return TRUE;
        }
    }
    
    LOG_INFO("Found existing instance window handle: 0x%p", hwndExisting);
    
    LPWSTR lpCmdLineW = GetCommandLineW();
    while (*lpCmdLineW && *lpCmdLineW != L' ') lpCmdLineW++;
    while (*lpCmdLineW == L' ') lpCmdLineW++;
    
    if (lpCmdLineW && lpCmdLineW[0] != L'\0') {
        char* cmdUtf8 = WideToUtf8Alloc(lpCmdLineW);
        if (cmdUtf8) {
            LOG_INFO("Command line arguments: '%s'", cmdUtf8);
            free(cmdUtf8);
        }
        
        if (TryForwardSimpleCliToExisting(hwndExisting, lpCmdLineW)) {
            LOG_INFO("Forwarded simple CLI command to existing instance and exiting");
            /* Do not call ReleaseMutex - we don't own the mutex when ERROR_ALREADY_EXISTS */
            CloseHandle(hMutex);
            g_GlobalMutex = NULL;
            *outMutex = NULL;
            return FALSE;
        } else {
            LOG_INFO("CLI command not suitable for forwarding, will restart instance");
        }
    }
    
    LOG_INFO("Closing existing instance to apply CLI arguments");
    SendMessage(hwndExisting, WM_CLOSE, 0, 0);
    
    /* Wait for the existing instance to release the mutex */
    LOG_INFO("Waiting for existing instance to release mutex (max 5 seconds)...");
    DWORD waitResult = WaitForSingleObject(hMutex, 5000);
    
    if (waitResult == WAIT_OBJECT_0) {
        /* Successfully acquired mutex ownership from the closed instance */
        LOG_INFO("Mutex acquired successfully after existing instance closed");
        *outMutex = hMutex;
        g_GlobalMutex = hMutex;
        return TRUE;
    } else if (waitResult == WAIT_ABANDONED) {
        /* Previous instance terminated abnormally - we now own the mutex */
        LOG_WARNING("Mutex was abandoned by previous instance. Acquired ownership.");
        *outMutex = hMutex;
        g_GlobalMutex = hMutex;
        return TRUE;
    } else if (waitResult == WAIT_TIMEOUT) {
        /* Timeout - existing instance didn't release mutex in time */
        LOG_WARNING("Timeout waiting for existing instance to close. Will retry...");
        
        /* Give it one more chance with force close */
        SendMessage(hwndExisting, WM_DESTROY, 0, 0);
        Sleep(500);
        
        waitResult = WaitForSingleObject(hMutex, 2000);
        if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED) {
            if (waitResult == WAIT_ABANDONED) {
                LOG_WARNING("Mutex was abandoned after force close. Acquired ownership.");
            } else {
                LOG_INFO("Mutex acquired after force close");
            }
            *outMutex = hMutex;
            g_GlobalMutex = hMutex;
            return TRUE;
        }
        
        /* Still timeout - force create new instance */
        LOG_ERROR("Failed to acquire mutex after force close. Creating new instance anyway.");
        CloseHandle(hMutex);
        g_GlobalMutex = NULL;
        
        /* Use unique mutex name to avoid conflict */
        wchar_t uniqueMutexName[128];
        swprintf(uniqueMutexName, 128, L"CatimeMutex_%lu", GetTickCount());
        *outMutex = CreateMutex(NULL, TRUE, uniqueMutexName);
        g_GlobalMutex = *outMutex;
        LOG_WARNING("Created unique mutex after force close failed: %ls", uniqueMutexName);
        return TRUE;
    } else {
        /* Wait failed */
        LOG_ERROR("Failed to wait for mutex (error: %lu, result: 0x%08X)", GetLastError(), waitResult);
        CloseHandle(hMutex);
        g_GlobalMutex = NULL;
        
        /* Use unique mutex name */
        wchar_t uniqueMutexName[128];
        swprintf(uniqueMutexName, 128, L"CatimeMutex_%lu", GetTickCount());
        *outMutex = CreateMutex(NULL, TRUE, uniqueMutexName);
        g_GlobalMutex = *outMutex;
        LOG_WARNING("Created unique mutex due to wait failure: %ls", uniqueMutexName);
        return TRUE;
    }
}

/**
 * Verify single instance mutex is still valid.
 * Should be called periodically or before critical operations.
 */
BOOL VerifySingleInstanceMutex(HANDLE hMutex) {
    if (!hMutex) return FALSE;
    
    /* Try to wait with 0 timeout - just checks if we still own it */
    DWORD result = WaitForSingleObject(hMutex, 0);
    
    if (result == WAIT_OBJECT_0) {
        /* We own it - immediately release to maintain state */
        ReleaseMutex(hMutex);
        return TRUE;
    } else if (result == WAIT_ABANDONED) {
        /* This shouldn't happen if we own it, but handle it */
        LOG_WARNING("Mutex ownership verification detected abandoned state");
        return TRUE;
    } else if (result == WAIT_TIMEOUT) {
        /* Another thread/process owns it - this is bad */
        LOG_ERROR("Single instance mutex is owned by another process!");
        return FALSE;
    }
    
    return FALSE;
}

/**
 * Get global mutex handle for emergency cleanup.
 * Used in crash handlers where normal cleanup path is not available.
 */
HANDLE GetGlobalMutexHandle(void) {
    return g_GlobalMutex;
}

/**
 * Clear global mutex handle.
 * Called after normal cleanup to prevent double-free in crash scenarios.
 */
void ClearGlobalMutexHandle(void) {
    g_GlobalMutex = NULL;
}

