/**
 * @file main.c
 * @brief Main entry with table-driven CLI routing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <dwmapi.h>
#include <winnls.h>
#include <commdlg.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <commctrl.h>
#include <tlhelp32.h>

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
#include "../include/timer_events.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "comctl32.lib")

int default_countdown_time = 0;
int CLOCK_DEFAULT_START_TIME = 300;
int elapsed_time = 0;
wchar_t inputText[256] = {0};
int message_shown = 0;
time_t last_config_time = 0;
RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];
int CLOCK_RECENT_FILES_COUNT = 0;
wchar_t CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH] = L"";

typedef enum {
    STARTUP_MODE_DEFAULT,
    STARTUP_MODE_COUNT_UP,
    STARTUP_MODE_NO_DISPLAY,
    STARTUP_MODE_SHOW_TIME
} StartupMode;

typedef struct {
    const wchar_t* command;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
} CliCommandMapping;

/** Adding new commands only requires table entries */
static const CliCommandMapping SINGLE_CHAR_COMMANDS[] = {
    {L"s", WM_HOTKEY, HOTKEY_ID_SHOW_TIME, 0},
    {L"u", WM_HOTKEY, HOTKEY_ID_COUNT_UP, 0},
    {L"p", WM_HOTKEY, HOTKEY_ID_POMODORO, 0},
    {L"v", WM_HOTKEY, HOTKEY_ID_TOGGLE_VISIBILITY, 0},
    {L"e", WM_HOTKEY, HOTKEY_ID_EDIT_MODE, 0},
    {L"r", WM_HOTKEY, HOTKEY_ID_RESTART_TIMER, 0},
    {L"h", WM_APP_SHOW_CLI_HELP, 0, 0},
};

static const CliCommandMapping QUICK_COUNTDOWN_COMMANDS[] = {
    {L"q1", WM_HOTKEY, HOTKEY_ID_QUICK_COUNTDOWN1, 0},
    {L"q2", WM_HOTKEY, HOTKEY_ID_QUICK_COUNTDOWN2, 0},
    {L"q3", WM_HOTKEY, HOTKEY_ID_QUICK_COUNTDOWN3, 0},
};

/** @return Allocated UTF-8 string (caller must free) */
static char* WideToUtf8(const wchar_t* wideStr) {
    if (!wideStr) return NULL;
    
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, NULL, 0, NULL, NULL);
    if (utf8Len <= 0) return NULL;
    
    char* utf8Str = (char*)malloc(utf8Len);
    if (!utf8Str) return NULL;
    
    WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, utf8Str, utf8Len, NULL, NULL);
    return utf8Str;
}

/** @return Pointer to trimmed string (within original buffer) */
static wchar_t* NormalizeWhitespace(wchar_t* str) {
    if (!str) return NULL;
    
    while (*str && iswspace(*str)) str++;
    
    size_t len = wcslen(str);
    while (len > 0 && iswspace(str[len - 1])) {
        str[--len] = L'\0';
    }
    
    return str;
}

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
            
            /** Prevent notifications when hidden */
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

static BOOL RouteSingleCharCommand(HWND hwnd, wchar_t cmd) {
    wchar_t cmdStr[2] = {cmd, L'\0'};
    
    for (size_t i = 0; i < sizeof(SINGLE_CHAR_COMMANDS) / sizeof(SINGLE_CHAR_COMMANDS[0]); i++) {
        if (wcscmp(cmdStr, SINGLE_CHAR_COMMANDS[i].command) == 0) {
            PostMessage(hwnd, SINGLE_CHAR_COMMANDS[i].message, 
                       SINGLE_CHAR_COMMANDS[i].wParam, 
                       SINGLE_CHAR_COMMANDS[i].lParam);
            return TRUE;
        }
    }
    
    return FALSE;
}

static BOOL RouteTwoCharCommand(HWND hwnd, const wchar_t* cmdStr) {
    if (towlower(cmdStr[0]) == L'p' && towlower(cmdStr[1]) == L'r') {
        PostMessage(hwnd, WM_HOTKEY, HOTKEY_ID_PAUSE_RESUME, 0);
        return TRUE;
    }
    
    for (size_t i = 0; i < sizeof(QUICK_COUNTDOWN_COMMANDS) / sizeof(QUICK_COUNTDOWN_COMMANDS[0]); i++) {
        if (_wcsicmp(cmdStr, QUICK_COUNTDOWN_COMMANDS[i].command) == 0) {
            PostMessage(hwnd, QUICK_COUNTDOWN_COMMANDS[i].message,
                       QUICK_COUNTDOWN_COMMANDS[i].wParam,
                       QUICK_COUNTDOWN_COMMANDS[i].lParam);
            return TRUE;
        }
    }
    
    return FALSE;
}

static BOOL RoutePomodoroCommand(HWND hwnd, const wchar_t* cmdStr) {
    if (towlower(cmdStr[0]) != L'p' || !iswdigit(cmdStr[1])) {
        return FALSE;
    }
    
    wchar_t* endp = NULL;
    long idx = wcstol(cmdStr + 1, &endp, 10);
    
    if (idx > 0 && (endp == NULL || *endp == L'\0')) {
        PostMessage(hwnd, WM_APP_QUICK_COUNTDOWN_INDEX, 0, (LPARAM)idx);
    } else {
        PostMessage(hwnd, WM_HOTKEY, HOTKEY_ID_COUNTDOWN, 0);
    }
    
    return TRUE;
}

static BOOL ForwardTimerInput(HWND hwnd, const wchar_t* cmdStr) {
    char* utf8Str = WideToUtf8(cmdStr);
    if (!utf8Str) return FALSE;
    
    COPYDATASTRUCT cds;
    cds.dwData = COPYDATA_ID_CLI_TEXT;
    cds.cbData = (DWORD)(strlen(utf8Str) + 1);
    cds.lpData = utf8Str;
    
    SendMessage(hwnd, WM_COPYDATA, 0, (LPARAM)&cds);
    free(utf8Str);
    
    return TRUE;
}

static BOOL TryForwardSimpleCliToExisting(HWND hwndExisting, const wchar_t* lpCmdLine) {
    if (!lpCmdLine || lpCmdLine[0] == L'\0') return FALSE;
    
    wchar_t buf[256];
    wcsncpy(buf, lpCmdLine, sizeof(buf)/sizeof(wchar_t) - 1);
    buf[sizeof(buf)/sizeof(wchar_t) - 1] = L'\0';
    
    wchar_t* cmd = NormalizeWhitespace(buf);
    if (!cmd || *cmd == L'\0') return FALSE;
    
    size_t len = wcslen(cmd);
    
    if (len == 1) {
        return RouteSingleCharCommand(hwndExisting, towlower(cmd[0]));
    }
    
    if (len == 2) {
        return RouteTwoCharCommand(hwndExisting, cmd);
    }
    
    if (RoutePomodoroCommand(hwndExisting, cmd)) {
        return TRUE;
    }
    
    for (size_t i = 0; i < len; i++) {
        if (iswdigit(cmd[i])) {
            return ForwardTimerInput(hwndExisting, cmd);
        }
    }
    
    return FALSE;
}

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

static HWND FindExistingInstanceWindow(void) {
    HWND hwnd = FindWindowW(L"CatimeWindow", L"Catime");
    if (hwnd) return hwnd;
    
    return FindInDesktopLayer();
}

static BOOL InitializeSubsystems(void) {
    InitCommonControls();
    
    if (!InitializeLogSystem()) {
        MessageBoxW(NULL, 
                   L"Log system initialization failed, the program will continue running but will not log.", 
                   L"Warning", MB_ICONWARNING);
    }
    
    SetupExceptionHandler();
    LOG_INFO("Catime is starting...");
    
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        LOG_ERROR("COM initialization failed, error code: 0x%08X", hr);
        MessageBoxW(NULL, L"COM initialization failed!", L"Error", MB_ICONERROR);
        return FALSE;
    }
    LOG_INFO("COM initialization successful");
    
    return TRUE;
}

static BOOL InitializeApplicationSubsystem(HINSTANCE hInstance) {
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

/** For package manager installs (WinGet, etc.) */
static void SetupDesktopShortcut(void) {
    LOG_INFO("Checking desktop shortcut...");
    
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    
    char* exe_path_utf8 = WideToUtf8(exe_path);
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

static void InitializeDialogLanguages(void) {
    LOG_INFO("Starting dialog multi-language support initialization...");
    if (!InitDialogLanguageSupport()) {
        LOG_WARNING("Dialog multi-language support initialization failed, but program will continue running");
    } else {
        LOG_INFO("Dialog multi-language support initialization successful");
    }
}

/** @return TRUE to continue, FALSE to exit (command forwarded) */
static BOOL HandleSingleInstance(LPWSTR lpCmdLine, HANDLE* outMutex) {
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
        char* cmdUtf8 = WideToUtf8(lpCmdLineW);
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

static BOOL SetupMainWindow(HINSTANCE hInstance, HWND hwnd, int nCmdShow) {
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
        
        char* cmdUtf8 = WideToUtf8(lpCmdLineW);
        if (cmdUtf8) {
            LOG_INFO("Command line detected: %s", cmdUtf8);
            free(cmdUtf8);
        }
        
        char* cmdCliUtf8 = WideToUtf8(cmdBuf);
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
    
    return TRUE;
}

static int RunMessageLoop(HWND hwnd) {
    LOG_INFO("Entering main message loop");
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        HWND hCliHelp = GetCliHelpDialog();
        if (hCliHelp && IsDialogMessage(hCliHelp, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}

static void CleanupResources(HANDLE hMutex) {
    LOG_INFO("Program preparing to exit, starting resource cleanup");
    
    LOG_INFO("Preparing to clean up update check thread resources");
    CleanupUpdateThread();
    
    if (hMutex) {
        CloseHandle(hMutex);
    }
    
    CoUninitialize();
    CleanupLogSystem();
}

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
