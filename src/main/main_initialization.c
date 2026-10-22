#include "main/main_initialization.h"
#include "main_initialization_internal.h"
#include "async_update_checker.h"
#include "audio_player.h"
#include "cli.h"
#include "config.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_language.h"
#include "dialog/dialog_notification_audio.h"
#include "drawing/drawing_timer_precision.h"
#include "log.h"
#include "markdown/markdown_interactive.h"
#include "plugin/plugin_data.h"
#include "plugin/plugin_manager.h"
#include "shortcut_checker.h"
#include "timer/main_timer.h"
#include "timer/timer.h"
#include "timer/timer_events.h"
#include "taskbar_monitor.h"
#include "tray/tray_animation_menu.h"
#include "tray/tray_menu_font.h"
#include "tray/tray_menu_theme.h"
#include "utils/package_identity.h"
#include "utils/string_convert.h"
#include "multi_window.h"
#include "window/window_desktop_integration.h"
#include "window/window_initialization.h"
#include "window/window_visual_effects.h"
#include "../../resource/resource.h"

#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUTO_UPDATE_DATE_BUFFER_SIZE 16

typedef enum {
    STARTUP_MODE_DEFAULT,
    STARTUP_MODE_COUNT_UP,
    STARTUP_MODE_NO_DISPLAY,
    STARTUP_MODE_SHOW_TIME,
    STARTUP_MODE_POMODORO
} StartupMode;

static StartupMode ParseStartupMode(const char* mode) {
    if (!mode || strcmp(mode, "COUNTDOWN") == 0 ||
        strcmp(mode, "DEFAULT") == 0) {
        return STARTUP_MODE_DEFAULT;
    }
    if (strcmp(mode, "COUNT_UP") == 0) return STARTUP_MODE_COUNT_UP;
    if (strcmp(mode, "NO_DISPLAY") == 0) return STARTUP_MODE_NO_DISPLAY;
    if (strcmp(mode, "SHOW_TIME") == 0) return STARTUP_MODE_SHOW_TIME;
    if (strcmp(mode, "POMODORO") == 0) return STARTUP_MODE_POMODORO;
    return STARTUP_MODE_DEFAULT;
}

void HandleStartupMode(HWND hwnd) {
    switch (ParseStartupMode(CLOCK_STARTUP_MODE)) {
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
            PostMessageW(hwnd, WM_COMMAND, CLOCK_IDM_POMODORO_START, 0);
            break;
        case STARTUP_MODE_DEFAULT:
        default: {
            CLOCK_SHOW_CURRENT_TIME = false;
            CLOCK_COUNT_UP = false;
            countdown_elapsed_time = 0;
            int startupTime = CLOCK_TOTAL_TIME > 0
                ? CLOCK_TOTAL_TIME : g_AppConfig.timer.default_start_time;
            CLOCK_TOTAL_TIME = startupTime > 0 ? startupTime : 60;
            ResetTimer();
            break;
        }
    }
}

BOOL InitializeSubsystems(void) {
    InitCommonControls();
    (void)InitializeLogSystem();
    SetupExceptionHandler();
    Main_DropPrivileges();
    if (!InitDWMFunctions()) {
        LOG_WARNING("DWM functions unavailable; visual effects limited");
    }
    HRESULT result = CoInitialize(NULL);
    if (FAILED(result)) {
        LOG_ERROR("COM initialization failed: 0x%08X", result);
        ShutdownWindowVisualEffects();
        CleanupLogSystem();
        return FALSE;
    }
    (void)InitializeNativeMenuTheme();
    return TRUE;
}

BOOL InitializeApplicationSubsystem(HINSTANCE hInstance) {
    InitializeAppConfigDefaults();
    InitMarkdownInteractive();
    PluginManager_Init();
    if (!InitializeApplication(hInstance)) {
        LOG_ERROR("Application initialization failed");
        return FALSE;
    }
    LOG_INFO("Application initialization completed");
    return TRUE;
}

void SetupDesktopShortcut(void) {
    int result = CheckAndCreateShortcut();
    if (result != 0) {
        LOG_WARNING("Desktop shortcut creation failed: %d", result);
    }
}

void InitializeDialogLanguages(void) {
    if (!InitDialogLanguageSupport()) {
        LOG_WARNING("Dialog language initialization failed");
    }
}

static void InitializeAsyncCaches(HWND hwnd) {
    PluginData_Init(hwnd);
    PluginManager_SetNotifyWindow(hwnd);
    PluginManager_RequestScanAsync();
    AnimationMenu_Initialize();
    AnimationMenu_RequestScanAsync();
    FontMenu_Initialize();
    FontMenu_RequestScanAsync();
    NotificationSoundCache_Initialize();
    NotificationSoundCache_RequestScanAsync();
}

static BOOL HandleCommandLine(HWND hwnd, BOOL* launchedFromStartup) {
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(
        GetCommandLineW(), &argumentCount);
    if (!arguments) return FALSE;

    wchar_t command[512] = {0};
    size_t used = 0;
    for (int i = 1; i < argumentCount; ++i) {
        const wchar_t* argument = arguments[i];
        if (!argument || !argument[0]) continue;
        if (_wcsicmp(argument, L"--startup") == 0) {
            *launchedFromStartup = TRUE;
            continue;
        }
        if (wcsncmp(argument, L"--", 2) == 0) continue;

        int written = _snwprintf_s(
            command + used, _countof(command) - used, _TRUNCATE,
            used ? L" %s" : L"%s", argument);
        if (written < 0) {
            command[0] = L'\0';
            break;
        }
        used += (size_t)written;
    }
    LocalFree(arguments);
    if (!command[0]) return TRUE;

    char* utf8 = WideToUtf8Alloc(command);
    if (utf8) {
        (void)HandleCliArguments(hwnd, utf8);
        free(utf8);
    }
    return TRUE;
}

static void StartAutomaticUpdateCheck(HWND hwnd) {
    if (IsRunningPackagedApp()) return;
    char today[AUTO_UPDATE_DATE_BUFFER_SIZE] = {0};
    if (!Main_ShouldRunStartupUpdateCheck(today, sizeof(today))) return;
    if (CheckForUpdateAsync(hwnd, TRUE)) {
        Main_MarkStartupUpdateCheckAttempt(today);
    } else {
        LOG_WARNING("Startup update check was not started");
    }
}

BOOL SetupMainWindow(HINSTANCE hInstance, HWND hwnd, int nCmdShow) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(nCmdShow);
    InitializeAsyncCaches(hwnd);
    if (!TaskbarMonitor_Initialize(hInstance, hwnd)) {
        LOG_WARNING("Taskbar monitor initialization failed");
    }
    BOOL launchedFromStartup = FALSE;

    if (!MainTimer_Start(hwnd, GetTimerInterval())) {
        LOG_WINDOWS_ERROR("Timer creation failed");
        return FALSE;
    }
    ResetTimerMilliseconds();
    if (!SetTimer(hwnd, TIMER_ID_FONT_VALIDATION, 2000, NULL)) {
        LOG_WARNING("Font validation timer creation failed");
    }

    if (!IsCiSmokeMode() && !MultiWindow_IsSecondary()) {
        StartAutomaticUpdateCheck(hwnd);
    }
    if (MultiWindow_IsSecondary()) {
        CLOCK_SHOW_CURRENT_TIME = true;
        CLOCK_COUNT_UP = false;
        CLOCK_IS_PAUSED = false;
    } else {
        HandleStartupMode(hwnd);
    }
    HandleCommandLine(hwnd, &launchedFromStartup);
    if (IsCiSmokeMode()) {
        Main_ScheduleCiSmokeExit(hwnd, GetCiExitTimeoutMs());
    }
    if (launchedFromStartup) {
        Main_ScheduleStartupWindowRecovery(hwnd, CLOCK_WINDOW_TOPMOST);
    }
    if (g_PerformFactoryReset) {
        PostMessageW(hwnd, WM_COMMAND, CLOCK_IDM_RESET_ALL, 0);
        g_PerformFactoryReset = FALSE;
    }
    return TRUE;
}

int RunMessageLoop(HWND hwnd) {
    UNREFERENCED_PARAMETER(hwnd);
    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        if (Dialog_ProcessModelessMessage(&message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return (int)message.wParam;
}
