/**
 * @file main.c
 * @brief Application main entry module implementation file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dwmapi.h>
#include <winnls.h>
#include <commdlg.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <commctrl.h>
#include "../resource/resource.h"
#include "../include/language.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/tray.h"
#include "../include/tray_menu.h"
#include "../include/timer.h"
#include "../include/window.h"
#include "../include/startup.h"
#include "../include/config.h"
#include "../include/window_procedure.h"
#include "../include/media.h"
#include "../include/notification.h"
#include "../include/cli.h"
#include "../include/async_update_checker.h"
#include "../include/log.h"
#include "../include/dialog_language.h"
#include "../include/shortcut_checker.h"

#include <tlhelp32.h>

// Required for older Windows SDK
#ifndef CSIDL_STARTUP
#endif

#ifndef CLSID_ShellLink
EXTERN_C const CLSID CLSID_ShellLink;
#endif

#ifndef IID_IShellLinkW
EXTERN_C const IID IID_IShellLinkW;
#endif

// Compiler directives
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "comctl32.lib")

extern void CleanupLogSystem(void);

int default_countdown_time = 0;
int CLOCK_DEFAULT_START_TIME = 300;
int elapsed_time = 0;
char inputText[256] = {0};
int message_shown = 0;
time_t last_config_time = 0;
RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];
int CLOCK_RECENT_FILES_COUNT = 0;
char CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH] = "";

extern char CLOCK_TEXT_COLOR[10];
extern char FONT_FILE_NAME[];
extern char FONT_INTERNAL_NAME[];
extern char PREVIEW_FONT_NAME[];
extern char PREVIEW_INTERNAL_NAME[];
extern BOOL IS_PREVIEWING;

INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
void ExitProgram(HWND hwnd);

/**
 * @brief Handle application startup mode
 * @param hwnd Main window handle
 */
static void HandleStartupMode(HWND hwnd) {
    LOG_INFO("Setting startup mode: %s", CLOCK_STARTUP_MODE);
    
    if (strcmp(CLOCK_STARTUP_MODE, "COUNT_UP") == 0) {
        LOG_INFO("Setting to count-up mode");
        CLOCK_COUNT_UP = TRUE;
        elapsed_time = 0;
    } else if (strcmp(CLOCK_STARTUP_MODE, "NO_DISPLAY") == 0) {
        LOG_INFO("Setting to hidden mode, window will be hidden");
        ShowWindow(hwnd, SW_HIDE);
        KillTimer(hwnd, 1);
        elapsed_time = CLOCK_TOTAL_TIME;
        CLOCK_IS_PAUSED = TRUE;
        message_shown = TRUE;
        countdown_message_shown = TRUE;
        countup_message_shown = TRUE;
        countdown_elapsed_time = 0;
        countup_elapsed_time = 0;
    } else if (strcmp(CLOCK_STARTUP_MODE, "SHOW_TIME") == 0) {
        LOG_INFO("Setting to show current time mode");
        CLOCK_SHOW_CURRENT_TIME = TRUE;
        CLOCK_LAST_TIME_UPDATE = 0;
    } else {
        LOG_INFO("Using default countdown mode");
    }
}

/**
 * @brief Forward simple CLI commands to an existing instance via WM_HOTKEY.
 *        Returns TRUE if forwarded and caller should exit without restarting.
 */
static BOOL TryForwardSimpleCliToExisting(HWND hwndExisting, const char* lpCmdLine) {
    if (!lpCmdLine || lpCmdLine[0] == '\0') return FALSE;

    // Trim spaces into buf
    char buf[256];
    strncpy(buf, lpCmdLine, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* p = buf; while (*p && isspace((unsigned char)*p)) p++;
    size_t len = strlen(p);
    while (len > 0 && isspace((unsigned char)p[len - 1])) { p[--len] = '\0'; }
    if (len == 0) return FALSE;

    // Single-letter or short tokens that map 1:1 to hotkeys
    if (len == 1) {
        char c = (char)tolower((unsigned char)p[0]);
        if (c == 's') { PostMessage(hwndExisting, WM_HOTKEY, HOTKEY_ID_SHOW_TIME, 0); return TRUE; }
        if (c == 'u') { PostMessage(hwndExisting, WM_HOTKEY, HOTKEY_ID_COUNT_UP, 0); return TRUE; }
        if (c == 'p') { PostMessage(hwndExisting, WM_HOTKEY, HOTKEY_ID_POMODORO, 0); return TRUE; }
        if (c == 'v') { PostMessage(hwndExisting, WM_HOTKEY, HOTKEY_ID_TOGGLE_VISIBILITY, 0); return TRUE; }
        if (c == 'e') { PostMessage(hwndExisting, WM_HOTKEY, HOTKEY_ID_EDIT_MODE, 0); return TRUE; }
        if (c == 'r') { PostMessage(hwndExisting, WM_HOTKEY, HOTKEY_ID_RESTART_TIMER, 0); return TRUE; }
        if (c == 'h') { PostMessage(hwndExisting, WM_APP_SHOW_CLI_HELP, 0, 0); return TRUE; }
    }

    // Two-char tokens
    if ((len == 2) && (tolower((unsigned char)p[0]) == 'p') && (tolower((unsigned char)p[1]) == 'r')) {
        PostMessage(hwndExisting, WM_HOTKEY, HOTKEY_ID_PAUSE_RESUME, 0);
        return TRUE;
    }

    // q1/q2/q3
    if (len == 2 && tolower((unsigned char)p[0]) == 'q' && (p[1] >= '1' && p[1] <= '3')) {
        if (p[1] == '1') { PostMessage(hwndExisting, WM_HOTKEY, HOTKEY_ID_QUICK_COUNTDOWN1, 0); return TRUE; }
        if (p[1] == '2') { PostMessage(hwndExisting, WM_HOTKEY, HOTKEY_ID_QUICK_COUNTDOWN2, 0); return TRUE; }
        if (p[1] == '3') { PostMessage(hwndExisting, WM_HOTKEY, HOTKEY_ID_QUICK_COUNTDOWN3, 0); return TRUE; }
    }

    return FALSE;
}

/**
 * @brief Find existing Catime window, regardless of topmost or desktop-child mode
 *
 * The app may reparent the main window to the desktop container (WorkerW/Progman)
 * when in non-topmost mode, which makes it a child window and thus invisible to
 * FindWindow (which only enumerates top-level windows). This helper searches both
 * the top-level window list and the desktop container for the window class
 * "CatimeWindow".
 */
static HWND FindExistingInstanceWindow(void) {
    // First try top-level window (topmost mode or before reattach)
    HWND hwnd = FindWindow("CatimeWindow", "Catime");
    if (hwnd) return hwnd;

    // Then try to locate our window as a child of the desktop container
    HWND hProgman = FindWindow("Progman", NULL);
    HWND hDesktop = NULL;
    if (hProgman != NULL) {
        hDesktop = hProgman;
        HWND hWorkerW = FindWindowEx(NULL, NULL, "WorkerW", NULL);
        while (hWorkerW != NULL) {
            HWND hView = FindWindowEx(hWorkerW, NULL, "SHELLDLL_DefView", NULL);
            if (hView != NULL) {
                hDesktop = hWorkerW;
                break;
            }
            hWorkerW = FindWindowEx(NULL, hWorkerW, "WorkerW", NULL);
        }
    }
    if (hDesktop != NULL) {
        // Window name may be NULL when WS_POPUP child; match by class only
        hwnd = FindWindowEx(hDesktop, NULL, "CatimeWindow", NULL);
        if (hwnd) return hwnd;
    }

    return NULL;
}

/**
 * @brief Application main entry point
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize Common Controls
    InitCommonControls();
    
    // Initialize log system
    if (!InitializeLogSystem()) {
        // If log system initialization fails, continue running but without logging
        MessageBox(NULL, "Log system initialization failed, the program will continue running but will not log.", "Warning", MB_ICONWARNING);
    }

    // Set up exception handler
    SetupExceptionHandler();

    LOG_INFO("Catime is starting...");
        // Initialize COM
        HRESULT hr = CoInitialize(NULL);
        if (FAILED(hr)) {
            LOG_ERROR("COM initialization failed, error code: 0x%08X", hr);
            MessageBox(NULL, "COM initialization failed!", "Error", MB_ICONERROR);
            return 1;
        }
        LOG_INFO("COM initialization successful");

        // Initialize application
        LOG_INFO("Starting application initialization...");
        if (!InitializeApplication(hInstance)) {
            LOG_ERROR("Application initialization failed");
            MessageBox(NULL, "Application initialization failed!", "Error", MB_ICONERROR);
            return 1;
        }
        LOG_INFO("Application initialization successful");

        // Check and create desktop shortcut (if necessary)
        LOG_INFO("Checking desktop shortcut...");
        char exe_path[MAX_PATH];
        GetModuleFileNameA(NULL, exe_path, MAX_PATH);
        LOG_INFO("Current program path: %s", exe_path);
        
        // Set log level to DEBUG to show detailed information
        WriteLog(LOG_LEVEL_DEBUG, "Starting shortcut detection, checking path: %s", exe_path);
        
        // Check if path contains WinGet identifier
        if (strstr(exe_path, "WinGet") != NULL) {
            WriteLog(LOG_LEVEL_DEBUG, "Path contains WinGet keyword");
        }
        
        // Additional test: directly test if file exists
        char desktop_path[MAX_PATH];
        char shortcut_path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, 0, desktop_path))) {
            sprintf(shortcut_path, "%s\\Catime.lnk", desktop_path);
            WriteLog(LOG_LEVEL_DEBUG, "Checking if desktop shortcut exists: %s", shortcut_path);
            if (GetFileAttributesA(shortcut_path) == INVALID_FILE_ATTRIBUTES) {
                WriteLog(LOG_LEVEL_DEBUG, "Desktop shortcut does not exist, need to create");
            } else {
                WriteLog(LOG_LEVEL_DEBUG, "Desktop shortcut already exists");
            }
        }
        
        int shortcut_result = CheckAndCreateShortcut();
        if (shortcut_result == 0) {
            LOG_INFO("Desktop shortcut check completed");
        } else {
            LOG_WARNING("Desktop shortcut creation failed, error code: %d", shortcut_result);
        }

        // Initialize dialog multi-language support
        LOG_INFO("Starting dialog multi-language support initialization...");
        if (!InitDialogLanguageSupport()) {
            LOG_WARNING("Dialog multi-language support initialization failed, but program will continue running");
        }
        LOG_INFO("Dialog multi-language support initialization successful");

        // Handle single instance
        LOG_INFO("Checking if another instance is running...");
        HANDLE hMutex = CreateMutex(NULL, TRUE, "CatimeMutex");
        DWORD mutexError = GetLastError();
        
        if (mutexError == ERROR_ALREADY_EXISTS) {
            LOG_INFO("Detected another instance is running");
            HWND hwndExisting = FindExistingInstanceWindow();
            if (hwndExisting) {
                // If command line is just 'h' (help), forward to existing instance and exit
                if (lpCmdLine && lpCmdLine[0] != '\0') {
                    if (TryForwardSimpleCliToExisting(hwndExisting, lpCmdLine)) {
                        LOG_INFO("Forwarded simple CLI command to existing instance and exiting");
                        ReleaseMutex(hMutex);
                        CloseHandle(hMutex);
                        CoUninitialize();
                        CleanupLogSystem();
                        return 0;
                    }
                }
                // Otherwise, close existing instance and continue startup
                LOG_INFO("Closing existing instance to apply CLI arguments");
                SendMessage(hwndExisting, WM_CLOSE, 0, 0);
                Sleep(200);
            } else {
                LOG_WARNING("Could not find window handle of existing instance, but mutex exists");
            }
            // Release old mutex
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
            
            // Create new mutex
            LOG_INFO("Creating new mutex");
            hMutex = CreateMutex(NULL, TRUE, "CatimeMutex");
            if (GetLastError() == ERROR_ALREADY_EXISTS) {
                LOG_WARNING("Still have conflict after creating new mutex, possible race condition");
            }
        }
        Sleep(50);

        // Create main window
        LOG_INFO("Starting main window creation...");
        HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
        if (!hwnd) {
            LOG_ERROR("Main window creation failed");
            MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
            return 0;
        }
        LOG_INFO("Main window creation successful, handle: 0x%p", hwnd);

        // Determine if launched via system startup
        BOOL launchedFromStartup = FALSE;
        char cmdBuf[512] = {0};
        if (lpCmdLine && lpCmdLine[0] != '\0') {
            strncpy(cmdBuf, lpCmdLine, sizeof(cmdBuf) - 1);
            cmdBuf[sizeof(cmdBuf) - 1] = '\0';
            // Check and strip internal flag "--startup"
            char* pStartup = strstr(cmdBuf, "--startup");
            if (pStartup) {
                launchedFromStartup = TRUE;
                size_t len = strlen("--startup");
                memmove(pStartup, pStartup + len, strlen(pStartup + len) + 1);
            }
            LOG_INFO("Command line detected: %s", lpCmdLine);
            if (HandleCliArguments(hwnd, cmdBuf)) {
                LOG_INFO("CLI countdown started successfully");
            } else {
                LOG_INFO("CLI arguments not parsed as countdown");
            }
        }

        // Set timer (ensure timer exists even if not started by CLI)
        LOG_INFO("Setting main timer...");
        if (SetTimer(hwnd, 1, 1000, NULL) == 0) {
            DWORD timerError = GetLastError();
            LOG_ERROR("Timer creation failed, error code: %lu", timerError);
            MessageBox(NULL, "Timer Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
            return 0;
        }
        LOG_INFO("Timer set successfully");

        // Handle startup mode
        LOG_INFO("Handling startup mode: %s", CLOCK_STARTUP_MODE);
        HandleStartupMode(hwnd);

        // Only when launched via system startup do we schedule topmost/desktop reattach retries
        if (launchedFromStartup) {
            if (CLOCK_WINDOW_TOPMOST) {
                SetTimer(hwnd, 999, 2000, NULL);
            } else {
                SetTimer(hwnd, 1001, 1500, NULL);
            }
        }
        
        // Automatic update check code has been removed

        // Message loop
        LOG_INFO("Entering main message loop");
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0) > 0) {
            // Route messages to modeless CLI help dialog if present
            HWND hCliHelp = GetCliHelpDialog();
            if (hCliHelp && IsDialogMessage(hCliHelp, &msg)) {
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Clean up resources
        LOG_INFO("Program preparing to exit, starting resource cleanup");
        
        // Clean up update check thread resources
        LOG_INFO("Preparing to clean up update check thread resources");
        CleanupUpdateThread();
        
        CloseHandle(hMutex);
        CoUninitialize();
        
        // Close log system
        CleanupLogSystem();
        
        return (int)msg.wParam;
    // If execution reaches here, the program has exited normally
}
