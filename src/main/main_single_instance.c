/**
 * @file main_single_instance.c
 * @brief Single instance detection and window finding implementation
 */

#include <windows.h>
#include "../../include/main/main_single_instance.h"
#include "../../include/main/main_cli_routing.h"
#include "../../include/log.h"
#include "../../include/utils/string_convert.h"

/** Search desktop wallpaper layer for timer window */
static HWND FindInDesktopLayer(void) {
    HWND hProgman = FindWindowW(L"Progman", NULL);
    if (!hProgman) return NULL;
    
    HWND hWorkerW = FindWindowExW(NULL, NULL, L"WorkerW", NULL);
    while (hWorkerW != NULL) {
        HWND hView = FindWindowExW(hWorkerW, NULL, L"SHELLDLL_DefView", NULL);
        if (hView != NULL) {
            return FindWindowExW(hWorkerW, NULL, L"CatimeWindow", NULL);
        }
        hWorkerW = FindWindowExW(NULL, hWorkerW, L"WorkerW", NULL);
    }
    
    return FindWindowExW(hProgman, NULL, L"CatimeWindow", NULL);
}

HWND FindExistingInstanceWindow(void) {
    HWND hwnd = FindWindowW(L"CatimeWindow", L"Catime");
    if (hwnd) return hwnd;
    
    return FindInDesktopLayer();
}

BOOL HandleSingleInstance(LPWSTR lpCmdLine, HANDLE* outMutex) {
    LOG_INFO("Checking if another instance is running...");
    
    HANDLE hMutex = CreateMutex(NULL, TRUE, L"CatimeMutex");
    *outMutex = hMutex;
    
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        Sleep(50);
        return TRUE;
    }
    
    LOG_INFO("Detected another instance is running");
    HWND hwndExisting = FindExistingInstanceWindow();
    
    if (!hwndExisting) {
        LOG_WARNING("Could not find window handle of existing instance, but mutex exists");
        LOG_INFO("Will continue with current instance startup");
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        
        *outMutex = CreateMutex(NULL, TRUE, L"CatimeMutex");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            LOG_WARNING("Still have conflict after creating new mutex, possible race condition");
        }
        Sleep(50);
        return TRUE;
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
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
            return FALSE;
        } else {
            LOG_INFO("CLI command not suitable for forwarding, will restart instance");
        }
    }
    
    LOG_INFO("Closing existing instance to apply CLI arguments");
    SendMessage(hwndExisting, WM_CLOSE, 0, 0);
    Sleep(200);
    
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    
    LOG_INFO("Creating new mutex");
    *outMutex = CreateMutex(NULL, TRUE, L"CatimeMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        LOG_WARNING("Still have conflict after creating new mutex, possible race condition");
    }
    Sleep(50);
    
    return TRUE;
}

