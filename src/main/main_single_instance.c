/**
 * @file main_single_instance.c
 * @brief Single instance detection and window finding implementation
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>
#include "main/main_single_instance.h"
#include "main/main_cli_routing.h"
#include "log.h"
#include "utils/string_convert.h"

/** Global mutex handle for emergency cleanup in crash scenarios */
static HANDLE g_GlobalMutex = NULL;

#define SINGLE_INSTANCE_MUTEX_NAME L"Local\\Vladelaina.Catime.SingleInstance"
#define EXISTING_WINDOW_RETRY_COUNT 20
#define EXISTING_WINDOW_RETRY_DELAY_MS 100

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

static HWND FindExistingInstanceWindowWithRetry(void) {
    for (int i = 0; i < EXISTING_WINDOW_RETRY_COUNT; ++i) {
        HWND hwnd = FindExistingInstanceWindow();
        if (hwnd && IsWindow(hwnd)) {
            return hwnd;
        }
        Sleep(EXISTING_WINDOW_RETRY_DELAY_MS);
    }
    return NULL;
}

static void TryActivateExistingWindow(HWND hwndExisting) {
    if (!hwndExisting || !IsWindow(hwndExisting)) return;

    if (!IsWindowVisible(hwndExisting)) {
        ShowWindow(hwndExisting, SW_SHOWNA);
    }

    if (IsIconic(hwndExisting)) {
        ShowWindow(hwndExisting, SW_RESTORE);
    }

    BringWindowToTop(hwndExisting);
    SetForegroundWindow(hwndExisting);
}

static BOOL ExtractCommandArguments(LPCWSTR fullCmdLine, wchar_t* outArgs, size_t outCount) {
    if (!outArgs || outCount == 0) return FALSE;

    outArgs[0] = L'\0';

    if (!fullCmdLine || fullCmdLine[0] == L'\0') return FALSE;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(fullCmdLine, &argc);
    if (!argv || argc <= 1) {
        if (argv) LocalFree(argv);
        return FALSE;
    }

    size_t used = 0;
    BOOL hasArgs = FALSE;
    for (int i = 1; i < argc; ++i) {
        const wchar_t* arg = argv[i];
        if (!arg || arg[0] == L'\0') continue;

        size_t argLen = wcslen(arg);
        if (used > 0) {
            if (used + 1 >= outCount) break;
            outArgs[used++] = L' ';
        }

        size_t copyLen = argLen;
        if (copyLen > outCount - used - 1) {
            copyLen = outCount - used - 1;
        }
        if (copyLen == 0) break;

        memcpy(outArgs + used, arg, copyLen * sizeof(wchar_t));
        used += copyLen;
        outArgs[used] = L'\0';
        hasArgs = TRUE;
    }

    LocalFree(argv);
    return hasArgs;
}

BOOL HandleSingleInstance(LPWSTR lpCmdLine, HANDLE* outMutex) {
    LOG_INFO("Checking if another instance is running...");

    if (!outMutex) {
        LOG_ERROR("HandleSingleInstance called with NULL outMutex");
        return FALSE;
    }
    *outMutex = NULL;

    HANDLE hMutex = CreateMutexW(NULL, TRUE, SINGLE_INSTANCE_MUTEX_NAME);
    if (!hMutex) {
        LOG_ERROR("CreateMutexW failed for single-instance mutex (err=%lu)", GetLastError());
        return FALSE;
    }

    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        g_GlobalMutex = hMutex;  /* Store globally for crash cleanup */
        *outMutex = hMutex;
        return TRUE;
    }

    LOG_INFO("Detected another instance is running");

    HWND hwndExisting = FindExistingInstanceWindowWithRetry();

    wchar_t forwardedArgs[512];
    BOOL hasArgs = ExtractCommandArguments(lpCmdLine, forwardedArgs, sizeof(forwardedArgs) / sizeof(forwardedArgs[0]));

    if (hasArgs) {
        char* cmdUtf8 = WideToUtf8Alloc(forwardedArgs);
        if (cmdUtf8) {
            LOG_INFO("Command line arguments: '%s'", cmdUtf8);
            free(cmdUtf8);
        }
    }

    if (!hwndExisting) {
        LOG_WARNING("Single-instance mutex exists but no existing window was found. Exiting to enforce single instance.");
        CloseHandle(hMutex);
        return FALSE;
    }

    LOG_INFO("Found existing instance window handle: 0x%p", hwndExisting);

    if (hasArgs) {
        if (TryForwardSimpleCliToExisting(hwndExisting, forwardedArgs)) {
            LOG_INFO("Forwarded CLI command to existing instance and exiting");
        } else {
            LOG_WARNING("Failed to forward CLI command to existing instance");
            TryActivateExistingWindow(hwndExisting);
        }
    } else {
        TryActivateExistingWindow(hwndExisting);
    }

    CloseHandle(hMutex);  /* We do not own this handle when ERROR_ALREADY_EXISTS */
    return FALSE;
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

