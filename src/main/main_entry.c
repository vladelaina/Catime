/**
 * @file main_entry.c
 * @brief WinMain entry point
 */

#include <time.h>
#include <windows.h>
#include "main/main_initialization.h"
#include "main/main_single_instance.h"
#include "window.h"
#include "window/window_visual_effects.h"
#include "log.h"
#include "config.h"

#ifdef _MSC_VER
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "comctl32.lib")
#endif

/** Global variables not yet refactored into AppConfig */
int default_countdown_time = 0;
int elapsed_time = 0;
wchar_t inputText[256] = {0};
int message_shown = 0;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    if (!InitializeSubsystems()) {
        return 1;
    }

    HANDLE hMutex = NULL;
    if (!HandleSingleInstance(GetCommandLineW(), &hMutex)) {
        ShutdownWindowVisualEffects();
        CoUninitialize();
        CleanupLogSystem();
        return 0;
    }

    if (!InitializeApplicationSubsystem(hInstance)) {
        CleanupResources(hMutex);
        return 1;
    }

    if (!IsCiSmokeMode()) {
        SetupDesktopShortcut();
    }
    InitializeDialogLanguages();

    LOG_INFO("Starting main window creation...");
    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (!hwnd) {
        LOG_ERROR("Main window creation failed. Application cannot continue. Check log file for details.");
        CleanupResources(hMutex);
        return 0;
    }
    LOG_INFO("Main window creation successful, handle: 0x%p", hwnd);

    if (!SetupMainWindow(hInstance, hwnd, nCmdShow)) {
        DestroyWindow(hwnd);
        CleanupResources(hMutex);
        return 0;
    }

    int exitCode = RunMessageLoop(hwnd);

    CleanupResources(hMutex);

    return exitCode;
}
