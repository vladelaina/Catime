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
#include "config/config_watcher.h"
#include "language.h"
#include "config/config_plugin_security.h"
#include "timer/timer.h"
#include "timer/main_timer.h"
#include "timer/timer_events.h"
#include "window.h"
#include "window/window_initialization.h"
#include "window/window_desktop_integration.h"
#include "window/window_visual_effects.h"
#include "cli.h"
#include "async_update_checker.h"
#include "update_checker.h"
#include "audio_player.h"
#include "dialog/dialog_language.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_font_picker.h"
#include "dialog/dialog_notification_audio.h"
#include "shortcut_checker.h"
#include "font.h"
#include "utils/string_convert.h"
#include "utils/package_identity.h"
#include "plugin/plugin_data.h"
#include "plugin/plugin_manager.h"
#include "tray/tray_animation_menu.h"
#include "tray/tray_menu_theme.h"
#include "tray/tray_menu_font.h"
#include "drawing/drawing_image.h"
#include "drawing/drawing_effect.h"
#include "drawing/drawing_render.h"
#include "drawing/drawing_timer_precision.h"
#include "markdown/markdown_image.h"
#include "markdown/markdown_interactive.h"
#include "notification.h"
#include "../resource/resource.h"
#include <tlhelp32.h>

#define STARTUP_WINDOW_RECOVERY_DELAY_MS 2000
#define AUTO_UPDATE_LAST_CHECK_DATE_KEY "AUTO_UPDATE_LAST_CHECK_DATE"
#define AUTO_UPDATE_LAST_CHECK_VERSION_KEY "AUTO_UPDATE_LAST_CHECK_VERSION"
#define AUTO_UPDATE_DATE_BUFFER_SIZE 16

static BOOL ContainsFlag(const wchar_t* cmdLine, const wchar_t* flag) {
    const wchar_t* pos;

    if (!cmdLine || !flag || !*flag) {
        return FALSE;
    }

    pos = wcsstr(cmdLine, flag);
    while (pos) {
        const wchar_t before = (pos == cmdLine) ? L' ' : pos[-1];
        const wchar_t after = pos[wcslen(flag)];
        const BOOL beforeOk = before == L' ' || before == L'\t' || before == L'"';
        const BOOL afterOk = after == L'\0' || after == L' ' || after == L'\t' || after == L'"' || after == L'=';

        if (beforeOk && afterOk) {
            return TRUE;
        }
        pos = wcsstr(pos + 1, flag);
    }

    return FALSE;
}

BOOL IsCiSmokeMode(void) {
    return ContainsFlag(GetCommandLineW(), L"--ci-smoke");
}

UINT GetCiExitTimeoutMs(void) {
    const wchar_t* cmdLine = GetCommandLineW();
    const wchar_t* marker = wcsstr(cmdLine, L"--ci-exit-ms=");
    wchar_t* end = NULL;
    unsigned long value;

    if (!marker) {
        return 3000;
    }

    marker += wcslen(L"--ci-exit-ms=");
    value = wcstoul(marker, &end, 10);
    if (end == marker || value < 250 || value > 60000) {
        return 3000;
    }

    return (UINT)value;
}

static VOID CALLBACK CiSmokeExitTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    (void)dwTime;

    if (uMsg != WM_TIMER || idEvent != TIMER_ID_CI_EXIT) {
        return;
    }

    KillTimer(hwnd, idEvent);
    LOG_INFO("CI smoke timeout reached, closing application");
    PostMessage(hwnd, WM_CLOSE, 0, 0);
}

static void ScheduleCiSmokeExit(HWND hwnd, UINT exitDelayMs) {
    if (!SetTimer(hwnd, TIMER_ID_CI_EXIT, exitDelayMs, CiSmokeExitTimerProc)) {
        LOG_WARNING("CI smoke exit timer creation failed (error: %lu); closing immediately",
                    GetLastError());
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
}

static void ScheduleStartupWindowRecovery(HWND hwnd, BOOL topmost) {
    UINT timerId = topmost ? TIMER_ID_TOPMOST_RETRY : TIMER_ID_VISIBILITY_RETRY;
    if (!SetTimer(hwnd, timerId, STARTUP_WINDOW_RECOVERY_DELAY_MS, NULL)) {
        LOG_WARNING("Startup window recovery timer %u creation failed (error: %lu)",
                    timerId, GetLastError());
        EnsureWindowVisibleWithTopmostState(hwnd);
    }
}

static BOOL FormatLocalDate(char* outDate, size_t outDateSize) {
    if (!outDate || outDateSize < AUTO_UPDATE_DATE_BUFFER_SIZE) {
        return FALSE;
    }

    SYSTEMTIME now;
    GetLocalTime(&now);

    return snprintf(outDate, outDateSize, "%04u-%02u-%02u",
                    (unsigned)now.wYear,
                    (unsigned)now.wMonth,
                    (unsigned)now.wDay) > 0;
}

static BOOL ParseConfigDate(const char* dateText, int* year, int* month, int* day) {
    char tail = '\0';
    int parsedYear = 0;
    int parsedMonth = 0;
    int parsedDay = 0;

    if (!dateText || !year || !month || !day) {
        return FALSE;
    }

    if (sscanf(dateText, "%4d-%2d-%2d%c", &parsedYear, &parsedMonth, &parsedDay, &tail) != 3) {
        return FALSE;
    }

    if (parsedYear < 2000 || parsedYear > 9999 ||
        parsedMonth < 1 || parsedMonth > 12 ||
        parsedDay < 1 || parsedDay > 31) {
        return FALSE;
    }

    *year = parsedYear;
    *month = parsedMonth;
    *day = parsedDay;
    return TRUE;
}

static int CompareConfigDate(const char* left, const char* right) {
    int leftYear = 0, leftMonth = 0, leftDay = 0;
    int rightYear = 0, rightMonth = 0, rightDay = 0;

    if (!ParseConfigDate(left, &leftYear, &leftMonth, &leftDay) ||
        !ParseConfigDate(right, &rightYear, &rightMonth, &rightDay)) {
        return 0;
    }

    if (leftYear != rightYear) return leftYear > rightYear ? 1 : -1;
    if (leftMonth != rightMonth) return leftMonth > rightMonth ? 1 : -1;
    if (leftDay != rightDay) return leftDay > rightDay ? 1 : -1;
    return 0;
}

static BOOL ShouldRunStartupUpdateCheck(char* today, size_t todaySize) {
    char configPath[MAX_PATH] = {0};
    char lastDate[AUTO_UPDATE_DATE_BUFFER_SIZE] = {0};
    char lastVersion[64] = {0};

    if (!FormatLocalDate(today, todaySize)) {
        LOG_WARNING("Could not format local date; allowing startup update check");
        return TRUE;
    }

    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0] == '\0') {
        LOG_WARNING("Config path unavailable; allowing startup update check");
        return TRUE;
    }

    ReadIniString(INI_SECTION_GENERAL, AUTO_UPDATE_LAST_CHECK_DATE_KEY, "",
                  lastDate, sizeof(lastDate), configPath);
    ReadIniString(INI_SECTION_GENERAL, AUTO_UPDATE_LAST_CHECK_VERSION_KEY, "",
                  lastVersion, sizeof(lastVersion), configPath);

    int storedYear = 0;
    int storedMonth = 0;
    int storedDay = 0;
    if (!ParseConfigDate(lastDate, &storedYear, &storedMonth, &storedDay)) {
        return TRUE;
    }

    int dateCompare = CompareConfigDate(lastDate, today);
    if (dateCompare > 0) {
        LOG_WARNING("Startup update check date is in the future (%s), ignoring it", lastDate);
        return TRUE;
    }

    if (strcmp(lastVersion, CATIME_VERSION) != 0) {
        return TRUE;
    }

    return dateCompare < 0;
}

static void MarkStartupUpdateCheckAttempt(const char* today) {
    char configPath[MAX_PATH] = {0};
    IniKeyValue updates[] = {
        {INI_SECTION_GENERAL, AUTO_UPDATE_LAST_CHECK_DATE_KEY, today},
        {INI_SECTION_GENERAL, AUTO_UPDATE_LAST_CHECK_VERSION_KEY, CATIME_VERSION}
    };

    if (!today || !*today) {
        return;
    }

    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0] == '\0') {
        LOG_WARNING("Config path unavailable; startup update check attempt was not recorded");
        return;
    }

    if (!WriteIniMultipleAtomic(configPath, updates, sizeof(updates) / sizeof(updates[0]))) {
        LOG_WARNING("Failed to record startup update check attempt");
    }
}

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

    /* Reconstruct command line safely - CreateProcess might modify the buffer */
    const wchar_t* pszOriginalCmdLine = GetCommandLineW();
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

    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
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
    OSVERSIONINFOEXW osvi = {0};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
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
            CLOCK_COUNT_UP = true;
            elapsed_time = 0;
            countup_elapsed_time = 0;
            
            g_start_time = GetAbsoluteTimeMs();
            break;
            
        case STARTUP_MODE_NO_DISPLAY:
            HideWindowIntentionally(hwnd);
            MainTimer_Stop();
            elapsed_time = CLOCK_TOTAL_TIME;
            CLOCK_IS_PAUSED = true;
            
            message_shown = TRUE;
            countdown_message_shown = true;
            countdown_elapsed_time = 0;
            countup_elapsed_time = 0;
            break;
            
        case STARTUP_MODE_SHOW_TIME:
            CLOCK_SHOW_CURRENT_TIME = true;
            CLOCK_LAST_TIME_UPDATE = 0;
            break;
            
        case STARTUP_MODE_POMODORO:
            PostMessage(hwnd, WM_COMMAND, CLOCK_IDM_POMODORO_START, 0);
            break;
            
        case STARTUP_MODE_DEFAULT:
        default:
            CLOCK_SHOW_CURRENT_TIME = false;
            CLOCK_COUNT_UP = false;
            countdown_elapsed_time = 0;
            
            int startupTime = CLOCK_TOTAL_TIME;
            if (startupTime <= 0) {
                startupTime = g_AppConfig.timer.default_start_time;
            }
            if (startupTime <= 0) {
                startupTime = 60;
            }
            CLOCK_TOTAL_TIME = startupTime;
            
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
        ShutdownWindowVisualEffects();
        CleanupLogSystem();
        return FALSE;
    }
    LOG_INFO("COM initialization successful");
    (void)InitializeNativeMenuTheme();

    return TRUE;
}

BOOL InitializeApplicationSubsystem(HINSTANCE hInstance) {
    InitializeAppConfigDefaults();
    LOG_INFO("Starting application initialization...");
    
    /* Initialize markdown interactive system */
    InitMarkdownInteractive();

    // Initialize plugin manager after single-instance ownership is confirmed.
    PluginManager_Init();

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
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(nCmdShow);
    const BOOL ciSmokeMode = IsCiSmokeMode();

    // Initialize Plugin Data subsystem early - needed by CLI handlers and startup mode
    PluginData_Init(hwnd);
    PluginManager_SetNotifyWindow(hwnd);
    PluginManager_RequestScanAsync();
    AnimationMenu_Initialize();
    AnimationMenu_RequestScanAsync();
    FontMenu_Initialize();
    FontMenu_RequestScanAsync();
    NotificationSoundCache_Initialize();
    NotificationSoundCache_RequestScanAsync();
    
    LPWSTR lpCmdLineW = GetCommandLineW();
    if (!lpCmdLineW) {
        lpCmdLineW = L"";
    }
    while (*lpCmdLineW && *lpCmdLineW != L' ') lpCmdLineW++;
    while (*lpCmdLineW == L' ') lpCmdLineW++;
    
    BOOL launchedFromStartup = FALSE;
    wchar_t cmdBuf[512] = {0};
    
    if (lpCmdLineW[0] != L'\0') {
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
    UINT interval = GetTimerInterval();
    
    if (!MainTimer_Start(hwnd, interval)) {
        LOG_WINDOWS_ERROR("Timer creation failed");
        return FALSE;
    }
    LOG_INFO("Timer set successfully with %ums interval", interval);
    
    ResetTimerMilliseconds();
    
    LOG_INFO("Setting font path check timer...");
    if (SetTimer(hwnd, TIMER_ID_FONT_VALIDATION, 2000, NULL) == 0) {
        LOG_WARNING("Font path check timer creation failed, auto-fix will not work");
    } else {
        LOG_INFO("Font path check timer set successfully (2 second interval)");
    }
    
    if (ciSmokeMode) {
        const UINT exitDelayMs = GetCiExitTimeoutMs();
        LOG_INFO("CI smoke mode enabled, skipping startup-only side effects and auto-exiting in %u ms", exitDelayMs);
        ScheduleCiSmokeExit(hwnd, exitDelayMs);
    } else {
        if (IsRunningPackagedApp()) {
            LOG_INFO("Skipping GitHub update check for Microsoft Store/MSIX package");
        } else {
            char startupUpdateDate[AUTO_UPDATE_DATE_BUFFER_SIZE] = {0};
            if (ShouldRunStartupUpdateCheck(startupUpdateDate, sizeof(startupUpdateDate))) {
                LOG_INFO("Starting automatic update check at startup...");
                if (CheckForUpdateAsync(hwnd, TRUE)) {
                    MarkStartupUpdateCheckAttempt(startupUpdateDate);
                } else {
                    LOG_WARNING("Startup automatic update check was not started");
                }
            } else {
                LOG_INFO("Skipping automatic update check at startup; already checked today");
            }
        }

        LOG_INFO("Handling startup mode: %s", CLOCK_STARTUP_MODE);
        HandleStartupMode(hwnd);
    }

    if (launchedFromStartup) {
        ScheduleStartupWindowRecovery(hwnd, CLOCK_WINDOW_TOPMOST);
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
    UNREFERENCED_PARAMETER(hwnd);

    LOG_INFO("Entering main message loop");
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (Dialog_ProcessModelessMessage(&msg)) {
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

    LOG_INFO("Cleaning up cached markdown render state");
    CleanupDrawingRenderCache();

    LOG_INFO("Cleaning up drawing effect buffers");
    CleanupDrawingEffects();

    LOG_INFO("Stopping notification audio playback");
    StopNotificationSound();

    LOG_INFO("Cleaning up notification drawing resources");
    CleanupNotificationResources();

    LOG_INFO("Preparing to clean up update check thread resources");
    CleanupUpdateThreadBlocking();

    LOG_INFO("Releasing update checker network resources");
    CleanupUpdateCheckResources();

    LOG_INFO("Shutting down animation menu cache");
    AnimationMenu_Shutdown();

    LOG_INFO("Shutting down font menu cache");
    FontMenu_Shutdown();

    LOG_INFO("Cleaning up system font picker resources");
    CleanupSystemFontDialogResources();

    LOG_INFO("Unloading active font resource");
    if (!UnloadCurrentFontResource()) {
        LOG_WARNING("Failed to unload font resources during final cleanup");
    }

    LOG_INFO("Shutting down notification sound cache");
    NotificationSoundCache_Shutdown();

    LOG_INFO("Shutting down plugin manager");
    PluginManager_Shutdown();

    LOG_INFO("Shutting down plugin data subsystem");
    PluginData_Shutdown();
    
    LOG_INFO("Cleaning up plugin trust resources");
    CleanupPluginTrustCS();

    LOG_INFO("Shutting down GDI+");
    ShutdownDrawingImage();

    LOG_INFO("Shutting down window visual effects");
    ShutdownWindowVisualEffects();

    LOG_INFO("Stopping config watcher");
    BOOL configWatcherStopped = ConfigWatcher_Shutdown();

    if (configWatcherStopped) {
        LOG_INFO("Shutting down INI cache");
        ShutdownIniCache();
    } else {
        LOG_WARNING("Config watcher did not stop; keeping INI cache alive for late watcher cleanup");
    }

    LOG_INFO("Cleaning up language resources");
    CleanupLanguage();

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
