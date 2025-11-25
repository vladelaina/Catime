/**
 * @file main_initialization.c
 * @brief Application initialization and window setup implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>
#include "main/main_initialization.h"
#include "log.h"
#include "config.h"
#include "timer/timer.h"
#include "timer/timer_events.h"
#include "window.h"
#include "cli.h"
#include "async_update_checker.h"
#include "dialog/dialog_language.h"
#include "shortcut_checker.h"
#include "utils/string_convert.h"
#include "cache/resource_cache.h"
#include "plugin/plugin_data.h"
#include "plugin/plugin_manager.h"
#include <tlhelp32.h>

/* Helper to check if process is elevated */
static BOOL IsElevated(void) {
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
            fRet = Elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return fRet;
}

/* Attempt to relaunch self as standard user using Explorer's token */
static BOOL RelaunchAsStandardUser(void) {
    HWND hShellWnd = GetShellWindow();
    if (!hShellWnd) return FALSE;

    DWORD dwShellPID;
    GetWindowThreadProcessId(hShellWnd, &dwShellPID);

    HANDLE hShellProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwShellPID);
    if (!hShellProcess) return FALSE;

    HANDLE hShellToken = NULL;
    if (!OpenProcessToken(hShellProcess, TOKEN_DUPLICATE, &hShellToken)) {
        CloseHandle(hShellProcess);
        return FALSE;
    }

    HANDLE hNewToken = NULL;
    /* Duplicate the shell's token (which is medium integrity/standard user) */
    if (!DuplicateTokenEx(hShellToken, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &hNewToken)) {
        CloseHandle(hShellToken);
        CloseHandle(hShellProcess);
        return FALSE;
    }

    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    
    /* Reconstruct command line safely - CreateProcess might modify the buffer */
    wchar_t* pszOriginalCmdLine = GetCommandLineW();
    size_t cmdLen = wcslen(pszOriginalCmdLine) + 1;
    wchar_t* pszCmdLineCopy = (wchar_t*)malloc(cmdLen * sizeof(wchar_t));
    
    if (!pszCmdLineCopy) {
        CloseHandle(hNewToken);
        CloseHandle(hShellToken);
        CloseHandle(hShellProcess);
        return FALSE;
    }
    
    wcscpy_s(pszCmdLineCopy, cmdLen, pszOriginalCmdLine);
    
    STARTUPINFOW si = {sizeof(STARTUPINFOW)};
    PROCESS_INFORMATION pi = {0};
    
    BOOL bResult = CreateProcessWithTokenW(hNewToken, LOGON_WITH_PROFILE, NULL, pszCmdLineCopy, 0, NULL, NULL, &si, &pi);

    free(pszCmdLineCopy);

    if (bResult) {
        LOG_INFO("Relaunched self as standard user (PID: %lu)", pi.dwProcessId);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        LOG_ERROR("Failed to relaunch as standard user, error: %lu", GetLastError());
    }

    CloseHandle(hNewToken);
    CloseHandle(hShellToken);
    CloseHandle(hShellProcess);
    
    return bResult;
}

/* Drop Administrator privileges if present to ensure Drag & Drop works from Explorer */
static void DropPrivileges(void) {
    if (IsElevated()) {
        LOG_INFO("Elevated privileges detected. Attempting to switch to standard user...");
        
        /* Prevent infinite loop if relaunch fails or we are genuinely the admin user (e.g. built-in Admin) */
        /* But here we just try once. If it succeeds, we exit. */
        
        if (RelaunchAsStandardUser()) {
            /* Exit this elevated instance */
            ExitProcess(0);
        } else {
            LOG_WARNING("Failed to switch to standard user. Drag & Drop may be restricted.");
        }
    } else {
        LOG_INFO("Running as standard user (optimal for Drag & Drop).");
    }
}

extern int elapsed_time;
extern int message_shown;
extern int countdown_message_shown;
extern int countup_message_shown;
extern int countdown_elapsed_time;
extern int countup_elapsed_time;

typedef enum {
    STARTUP_MODE_DEFAULT,
    STARTUP_MODE_COUNT_UP,
    STARTUP_MODE_NO_DISPLAY,
    STARTUP_MODE_SHOW_TIME
} StartupMode;

static StartupMode ParseStartupMode(const char* modeStr) {
    if (!modeStr) return STARTUP_MODE_DEFAULT;
    
    if (strcmp(modeStr, "COUNT_UP") == 0) return STARTUP_MODE_COUNT_UP;
    if (strcmp(modeStr, "NO_DISPLAY") == 0) return STARTUP_MODE_NO_DISPLAY;
    if (strcmp(modeStr, "SHOW_TIME") == 0) return STARTUP_MODE_SHOW_TIME;
    
    return STARTUP_MODE_DEFAULT;
}

static void HandleStartupMode(HWND hwnd) {
    StartupMode mode = ParseStartupMode(CLOCK_STARTUP_MODE);
    
    LOG_INFO("Setting startup mode: %s", CLOCK_STARTUP_MODE);
    
    switch (mode) {
        case STARTUP_MODE_COUNT_UP:
            LOG_INFO("Setting to count-up mode");
            CLOCK_COUNT_UP = TRUE;
            elapsed_time = 0;
            break;
            
        case STARTUP_MODE_NO_DISPLAY:
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
            break;
            
        case STARTUP_MODE_SHOW_TIME:
            LOG_INFO("Setting to show current time mode");
            CLOCK_SHOW_CURRENT_TIME = TRUE;
            CLOCK_LAST_TIME_UPDATE = 0;
            break;
            
        case STARTUP_MODE_DEFAULT:
        default:
            LOG_INFO("Using default countdown mode");
            break;
    }
}

BOOL InitializeSubsystems(void) {
    InitCommonControls();
    
    if (!InitializeLogSystem()) {
        MessageBoxW(NULL, 
                   L"Log system initialization failed, the program will continue running but will not log.", 
                   L"Warning", MB_ICONWARNING);
    }
    
    SetupExceptionHandler();
    LOG_INFO("Catime is starting...");
    
    DropPrivileges();
    
    // Initialize DWM functions for visual effects (Blur/Glass)
    if (!InitDWMFunctions()) {
        LOG_WARNING("DWM functions failed to load, visual effects may be limited");
    }
    
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        LOG_ERROR("COM initialization failed, error code: 0x%08X", hr);
        MessageBoxW(NULL, L"COM initialization failed!", L"Error", MB_ICONERROR);
        return FALSE;
    }
    LOG_INFO("COM initialization successful");
    
    // Initialize resource cache system with background scanning
    if (!ResourceCache_Initialize(TRUE)) {
        LOG_WARNING("Resource cache initialization failed, menu building will use fallback mode");
    } else {
        LOG_INFO("Resource cache initialized with background scanning");
    }

    // Initialize plugin manager
    PluginManager_Init();
    PluginManager_ScanPlugins();
    PluginManager_StartAllPlugins();
    LOG_INFO("Plugin manager initialized");

    return TRUE;
}

BOOL InitializeApplicationSubsystem(HINSTANCE hInstance) {
    LOG_INFO("Starting application initialization...");
    
    extern BOOL InitializeApplication(HINSTANCE);
    if (!InitializeApplication(hInstance)) {
        LOG_ERROR("Application initialization failed");
        MessageBoxW(NULL, L"Application initialization failed!", L"Error", MB_ICONERROR);
        return FALSE;
    }

    LOG_INFO("Application initialization successful");
    return TRUE;
}

void SetupDesktopShortcut(void) {
    LOG_INFO("Checking desktop shortcut...");
    
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    
    char* exe_path_utf8 = WideToUtf8Alloc(exe_path);
    if (exe_path_utf8) {
        LOG_INFO("Current program path: %s", exe_path_utf8);
        free(exe_path_utf8);
    }
    
    int result = CheckAndCreateShortcut();
    if (result == 0) {
        LOG_INFO("Desktop shortcut check completed");
    } else {
        LOG_WARNING("Desktop shortcut creation failed, error code: %d", result);
    }
}

void InitializeDialogLanguages(void) {
    LOG_INFO("Starting dialog multi-language support initialization...");
    if (!InitDialogLanguageSupport()) {
        LOG_WARNING("Dialog multi-language support initialization failed, but program will continue running");
    } else {
        LOG_INFO("Dialog multi-language support initialization successful");
    }
}

BOOL SetupMainWindow(HINSTANCE hInstance, HWND hwnd, int nCmdShow) {
    (void)nCmdShow; // Unused parameter
    
    LPWSTR lpCmdLineW = GetCommandLineW();
    while (*lpCmdLineW && *lpCmdLineW != L' ') lpCmdLineW++;
    while (*lpCmdLineW == L' ') lpCmdLineW++;
    
    BOOL launchedFromStartup = FALSE;
    wchar_t cmdBuf[512] = {0};
    
    if (lpCmdLineW && lpCmdLineW[0] != L'\0') {
        wcsncpy(cmdBuf, lpCmdLineW, sizeof(cmdBuf)/sizeof(wchar_t) - 1);
        cmdBuf[sizeof(cmdBuf)/sizeof(wchar_t) - 1] = L'\0';
        
        wchar_t* pStartup = wcsstr(cmdBuf, L"--startup");
        if (pStartup) {
            launchedFromStartup = TRUE;
            size_t len = wcslen(L"--startup");
            wmemmove(pStartup, pStartup + len, wcslen(pStartup + len) + 1);
        }
        
        char* cmdUtf8 = WideToUtf8Alloc(lpCmdLineW);
        if (cmdUtf8) {
            LOG_INFO("Command line detected: %s", cmdUtf8);
            free(cmdUtf8);
        }
        
        char* cmdCliUtf8 = WideToUtf8Alloc(cmdBuf);
        if (cmdCliUtf8) {
            if (HandleCliArguments(hwnd, cmdCliUtf8)) {
                LOG_INFO("CLI countdown started successfully");
            } else {
                LOG_INFO("CLI arguments not parsed as countdown");
            }
            free(cmdCliUtf8);
        }
    }
    
    LOG_INFO("Setting main timer...");
    extern UINT GetTimerInterval(void);
    UINT interval = GetTimerInterval();
    
    if (SetTimer(hwnd, 1, interval, NULL) == 0) {
        LOG_ERROR("Timer creation failed, error code: %lu", GetLastError());
        MessageBoxW(NULL, L"Timer Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return FALSE;
    }
    LOG_INFO("Timer set successfully with %ums interval", interval);
    
    extern void ResetTimerMilliseconds(void);
    ResetTimerMilliseconds();
    
    LOG_INFO("Setting font path check timer...");
    if (SetTimer(hwnd, TIMER_ID_FONT_VALIDATION, 2000, NULL) == 0) {
        LOG_WARNING("Font path check timer creation failed, auto-fix will not work");
    } else {
        LOG_INFO("Font path check timer set successfully (2 second interval)");
    }
    
    LOG_INFO("Starting automatic update check at startup...");
    CheckForUpdateAsync(hwnd, TRUE);
    
    LOG_INFO("Handling startup mode: %s", CLOCK_STARTUP_MODE);
    HandleStartupMode(hwnd);
    
    if (launchedFromStartup) {
        if (CLOCK_WINDOW_TOPMOST) {
            SetTimer(hwnd, TIMER_ID_TOPMOST_RETRY, 2000, NULL);
        } else {
            SetTimer(hwnd, TIMER_ID_VISIBILITY_RETRY, 2000, NULL);
        }
    }
    
    // Initialize Plugin Data subsystem (File Watcher)
    PluginData_Init(hwnd);

    return TRUE;
}

int RunMessageLoop(HWND hwnd) {
    (void)hwnd; // May be used for future enhancements
    
    LOG_INFO("Entering main message loop");
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        extern HWND GetCliHelpDialog(void);
        HWND hCliHelp = GetCliHelpDialog();
        if (hCliHelp && IsDialogMessage(hCliHelp, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}

void CleanupResources(HANDLE hMutex) {
    LOG_INFO("Program preparing to exit, starting resource cleanup");

    LOG_INFO("Preparing to clean up update check thread resources");
    CleanupUpdateThread();

    LOG_INFO("Shutting down plugin manager");
    PluginManager_Shutdown();

    LOG_INFO("Shutting down plugin data subsystem");
    PluginData_Shutdown();

    if (hMutex) {
        CloseHandle(hMutex);
    }

    CoUninitialize();
    CleanupLogSystem();
}

