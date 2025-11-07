/**
 * @file window_commands.c
 * @brief Menu command handlers and dispatch system
 */

#include "window_procedure/window_commands.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_helpers.h"
#include "window_procedure/window_hotkeys.h"
#include "timer/timer_events.h"
#include "tray/tray_events.h"
#include "window_procedure/window_events.h"
#include "drag_scale.h"
#include "timer/timer.h"
#include "window.h"
#include "config.h"
#include "language.h"
#include "startup.h"
#include "notification.h"
#include "font.h"
#include "color/color.h"
#include "pomodoro.h"
#include "tray/tray.h"
#include "dialog/dialog_procedure.h"
#include "hotkey.h"
#include "update_checker.h"
#include "async_update_checker.h"
#include "window_procedure/window_procedure.h"
#include "window_procedure/window_menus.h"
#include "tray/tray_animation_menu.h"
#include "tray/tray_animation_core.h"
#include "menu_preview.h"
#include "../resource/resource.h"
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

extern wchar_t inputText[256];
extern int time_options[];
extern int time_options_count;
extern size_t COLOR_OPTIONS_COUNT;
extern PredefinedColor* COLOR_OPTIONS;
extern char CLOCK_TEXT_COLOR[10];


/* ============================================================================
 * Command Handlers
 * ============================================================================ */

static LRESULT CmdCustomCountdown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (CLOCK_SHOW_CURRENT_TIME) {
        CLOCK_SHOW_CURRENT_TIME = FALSE;
        extern time_t CLOCK_LAST_TIME_UPDATE;
        CLOCK_LAST_TIME_UPDATE = 0;
        KillTimer(hwnd, 1);
    }
    
    int total_seconds = 0;
    if (ValidatedTimeInputLoop(hwnd, CLOCK_IDD_DIALOG1, &total_seconds)) {
        CleanupBeforeTimerAction();
        StartCountdownWithTime(hwnd, total_seconds);
    }
    return 0;
}

static LRESULT CmdExit(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    RemoveTrayIcon();
    PostQuitMessage(0);
    return 0;
}

static LRESULT CmdPauseResume(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    TogglePauseResumeTimer(hwnd);
    return 0;
}

static LRESULT CmdRestartTimer(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CleanupBeforeTimerAction();
    CloseAllNotifications();
    RestartCurrentTimer(hwnd);
    return 0;
}

static LRESULT CmdAbout(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowAboutDialog(hwnd);
    return 0;
}

static LRESULT CmdToggleTopmost(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    BOOL newTopmost = !CLOCK_WINDOW_TOPMOST;
    WriteConfigTopmost(newTopmost ? STR_TRUE : STR_FALSE);
    return 0;
}

static LRESULT CmdTimeFormatDefault(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    WriteConfigTimeFormat(TIME_FORMAT_DEFAULT);
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

static LRESULT CmdTimeFormatZeroPadded(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    WriteConfigTimeFormat(TIME_FORMAT_ZERO_PADDED);
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

static LRESULT CmdTimeFormatFullPadded(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    WriteConfigTimeFormat(TIME_FORMAT_FULL_PADDED);
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

static LRESULT CmdToggleMilliseconds(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    WriteConfigShowMilliseconds(!g_AppConfig.display.time_format.show_milliseconds);
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

static LRESULT CmdCountdownReset(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CleanupBeforeTimerAction();
    if (CLOCK_COUNT_UP) CLOCK_COUNT_UP = FALSE;
    ResetTimer();
    KillTimer(hwnd, 1);
    ResetTimerWithInterval(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
    HandleWindowReset(hwnd);
    return 0;
}

static LRESULT CmdEditMode(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (CLOCK_EDIT_MODE) EndEditMode(hwnd);
    else StartEditMode(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

static LRESULT CmdToggleVisibility(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    PostMessage(hwnd, WM_HOTKEY, HOTKEY_ID_TOGGLE_VISIBILITY, 0);
    return 0;
}

static LRESULT CmdCustomizeColor(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    COLORREF color = ShowColorDialog(hwnd);
    if (color != (COLORREF)-1) {
        char hex_color[10];
        snprintf(hex_color, sizeof(hex_color), "#%02X%02X%02X", 
                GetRValue(color), GetGValue(color), GetBValue(color));
        WriteConfigColor(hex_color);
    }
    return 0;
}

static LRESULT CmdFontLicense(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    
    extern INT_PTR ShowFontLicenseDialog(HWND);
    extern void SetFontLicenseAccepted(BOOL);
    
    if (ShowFontLicenseDialog(hwnd) == IDOK) {
        SetFontLicenseAccepted(TRUE);
        SetFontLicenseVersionAccepted(GetCurrentFontLicenseVersion());
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
}

static LRESULT CmdFontAdvanced(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    
    char configPathUtf8[MAX_PATH];
    GetConfigPath(configPathUtf8, MAX_PATH);
    
    WideString wsConfig = ToWide(configPathUtf8);
    if (!wsConfig.valid) return 0;
    
    wchar_t* lastSep = wcsrchr(wsConfig.buf, L'\\');
    if (lastSep) {
        *lastSep = L'\0';
        wchar_t wFontsFolderPath[MAX_PATH];
        _snwprintf_s(wFontsFolderPath, MAX_PATH, _TRUNCATE, L"%s\\resources\\fonts", wsConfig.buf);
        SHCreateDirectoryExW(NULL, wFontsFolderPath, NULL);
        ShellExecuteW(hwnd, L"open", wFontsFolderPath, NULL, NULL, SW_SHOWNORMAL);
    }
    return 0;
}

static LRESULT CmdShowCurrentTime(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CleanupBeforeTimerAction();
    
    if (!CLOCK_SHOW_CURRENT_TIME) {
        TimerModeParams params = {0, TRUE, FALSE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_SHOW_TIME, &params);
    } else {
        TimerModeParams params = {0, TRUE, FALSE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
    }
    
    // Ensure timer is running after mode switch
    KillTimer(hwnd, 1);
    ResetTimerWithInterval(hwnd);
    
    return 0;
}

static LRESULT Cmd24HourFormat(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleConfigBool(hwnd, CFG_KEY_USE_24HOUR, &CLOCK_USE_24HOUR, TRUE);
    return 0;
}

static LRESULT CmdShowSeconds(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleConfigBool(hwnd, CFG_KEY_SHOW_SECONDS, &CLOCK_SHOW_SECONDS, TRUE);
    return 0;
}

static LRESULT CmdCountUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CleanupBeforeTimerAction();
    
    if (!CLOCK_COUNT_UP) {
        TimerModeParams params = {0, TRUE, FALSE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_COUNTUP, &params);
        
        // Ensure timer is running
        KillTimer(hwnd, 1);
        ResetTimerWithInterval(hwnd);
    } else {
        CLOCK_COUNT_UP = FALSE;
        KillTimer(hwnd, 1);
        ResetTimerWithInterval(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
}

static LRESULT CmdCountUpStart(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CleanupBeforeTimerAction();
    
    if (!CLOCK_COUNT_UP) {
        TimerModeParams params = {0, TRUE, FALSE, TRUE};
        SwitchTimerMode(hwnd, TIMER_MODE_COUNTUP, &params);
        
        // Ensure timer is running
        KillTimer(hwnd, 1);
        ResetTimerWithInterval(hwnd);
    } else {
        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
    }
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

static LRESULT CmdCountUpReset(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CleanupBeforeTimerAction();
    ResetTimer();
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

static LRESULT CmdAutoStart(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    BOOL isEnabled = IsAutoStartEnabled();
    if (isEnabled) {
        if (RemoveShortcut()) {
            CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_UNCHECKED);
        }
    } else {
        if (CreateShortcut()) {
            CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_CHECKED);
        }
    }
    return 0;
}

static LRESULT CmdColorDialog(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_COLOR_DIALOG), 
              hwnd, (DLGPROC)ColorDlgProc);
    return 0;
}

static LRESULT CmdColorPanel(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (ShowColorDialog(hwnd) != (COLORREF)-1) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
}

static LRESULT CmdPomodoroStart(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CleanupBeforeTimerAction();
    
    if (!IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_SHOW);
    
    TimerModeParams params = {g_AppConfig.pomodoro.work_time, TRUE, FALSE, TRUE};
    SwitchTimerMode(hwnd, TIMER_MODE_COUNTDOWN, &params);
    
    extern void InitializePomodoro(void);
    InitializePomodoro();
    extern TimeoutActionType CLOCK_TIMEOUT_ACTION;
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    
    // Ensure timer is running after starting Pomodoro
    KillTimer(hwnd, 1);
    ResetTimerWithInterval(hwnd);
    
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

static LRESULT CmdPomodoroReset(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CleanupBeforeTimerAction();
    
    ResetTimer();
    
    if (CLOCK_TOTAL_TIME == g_AppConfig.pomodoro.work_time || 
        CLOCK_TOTAL_TIME == g_AppConfig.pomodoro.short_break || 
        CLOCK_TOTAL_TIME == g_AppConfig.pomodoro.long_break) {
        KillTimer(hwnd, 1);
        ResetTimerWithInterval(hwnd);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
    HandleWindowReset(hwnd);
    return 0;
}

static LRESULT CmdPomodoroLoopCount(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowPomodoroLoopDialog(hwnd);
    return 0;
}

static LRESULT CmdPomodoroCombo(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowPomodoroComboDialog(hwnd);
    return 0;
}

static LRESULT CmdOpenWebsite(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowWebsiteDialog(hwnd);
    return 0;
}

static LRESULT CmdNotificationContent(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowNotificationMessagesDialog(hwnd);
    return 0;
}

static LRESULT CmdNotificationDisplay(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowNotificationDisplayDialog(hwnd);
    return 0;
}

static LRESULT CmdNotificationSettings(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowNotificationSettingsDialog(hwnd);
    return 0;
}

static LRESULT CmdCheckUpdate(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CheckForUpdateAsync(hwnd, FALSE);
    return 0;
}

static LRESULT CmdHotkeySettings(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowHotkeySettingsDialog(hwnd);
    RegisterGlobalHotkeys(hwnd);
    return 0;
}

static LRESULT CmdHelp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    extern void OpenUserGuide(void);
    OpenUserGuide();
    return 0;
}

static LRESULT CmdSupport(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    extern void OpenSupportPage(void);
    OpenSupportPage();
    return 0;
}

static LRESULT CmdFeedback(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    extern void OpenFeedbackPage(void);
    OpenFeedbackPage();
    return 0;
}

static LRESULT CmdModifyTimeOptions(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    
    while (1) {
        ClearInputBuffer(inputText, sizeof(inputText));
        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_SHORTCUT_DIALOG), 
                       NULL, DlgProc, (LPARAM)CLOCK_IDD_SHORTCUT_DIALOG);
        
        if (isAllSpacesOnly(inputText)) break;
        
        Utf8String us = ToUtf8(inputText);
        char inputTextA[MAX_PATH];
        strcpy_s(inputTextA, sizeof(inputTextA), us.buf);
        
        char* token = strtok(inputTextA, " ");
        char options[256] = {0};
        int valid = 1, count = 0;
        
        while (token && count < MAX_TIME_OPTIONS) {
            int seconds = 0;
            extern BOOL ParseTimeInput(const char*, int*);
            if (!ParseTimeInput(token, &seconds) || seconds <= 0) {
                valid = 0;
                break;
            }
            
            if (count > 0) strcat_s(options, sizeof(options), ",");
            
            char secondsStr[32];
            snprintf(secondsStr, sizeof(secondsStr), "%d", seconds);
            strcat_s(options, sizeof(options), secondsStr);
            count++;
            token = strtok(NULL, " ");
        }
        
        if (valid && count > 0) {
            CleanupBeforeTimerAction();
            WriteConfigTimeOptions(options);
            break;
        } else {
            ShowErrorDialog(hwnd);
        }
    }
    return 0;
}

static LRESULT CmdModifyDefaultTime(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    int total_seconds = 0;
    if (ValidatedTimeInputLoop(hwnd, CLOCK_IDD_STARTUP_DIALOG, &total_seconds)) {
        CleanupBeforeTimerAction();
        WriteConfigDefaultStartTime(total_seconds);
        WriteConfigStartupMode("COUNTDOWN");
    }
    return 0;
}

static inline LRESULT HandleStartupMode(HWND hwnd, const char* mode) {
    SetStartupMode(hwnd, mode);
    return 0;
}

static LRESULT CmdSetCountdownTime(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    int total_seconds = 0;
    if (ValidatedTimeInputLoop(hwnd, CLOCK_IDD_STARTUP_DIALOG, &total_seconds)) {
        WriteConfigDefaultStartTime(total_seconds);
    }
    return HandleStartupMode(hwnd, "COUNTDOWN");
}

static LRESULT CmdStartupShowTime(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    return HandleStartupMode(hwnd, "SHOW_TIME");
}

static LRESULT CmdStartupCountUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    return HandleStartupMode(hwnd, "COUNT_UP");
}

static LRESULT CmdStartupNoDisplay(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    return HandleStartupMode(hwnd, "NO_DISPLAY");
}

static LRESULT CmdTimeoutShowTime(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    WriteConfigTimeoutAction("SHOW_TIME");
    return 0;
}

static LRESULT CmdTimeoutCountUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    WriteConfigTimeoutAction("COUNT_UP");
    return 0;
}

static LRESULT CmdTimeoutShowMessage(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    WriteConfigTimeoutAction("MESSAGE");
    return 0;
}

static LRESULT CmdTimeoutLockScreen(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    WriteConfigTimeoutAction("LOCK");
    return 0;
}

static LRESULT CmdTimeoutShutdown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    WriteConfigTimeoutAction("SHUTDOWN");
    return 0;
}

static LRESULT CmdTimeoutRestart(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    WriteConfigTimeoutAction("RESTART");
    return 0;
}

static LRESULT CmdTimeoutSleep(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    WriteConfigTimeoutAction("SLEEP");
    return 0;
}

static LRESULT CmdBrowseFile(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    char utf8Path[MAX_PATH];
    if (ShowFilePicker(hwnd, utf8Path, sizeof(utf8Path))) {
        ValidateAndSetTimeoutFile(hwnd, utf8Path);
    }
    return 0;
}

static LRESULT CmdResetDefaults(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    
    CleanupBeforeTimerAction();
    KillTimer(hwnd, 1);
    UnregisterGlobalHotkeys(hwnd);
    
    ResetTimerStateToDefaults();
    
    CLOCK_EDIT_MODE = FALSE;
    SetClickThrough(hwnd, TRUE);
    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
    extern char CLOCK_TIMEOUT_FILE_PATH[];
    memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
    
    AppLanguage defaultLanguage = DetectSystemLanguage();
    extern AppLanguage CURRENT_LANGUAGE;
    if (CURRENT_LANGUAGE != defaultLanguage) {
        CURRENT_LANGUAGE = defaultLanguage;
    }
    ResetConfigurationFile();
    ReloadDefaultFont();
    
    InvalidateRect(hwnd, NULL, TRUE);
    RecalculateWindowSize(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    ResetTimerWithInterval(hwnd);
    
    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    RegisterGlobalHotkeys(hwnd);
    
    return 0;
}

/* ============================================================================
 * Command Dispatch Table
 * ============================================================================ */

typedef LRESULT (*CommandHandler)(HWND hwnd, WPARAM wp, LPARAM lp);

typedef struct {
    UINT cmdId;
    CommandHandler handler;
    const char* description;
} CommandDispatchEntry;

static const CommandDispatchEntry COMMAND_DISPATCH_TABLE[] = {
    {101, CmdCustomCountdown, "Custom countdown"},
    {109, CmdExit, "Exit application"},
    {200, CmdResetDefaults, "Reset to defaults"},
    
    {CLOCK_IDC_MODIFY_TIME_OPTIONS, CmdModifyTimeOptions, "Modify time options"},
    {CLOCK_IDC_MODIFY_DEFAULT_TIME, CmdModifyDefaultTime, "Modify default time"},
    {CLOCK_IDC_SET_COUNTDOWN_TIME, CmdSetCountdownTime, "Set countdown time"},
    {CLOCK_IDC_START_SHOW_TIME, CmdStartupShowTime, "Start show time"},
    {CLOCK_IDC_START_COUNT_UP, CmdStartupCountUp, "Start count up"},
    {CLOCK_IDC_START_NO_DISPLAY, CmdStartupNoDisplay, "Start no display"},
    {CLOCK_IDC_AUTO_START, CmdAutoStart, "Auto-start toggle"},
    {CLOCK_IDC_EDIT_MODE, CmdEditMode, "Edit mode toggle"},
    {CLOCK_IDC_TOGGLE_VISIBILITY, CmdToggleVisibility, "Toggle visibility"},
    {CLOCK_IDC_CUSTOMIZE_LEFT, CmdCustomizeColor, "Customize color"},
    {CLOCK_IDC_FONT_LICENSE_AGREE, CmdFontLicense, "Font license agree"},
    {CLOCK_IDC_FONT_ADVANCED, CmdFontAdvanced, "Advanced font selection"},
    {CLOCK_IDC_COLOR_VALUE, CmdColorDialog, "Color value dialog"},
    {CLOCK_IDC_COLOR_PANEL, CmdColorPanel, "Color panel"},
    {CLOCK_IDC_TIMEOUT_BROWSE, CmdBrowseFile, "Browse timeout file"},
    
    {CLOCK_IDM_TIMER_PAUSE_RESUME, CmdPauseResume, "Pause/Resume"},
    {CLOCK_IDM_TIMER_RESTART, CmdRestartTimer, "Restart timer"},
    {CLOCK_IDM_ABOUT, CmdAbout, "About dialog"},
    {CLOCK_IDM_TOPMOST, CmdToggleTopmost, "Toggle topmost"},
    
    {CLOCK_IDM_TIME_FORMAT_DEFAULT, CmdTimeFormatDefault, "Time format default"},
    {CLOCK_IDM_TIME_FORMAT_ZERO_PADDED, CmdTimeFormatZeroPadded, "Time format zero-padded"},
    {CLOCK_IDM_TIME_FORMAT_FULL_PADDED, CmdTimeFormatFullPadded, "Time format full-padded"},
    {CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS, CmdToggleMilliseconds, "Toggle milliseconds"},
    
    {CLOCK_IDM_COUNTDOWN_RESET, CmdCountdownReset, "Countdown reset"},
    {CLOCK_IDM_SHOW_CURRENT_TIME, CmdShowCurrentTime, "Show current time"},
    {CLOCK_IDM_24HOUR_FORMAT, Cmd24HourFormat, "24-hour format"},
    {CLOCK_IDM_SHOW_SECONDS, CmdShowSeconds, "Show seconds"},
    
    {CLOCK_IDM_BROWSE_FILE, CmdBrowseFile, "Browse file"},
    {CLOCK_IDM_COUNT_UP, CmdCountUp, "Count up"},
    {CLOCK_IDM_COUNT_UP_START, CmdCountUpStart, "Count up start"},
    {CLOCK_IDM_COUNT_UP_RESET, CmdCountUpReset, "Count up reset"},
    
    {CLOCK_IDM_POMODORO_START, CmdPomodoroStart, "Pomodoro start"},
    {CLOCK_IDM_POMODORO_RESET, CmdPomodoroReset, "Pomodoro reset"},
    {CLOCK_IDM_POMODORO_LOOP_COUNT, CmdPomodoroLoopCount, "Pomodoro loop count"},
    {CLOCK_IDM_POMODORO_COMBINATION, CmdPomodoroCombo, "Pomodoro combination"},
    
    {CLOCK_IDM_TIMEOUT_SHOW_TIME, CmdTimeoutShowTime, "Timeout show time"},
    {CLOCK_IDM_TIMEOUT_COUNT_UP, CmdTimeoutCountUp, "Timeout count up"},
    {CLOCK_IDM_SHOW_MESSAGE, CmdTimeoutShowMessage, "Show message"},
    {CLOCK_IDM_LOCK_SCREEN, CmdTimeoutLockScreen, "Lock screen"},
    {CLOCK_IDM_SHUTDOWN, CmdTimeoutShutdown, "Shutdown"},
    {CLOCK_IDM_RESTART, CmdTimeoutRestart, "Restart"},
    {CLOCK_IDM_SLEEP, CmdTimeoutSleep, "Sleep"},
    
    {CLOCK_IDM_CHECK_UPDATE, CmdCheckUpdate, "Check update"},
    {CLOCK_IDM_OPEN_WEBSITE, CmdOpenWebsite, "Open website"},
    {CLOCK_IDM_CURRENT_WEBSITE, CmdOpenWebsite, "Current website"},
    
    {CLOCK_IDM_NOTIFICATION_CONTENT, CmdNotificationContent, "Notification content"},
    {CLOCK_IDM_NOTIFICATION_DISPLAY, CmdNotificationDisplay, "Notification display"},
    {CLOCK_IDM_NOTIFICATION_SETTINGS, CmdNotificationSettings, "Notification settings"},
    
    {CLOCK_IDM_HOTKEY_SETTINGS, CmdHotkeySettings, "Hotkey settings"},
    {CLOCK_IDM_HELP, CmdHelp, "Help"},
    {CLOCK_IDM_SUPPORT, CmdSupport, "Support"},
    {CLOCK_IDM_FEEDBACK, CmdFeedback, "Feedback"},
    
    {0, NULL, NULL}
};

/* ============================================================================
 * Range Command Handlers
 * ============================================================================ */

static BOOL HandleQuickCountdown(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    if (index >= 0 && index < time_options_count && time_options[index] > 0) {
        CleanupBeforeTimerAction();
        StartCountdownWithTime(hwnd, time_options[index]);
    }
    return TRUE;
}

static BOOL HandleColorSelection(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    if (index >= 0 && index < (int)COLOR_OPTIONS_COUNT) {
        strncpy_s(CLOCK_TEXT_COLOR, sizeof(CLOCK_TEXT_COLOR), 
                 COLOR_OPTIONS[index].hexColor, _TRUNCATE);
        char config_path[MAX_PATH];
        GetConfigPath(config_path, MAX_PATH);
        WriteConfig(config_path);
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return TRUE;
}

static BOOL HandleRecentFile(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    if (index >= g_AppConfig.recent_files.count) return TRUE;
    
    if (!ValidateAndSetTimeoutFile(hwnd, g_AppConfig.recent_files.files[index].path)) {
        WriteConfigKeyValue("CLOCK_TIMEOUT_FILE", "");
        WriteConfigTimeoutAction("MESSAGE");
        for (int i = index; i < g_AppConfig.recent_files.count - 1; i++) {
            g_AppConfig.recent_files.files[i] = g_AppConfig.recent_files.files[i + 1];
        }
        g_AppConfig.recent_files.count--;
        char config_path[MAX_PATH];
        GetConfigPath(config_path, MAX_PATH);
        WriteConfig(config_path);
    }
    return TRUE;
}

static BOOL HandlePomodoroTime(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    HandlePomodoroTimeConfig(hwnd, index);
    return TRUE;
}

static BOOL HandleFontSelection(HWND hwnd, UINT cmd, int index) {
    (void)index;
    wchar_t fontsFolderRootW[MAX_PATH];
    if (!GetFontsFolderWideFromConfig(fontsFolderRootW, MAX_PATH)) return TRUE;
    
    int currentIndex = CMD_FONT_SELECTION_BASE;
    wchar_t foundRelativePathW[MAX_PATH];
    
    if (FindFontByIdRecursiveW(fontsFolderRootW, cmd, &currentIndex, 
                              foundRelativePathW, fontsFolderRootW)) {
        char foundFontNameUTF8[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, foundRelativePathW, -1, 
                          foundFontNameUTF8, MAX_PATH, NULL, NULL);
        HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
        if (SwitchFont(hInstance, foundFontNameUTF8)) {
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateWindow(hwnd);
        }
    }
    return TRUE;
}

typedef BOOL (*RangeCommandHandler)(HWND hwnd, UINT cmd, int index);

typedef struct {
    UINT rangeStart;
    UINT rangeEnd;
    RangeCommandHandler handler;
} RangeCommandDescriptor;

BOOL DispatchRangeCommand(HWND hwnd, UINT cmd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    
    RangeCommandDescriptor rangeTable[] = {
        {CMD_QUICK_COUNTDOWN_BASE, CMD_QUICK_COUNTDOWN_END, HandleQuickCountdown},
        {CLOCK_IDM_QUICK_TIME_BASE, CLOCK_IDM_QUICK_TIME_BASE + MAX_TIME_OPTIONS - 1, HandleQuickCountdown},
        {CMD_COLOR_OPTIONS_BASE, CMD_COLOR_OPTIONS_BASE + COLOR_OPTIONS_COUNT - 1, HandleColorSelection},
        {CLOCK_IDM_RECENT_FILE_1, CLOCK_IDM_RECENT_FILE_5, HandleRecentFile},
        {CMD_POMODORO_TIME_BASE, CMD_POMODORO_TIME_END, HandlePomodoroTime},
        {CMD_FONT_SELECTION_BASE, CMD_FONT_SELECTION_END - 1, HandleFontSelection},
        {0, 0, NULL}
    };
    
    for (const RangeCommandDescriptor* r = rangeTable; r->handler; r++) {
        if (cmd >= r->rangeStart && cmd <= r->rangeEnd) {
            int index = cmd - r->rangeStart;
            return r->handler(hwnd, cmd, index);
        }
    }
    
    if (cmd >= CLOCK_IDM_LANG_CHINESE && cmd <= CLOCK_IDM_LANG_KOREAN) {
        return HandleLanguageSelection(hwnd, cmd);
    }
    if (cmd == CLOCK_IDM_POMODORO_WORK || cmd == CLOCK_IDM_POMODORO_BREAK || 
        cmd == CLOCK_IDM_POMODORO_LBREAK) {
        int idx = (cmd == CLOCK_IDM_POMODORO_WORK) ? 0 : 
                 (cmd == CLOCK_IDM_POMODORO_BREAK) ? 1 : 2;
        return HandlePomodoroTime(hwnd, cmd, idx);
    }
    if (HandleAnimationMenuCommand(hwnd, cmd)) return TRUE;
    
    return FALSE;
}

/* ============================================================================
 * Main Command Handler
 * ============================================================================ */

LRESULT HandleCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    WORD cmd = LOWORD(wp);
    
    #define IDT_MENU_DEBOUNCE 500
    BOOL isAnimationSelectionCommand = 
        (cmd >= CLOCK_IDM_ANIMATIONS_BASE && cmd < CLOCK_IDM_ANIMATIONS_BASE + 1000) ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_LOGO ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_CPU ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_MEM;
    
    if (isAnimationSelectionCommand) {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
    } else {
        CancelAnimationPreview();
    }
    
    if (DispatchRangeCommand(hwnd, cmd, wp, lp)) return 0;
    
    for (const CommandDispatchEntry* entry = COMMAND_DISPATCH_TABLE; entry->handler; entry++) {
        if (entry->cmdId == cmd) return entry->handler(hwnd, wp, lp);
    }
    
    return 0;
}

/* ============================================================================
 * Language Selection
 * ============================================================================ */

static const struct {
    UINT menuId;
    AppLanguage language;
} LANGUAGE_MAP[] = {
    {CLOCK_IDM_LANG_CHINESE, APP_LANG_CHINESE_SIMP},
    {CLOCK_IDM_LANG_CHINESE_TRAD, APP_LANG_CHINESE_TRAD},
    {CLOCK_IDM_LANG_ENGLISH, APP_LANG_ENGLISH},
    {CLOCK_IDM_LANG_SPANISH, APP_LANG_SPANISH},
    {CLOCK_IDM_LANG_FRENCH, APP_LANG_FRENCH},
    {CLOCK_IDM_LANG_GERMAN, APP_LANG_GERMAN},
    {CLOCK_IDM_LANG_RUSSIAN, APP_LANG_RUSSIAN},
    {CLOCK_IDM_LANG_PORTUGUESE, APP_LANG_PORTUGUESE},
    {CLOCK_IDM_LANG_JAPANESE, APP_LANG_JAPANESE},
    {CLOCK_IDM_LANG_KOREAN, APP_LANG_KOREAN}
};

BOOL HandleLanguageSelection(HWND hwnd, UINT menuId) {
    for (size_t i = 0; i < sizeof(LANGUAGE_MAP) / sizeof(LANGUAGE_MAP[0]); i++) {
        if (menuId == LANGUAGE_MAP[i].menuId) {
            SetLanguage(LANGUAGE_MAP[i].language);
            WriteConfigLanguage(LANGUAGE_MAP[i].language);
            InvalidateRect(hwnd, NULL, TRUE);
            extern void UpdateTrayIcon(HWND);
            UpdateTrayIcon(hwnd);
            return TRUE;
        }
    }
    
    return FALSE;
}

/* ============================================================================
 * Pomodoro Time Configuration
 * ============================================================================ */

BOOL HandlePomodoroTimeConfig(HWND hwnd, int selectedIndex) {
    if (selectedIndex < 0 || selectedIndex >= g_AppConfig.pomodoro.times_count) {
        return FALSE;
    }
    
    memset(inputText, 0, sizeof(inputText));
    DialogBoxParamW(GetModuleHandle(NULL), 
             MAKEINTRESOURCEW(CLOCK_IDD_POMODORO_TIME_DIALOG),
             hwnd, DlgProc, (LPARAM)CLOCK_IDD_POMODORO_TIME_DIALOG);
    
    if (inputText[0] && !isAllSpacesOnly(inputText)) {
        int total_seconds = 0;

        char inputTextA[256];
        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
        extern int ParseInput(const char*, int*);
        if (ParseInput(inputTextA, &total_seconds)) {
            g_AppConfig.pomodoro.times[selectedIndex] = total_seconds;
            
            WriteConfigPomodoroTimeOptions(g_AppConfig.pomodoro.times, g_AppConfig.pomodoro.times_count);
            
            if (selectedIndex == 0) g_AppConfig.pomodoro.work_time = total_seconds;
            else if (selectedIndex == 1) g_AppConfig.pomodoro.short_break = total_seconds;
            else if (selectedIndex == 2) g_AppConfig.pomodoro.long_break = total_seconds;
            
            return TRUE;
        }
    }
    
    return FALSE;
}

