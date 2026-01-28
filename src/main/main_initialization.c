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
#include "main/main_single_instance.h"
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
#include "plugin/plugin_data.h"
#include "plugin/plugin_manager.h"
#include "drawing/drawing_image.h"
#include "markdown/markdown_interactive.h"
#include "../resource/resource.h"
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
    if (!hShellWnd) {
        LOG_WARNING("GetShellWindow() returned NULL, Explorer may not be running.");
        return FALSE;
    }

    DWORD dwShellPID = 0;
    GetWindowThreadProcessId(hShellWnd, &dwShellPID);
    if (dwShellPID == 0) {
        LOG_WARNING("Failed to get Explorer PID.");
        return FALSE;
    }

    HANDLE hShellProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwShellPID);
    if (!hShellProcess) {
        LOG_WARNING("Failed to open Explorer process, error: %lu", GetLastError());
        return FALSE;
    }

    HANDLE hShellToken = NULL;
    if (!OpenProcessToken(hShellProcess, TOKEN_DUPLICATE, &hShellToken)) {
        LOG_WARNING("Failed to open Explorer token, error: %lu", GetLastError());
        CloseHandle(hShellProcess);
        return FALSE;
    }

    HANDLE hNewToken = NULL;
    /* Duplicate the shell's token (which is medium integrity/standard user) */
    if (!DuplicateTokenEx(hShellToken, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &hNewToken)) {
        LOG_WARNING("Failed to duplicate token, error: %lu", GetLastError());
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
        LOG_WARNING("Failed to allocate command line buffer.");
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
        /* Verify the new process is actually running */
        DWORD exitCode = 0;
        if (WaitForSingleObject(pi.hProcess, 100) == WAIT_TIMEOUT) {
            /* Process is still running after 100ms - success */
            LOG_INFO("Relaunched self as standard user (PID: %lu)", pi.dwProcessId);
        } else if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            /* Process exited immediately - something went wrong */
            LOG_WARNING("New process exited immediately with code: %lu", exitCode);
            bResult = FALSE;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        LOG_WARNING("CreateProcessWithTokenW failed, error: %lu", GetLastError());
    }

    CloseHandle(hNewToken);
    CloseHandle(hShellToken);
    CloseHandle(hShellProcess);

    return bResult;
}

/* Check if UAC is enabled - returns FALSE if disabled or uncertain */
static BOOL IsUACEnabled(void) {
    HKEY hKey;
    DWORD dwValue = 0;  /* Default to DISABLED for safety - skip privilege drop if uncertain */
    DWORD dwSize = sizeof(DWORD);

    LSTATUS status = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        0, KEY_READ, &hKey);

    if (status != ERROR_SUCCESS) {
        LOG_WARNING("Cannot read UAC registry key (error: %ld), assuming UAC disabled for safety", status);
        return FALSE;
    }

    status = RegQueryValueExW(hKey, L"EnableLUA", NULL, NULL, (LPBYTE)&dwValue, &dwSize);
    RegCloseKey(hKey);

    if (status != ERROR_SUCCESS) {
        LOG_WARNING("Cannot read EnableLUA value (error: %ld), assuming UAC disabled for safety", status);
        return FALSE;
    }

    LOG_INFO("UAC EnableLUA registry value: %lu", dwValue);
    return dwValue != 0;
}

/* Check if Secondary Logon service is running - required for CreateProcessWithTokenW */
static BOOL IsSecondaryLogonServiceRunning(void) {
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) {
        LOG_WARNING("Cannot open SCManager (error: %lu), assuming service unavailable", GetLastError());
        return FALSE;
    }

    SC_HANDLE hService = OpenServiceW(hSCManager, L"seclogon", SERVICE_QUERY_STATUS);
    if (!hService) {
        DWORD err = GetLastError();
        CloseServiceHandle(hSCManager);
        if (err == ERROR_SERVICE_DOES_NOT_EXIST) {
            LOG_WARNING("Secondary Logon service does not exist on this system");
        } else {
            LOG_WARNING("Cannot open Secondary Logon service (error: %lu)", err);
        }
        return FALSE;
    }

    SERVICE_STATUS status;
    BOOL bRunning = FALSE;
    if (QueryServiceStatus(hService, &status)) {
        bRunning = (status.dwCurrentState == SERVICE_RUNNING);
        LOG_INFO("Secondary Logon service state: %lu (running=%s)",
                 status.dwCurrentState, bRunning ? "yes" : "no");
    } else {
        LOG_WARNING("Cannot query Secondary Logon service status (error: %lu)", GetLastError());
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return bRunning;
}

/* Check if running on Windows Server edition */
static BOOL IsWindowsServer(void) {
    OSVERSIONINFOEXW osvi = {sizeof(OSVERSIONINFOEXW)};
    DWORDLONG dwlConditionMask = 0;

    VER_SET_CONDITION(dwlConditionMask, VER_PRODUCT_TYPE, VER_EQUAL);
    osvi.wProductType = VER_NT_WORKSTATION;

    /* If this is NOT a workstation, it's a server */
    BOOL bIsWorkstation = VerifyVersionInfoW(&osvi, VER_PRODUCT_TYPE, dwlConditionMask);

    if (!bIsWorkstation) {
        LOG_INFO("Detected Windows Server edition");
        return TRUE;
    }
    return FALSE;
}

/* Check if Explorer (shell) is running as elevated */
static BOOL IsShellElevated(void) {
    HWND hShellWnd = GetShellWindow();
    if (!hShellWnd) return TRUE;  /* Assume elevated if no shell */

    DWORD dwShellPID = 0;
    GetWindowThreadProcessId(hShellWnd, &dwShellPID);
    if (dwShellPID == 0) return TRUE;

    HANDLE hShellProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwShellPID);
    if (!hShellProcess) return TRUE;

    HANDLE hShellToken = NULL;
    BOOL bShellElevated = TRUE;  /* Default to TRUE (skip privilege drop) */

    if (OpenProcessToken(hShellProcess, TOKEN_QUERY, &hShellToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hShellToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            bShellElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hShellToken);
    }

    CloseHandle(hShellProcess);
    return bShellElevated;
}

/* Drop Administrator privileges if present to ensure Drag & Drop works from Explorer */
static void DropPrivileges(void) {
    /* CRITICAL: This function must NEVER prevent the application from running.
     * All checks are designed to fail-safe (skip privilege drop on any uncertainty).
     */

    if (!IsElevated()) {
        LOG_INFO("Running as standard user (optimal for Drag & Drop).");
        return;
    }

    LOG_INFO("Elevated privileges detected. Checking if privilege drop is safe...");

    /* Check 1: Windows Server - often has different security policies */
    if (IsWindowsServer()) {
        LOG_INFO("Running on Windows Server, skipping privilege drop for compatibility.");
        return;
    }

    /* Check 2: UAC disabled - privilege drop is meaningless and may cause issues */
    if (!IsUACEnabled()) {
        LOG_INFO("UAC is disabled, skipping privilege drop (Drag & Drop may be restricted).");
        return;
    }

    /* Check 3: Secondary Logon service - required for CreateProcessWithTokenW */
    if (!IsSecondaryLogonServiceRunning()) {
        LOG_INFO("Secondary Logon service not running, skipping privilege drop.");
        return;
    }

    /* Check 4: Explorer itself is elevated - privilege drop would be pointless */
    if (IsShellElevated()) {
        LOG_INFO("Explorer is also elevated, skipping privilege drop.");
        return;
    }

    LOG_INFO("All preconditions met. Attempting to switch to standard user...");

    if (RelaunchAsStandardUser()) {
        LOG_INFO("Relaunch successful, exiting elevated instance.");
        /* Exit this elevated instance */
        ExitProcess(0);
    } else {
        LOG_WARNING("Failed to switch to standard user. Continuing with elevated privileges.");
        LOG_WARNING("Drag & Drop from Explorer may be restricted.");
    }
}

extern int elapsed_time;
extern int message_shown;
extern int countdown_message_shown;
extern int countdown_elapsed_time;
extern int countup_elapsed_time;

typedef enum {
    STARTUP_MODE_DEFAULT,
    STARTUP_MODE_COUNT_UP,
    STARTUP_MODE_NO_DISPLAY,
    STARTUP_MODE_SHOW_TIME,
    STARTUP_MODE_POMODORO
} StartupMode;

static StartupMode ParseStartupMode(const char* modeStr) {
    if (!modeStr) return STARTUP_MODE_DEFAULT;
    
    if (strcmp(modeStr, "COUNTDOWN") == 0) return STARTUP_MODE_DEFAULT;
    if (strcmp(modeStr, "DEFAULT") == 0) return STARTUP_MODE_DEFAULT;
    if (strcmp(modeStr, "COUNT_UP") == 0) return STARTUP_MODE_COUNT_UP;
    if (strcmp(modeStr, "NO_DISPLAY") == 0) return STARTUP_MODE_NO_DISPLAY;
    if (strcmp(modeStr, "SHOW_TIME") == 0) return STARTUP_MODE_SHOW_TIME;
    if (strcmp(modeStr, "POMODORO") == 0) return STARTUP_MODE_POMODORO;
    
    return STARTUP_MODE_DEFAULT;
}

void HandleStartupMode(HWND hwnd) {
    StartupMode mode = ParseStartupMode(CLOCK_STARTUP_MODE);
    
    LOG_INFO("Setting startup mode: %s", CLOCK_STARTUP_MODE);
    
    switch (mode) {
        case STARTUP_MODE_COUNT_UP:
            CLOCK_COUNT_UP = TRUE;
            elapsed_time = 0;
            countup_elapsed_time = 0;
            
            extern int64_t g_start_time;
            extern int64_t GetAbsoluteTimeMs(void);
            g_start_time = GetAbsoluteTimeMs();
            break;
            
        case STARTUP_MODE_NO_DISPLAY:
            ShowWindow(hwnd, SW_HIDE);
            KillTimer(hwnd, 1);
            elapsed_time = CLOCK_TOTAL_TIME;
            CLOCK_IS_PAUSED = TRUE;
            
            message_shown = TRUE;
            countdown_message_shown = TRUE;
            countdown_elapsed_time = 0;
            countup_elapsed_time = 0;
            break;
            
        case STARTUP_MODE_SHOW_TIME:
            CLOCK_SHOW_CURRENT_TIME = TRUE;
            CLOCK_LAST_TIME_UPDATE = 0;
            break;
            
        case STARTUP_MODE_POMODORO:
            PostMessage(hwnd, WM_COMMAND, CLOCK_IDM_POMODORO_START, 0);
            break;
            
        case STARTUP_MODE_DEFAULT:
        default:
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            CLOCK_COUNT_UP = FALSE;
            countdown_elapsed_time = 0;
            
            if (CLOCK_TOTAL_TIME <= 0) {
                CLOCK_TOTAL_TIME = g_AppConfig.timer.default_start_time;
            }
            if (CLOCK_TOTAL_TIME <= 0) {
                CLOCK_TOTAL_TIME = 60;
            }
            
            ResetTimer();
            break;
    }
}

BOOL InitializeSubsystems(void) {
    InitCommonControls();
    
    if (!InitializeLogSystem()) {
        /* Log system failed - silently continue without logging capability */
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
        LOG_ERROR("COM initialization failed, error code: 0x%08X. Application cannot continue.", hr);
        return FALSE;
    }
    LOG_INFO("COM initialization successful");

    // Initialize GDI+ for image rendering
    InitDrawingImage();

    // Initialize plugin manager
    PluginManager_Init();
    PluginManager_ScanPlugins();
    LOG_INFO("Plugin manager initialized");

    return TRUE;
}

BOOL InitializeApplicationSubsystem(HINSTANCE hInstance) {
    LOG_INFO("Starting application initialization...");
    
    /* Initialize markdown interactive system */
    InitMarkdownInteractive();
    
    extern BOOL InitializeApplication(HINSTANCE);
    if (!InitializeApplication(hInstance)) {
        LOG_ERROR("Application initialization failed. Check log file for details.");
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
    
    // Initialize Plugin Data subsystem early - needed by CLI handlers and startup mode
    PluginData_Init(hwnd);
    PluginManager_SetNotifyWindow(hwnd);
    
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
        LOG_WINDOWS_ERROR("Timer creation failed");
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

    /* Check if a factory reset was requested during config loading */
    if (g_PerformFactoryReset) {
        LOG_WARNING("SetupMainWindow: Factory reset flag detected. Triggering full reset...");
        /* Post message to ensure it runs after window is fully initialized and visible */
        PostMessage(hwnd, WM_COMMAND, CLOCK_IDM_RESET_ALL, 0);
        
        /* Reset flag */
        g_PerformFactoryReset = FALSE;
    }

    return TRUE;
}

int RunMessageLoop(HWND hwnd) {
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

    LOG_INFO("Cleaning up markdown interactive system");
    CleanupMarkdownInteractive();

    LOG_INFO("Preparing to clean up update check thread resources");
    CleanupUpdateThread();

    LOG_INFO("Shutting down plugin manager");
    PluginManager_Shutdown();

    LOG_INFO("Shutting down plugin data subsystem");
    PluginData_Shutdown();
    
    LOG_INFO("Cleaning up plugin trust resources");
    extern void CleanupPluginTrustCS(void);
    CleanupPluginTrustCS();

    LOG_INFO("Shutting down GDI+");
    ShutdownDrawingImage();

    if (hMutex) {
        LOG_INFO("Releasing mutex before exit");
        ReleaseMutex(hMutex);  /* Release ownership before closing handle */
        CloseHandle(hMutex);
        
        /* Clear global mutex handle to prevent double-free in crash scenarios */
        ClearGlobalMutexHandle();
        
        LOG_INFO("Mutex released and closed successfully");
    }

    CoUninitialize();
    CleanupLogSystem();
}
