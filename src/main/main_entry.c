/**
 * @file main_entry.c
 * @brief WinMain entry point
 */

#include <time.h>
#include <windows.h>
#include "main/main_initialization.h"
#include "main/main_single_instance.h"
#include "window.h"
#include "log.h"
#include "config.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "comctl32.lib")

/** Global variables not yet refactored into AppConfig */
int default_countdown_time = 0;
int elapsed_time = 0;
wchar_t inputText[256] = {0};
int message_shown = 0;
wchar_t CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH] = L"";

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    
    if (!InitializeSubsystems()) {
        return 1;
    }
    
    if (!InitializeApplicationSubsystem(hInstance)) {
        CoUninitialize();
        CleanupLogSystem();
        return 1;
    }

    
    SetupDesktopShortcut();
    InitializeDialogLanguages();
    
    HANDLE hMutex = NULL;
    if (!HandleSingleInstance(GetCommandLineW(), &hMutex)) {
        CoUninitialize();
        CleanupLogSystem();
        return 0;
    }
    
    LOG_INFO("Starting main window creation...");
    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (!hwnd) {
        LOG_ERROR("Main window creation failed");
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        CleanupResources(hMutex);
        return 0;
    }
    LOG_INFO("Main window creation successful, handle: 0x%p", hwnd);
    
    if (!SetupMainWindow(hInstance, hwnd, nCmdShow)) {
        CleanupResources(hMutex);
        return 0;
    }
    
    int exitCode = RunMessageLoop(hwnd);
    
    CleanupResources(hMutex);
    
    return exitCode;
}

