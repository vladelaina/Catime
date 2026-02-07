/**
 * @file window_commands.c
 * @brief Menu command handlers and dispatch system (core)
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
#include "config/config_applier.h"
#include "log.h"
#include "language.h"
#include "startup.h"
#include "notification.h"
#include "font.h"
#include "color/color.h"
#include "pomodoro.h"
#include "tray/tray.h"

extern void HandleStartupMode(HWND hwnd);
#include "dialog/dialog_procedure.h"
#include "hotkey.h"
#include "update_checker.h"
#include "async_update_checker.h"
#include "window_procedure/window_procedure.h"
#include "window_procedure/window_menus.h"
#include "tray/tray_animation_menu.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_menu_font.h"
#include "menu_preview.h"
#include "dialog/dialog_font_picker.h"
#include "../resource/resource.h"
#include "color/color_parser.h"
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

extern TextEffectType CLOCK_TEXT_EFFECT;

extern wchar_t inputText[256];
extern int time_options[];
extern int time_options_count;
extern size_t COLOR_OPTIONS_COUNT;
extern PredefinedColor* COLOR_OPTIONS;
extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];

/* ============================================================================
 * Simple Command Handlers (kept in core)
 * ============================================================================ */

static LRESULT CmdExit(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    RemoveTrayIcon();
    PostQuitMessage(0);
    return 0;
}

static LRESULT CmdAbout(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowAboutDialog(hwnd);
    return 0;
}

static LRESULT CmdToggleTopmost(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    ToggleTopmost(hwnd);
    return 0;
}

/* Adaptive frame interval based on window size to prevent mouse lag */
static UINT GetAdaptiveAnimationInterval(HWND hwnd) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    int pixels = rect.right * rect.bottom;
    
    /* 
     * Larger windows need longer intervals to avoid blocking DWM.
     * UpdateLayeredWindow is synchronous and blocks until DWM composites.
     * 
     * Holographic effect is significantly heavier (double Gaussian blur + 
     * per-pixel HSV conversion) and needs more aggressive throttling.
     */
    if (CLOCK_HOLOGRAPHIC_EFFECT) {
        if (pixels < 30000) return 50;
        if (pixels < 100000) return 80;
        if (pixels < 300000) return 120;
        return 200;
    }
    
    /* Standard thresholds for other effects */
    if (pixels < 50000) return 33;
    if (pixels < 200000) return 50;
    if (pixels < 500000) return 80;
    return 120;
}

static void UpdateAnimationTimer(HWND hwnd) {
    /*
     * Temporal Decoupling: Animated effects use a dedicated timer
     * to drive visual flow independently of the 1FPS logic clock.
     * Interval is adaptive to window size to prevent mouse lag.
     */
    if (CLOCK_TEXT_EFFECT != TEXT_EFFECT_NONE) {
        UINT interval = GetAdaptiveAnimationInterval(hwnd);
        SetTimer(hwnd, TIMER_ID_RENDER_ANIMATION, interval, NULL);
    } else {
        KillTimer(hwnd, TIMER_ID_RENDER_ANIMATION);
    }
}

/* Convert TextEffectType to config string */
static const char* TextEffectToString(TextEffectType effect) {
    switch (effect) {
        case TEXT_EFFECT_GLOW:        return "GLOW";
        case TEXT_EFFECT_GLASS:       return "GLASS";
        case TEXT_EFFECT_NEON:        return "NEON";
        case TEXT_EFFECT_HOLOGRAPHIC: return "HOLOGRAPHIC";
        case TEXT_EFFECT_LIQUID:      return "LIQUID";
        default:                      return "NONE";
    }
}

/* Toggle effect: if already active, turn off; otherwise switch to it */
static void ToggleTextEffect(HWND hwnd, TextEffectType effect) {
    if (CLOCK_TEXT_EFFECT == effect) {
        CLOCK_TEXT_EFFECT = TEXT_EFFECT_NONE;
    } else {
        CLOCK_TEXT_EFFECT = effect;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    WriteIniString(INI_SECTION_DISPLAY, "TEXT_EFFECT",
                   TextEffectToString(CLOCK_TEXT_EFFECT), config_path);

    UpdateAnimationTimer(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

static LRESULT CmdToggleGlowEffect(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleTextEffect(hwnd, TEXT_EFFECT_GLOW);
    return 0;
}

static LRESULT CmdToggleGlassEffect(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleTextEffect(hwnd, TEXT_EFFECT_GLASS);
    return 0;
}

static LRESULT CmdToggleNeonEffect(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleTextEffect(hwnd, TEXT_EFFECT_NEON);
    return 0;
}

static LRESULT CmdToggleHolographicEffect(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleTextEffect(hwnd, TEXT_EFFECT_HOLOGRAPHIC);
    return 0;
}

static LRESULT CmdToggleLiquidEffect(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleTextEffect(hwnd, TEXT_EFFECT_LIQUID);
    return 0;
}

static LRESULT CmdEditMode(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleEditMode(hwnd);
    return 0;
}

static LRESULT CmdToggleVisibility(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleWindowVisibility(hwnd);
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
    extern void ShowFontLicenseDialog(HWND);
    
    /* Show modeless dialog - result will be handled via WM_DIALOG_FONT_LICENSE */
    ShowFontLicenseDialog(hwnd);
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

static LRESULT CmdSystemFontPicker(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowSystemFontDialog(hwnd);
    return 0;
}

static LRESULT CmdAutoStart(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    BOOL isEnabled = IsAutoStartEnabled();
    if (isEnabled) {
        if (RemoveShortcut()) CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_UNCHECKED);
    } else {
        if (CreateShortcut()) CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_CHECKED);
    }
    return 0;
}

static LRESULT CmdColorDialog(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    /* Use modeless dialog - result handled via WM_DIALOG_COLOR */
    ShowColorInputDialog(hwnd);
    return 0;
}

static LRESULT CmdColorPanel(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (ShowColorDialog(hwnd) != (COLORREF)-1) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
}

static LRESULT CmdAnimationSpeed(HWND hwnd, AnimationSpeedMetric metric) {
    WriteConfigAnimationSpeedMetric(metric);
    InvalidateRect(hwnd, NULL, TRUE);
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
    /* ShowHotkeySettingsDialog is modeless - hotkeys are unregistered inside dialog's WM_INITDIALOG
     * and re-registered when dialog closes (via WM_APP+1 message). Don't call RegisterGlobalHotkeys here. */
    ShowHotkeySettingsDialog(hwnd);
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

static LRESULT CmdBrowseFile(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    char utf8Path[MAX_PATH];
    if (ShowFilePicker(hwnd, utf8Path, sizeof(utf8Path))) {
        ValidateAndSetTimeoutFile(hwnd, utf8Path);
    }
    return 0;
}

static LRESULT CmdResetPosition(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    
    /* Write default values to config */
    char posXStr[32], posYStr[32];
    snprintf(posXStr, sizeof(posXStr), "%d", DEFAULT_WINDOW_POS_X);
    snprintf(posYStr, sizeof(posYStr), "%d", DEFAULT_WINDOW_POS_Y);
    
    WriteConfigKeyValue("CLOCK_WINDOW_POS_X", posXStr);
    WriteConfigKeyValue("CLOCK_WINDOW_POS_Y", posYStr);
    WriteConfigKeyValue("WINDOW_SCALE", DEFAULT_WINDOW_SCALE);
    WriteConfigKeyValue("PLUGIN_SCALE", DEFAULT_PLUGIN_SCALE);
    
    /* Directly apply position (don't rely on hot-reload for special values) */
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int windowWidth = windowRect.right - windowRect.left;
    
    POINT pt = {0, 0};
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfo(hMon, &mi);
    int screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
    
    int posX, posY;
    
    /* Handle special X position values */
    if (DEFAULT_WINDOW_POS_X == -2) {
        posX = mi.rcMonitor.left + (int)(screenWidth * 0.618f) - (windowWidth / 2);
        if (posX + windowWidth > mi.rcMonitor.right) {
            posX = mi.rcMonitor.right - windowWidth - 20;
        }
    } else if (DEFAULT_WINDOW_POS_X == -1) {
        posX = mi.rcMonitor.left + (screenWidth - windowWidth) / 2;
    } else {
        posX = mi.rcMonitor.left + DEFAULT_WINDOW_POS_X;
    }
    
    /* Handle special Y position value (-1 = top of monitor) */
    if (DEFAULT_WINDOW_POS_Y == -1) {
        posY = mi.rcMonitor.top;
    } else if (DEFAULT_WINDOW_POS_Y < 0) {
        posY = mi.rcMonitor.top;
    } else {
        posY = mi.rcMonitor.top + DEFAULT_WINDOW_POS_Y;
    }
    
    CLOCK_WINDOW_POS_X = posX;
    CLOCK_WINDOW_POS_Y = posY;
    SetWindowPos(hwnd, NULL, posX, posY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    return 0;
}

static LRESULT CmdResetDefaults(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    
    LOG_INFO("========== User triggered 'Reset All Settings' operation ==========");
    
    /* Step 1: Clean up active state */
    CleanupBeforeTimerAction();
    KillTimer(hwnd, 1);
    UnregisterGlobalHotkeys(hwnd);
    LOG_INFO("Reset: Active state cleaned (notifications, timers, hotkeys stopped)");
    
    /* Step 2: Disable redraw to prevent flicker */
    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
    
    /* Step 3: Reset runtime timer state (not in config file) */
    ResetTimerStateToDefaults();
    LOG_INFO("Reset: Runtime timer state reset");
    
    /* Step 4: Delete config file and let ReadConfig recreate it */
    ResetConfigurationFile();
    LOG_INFO("Reset: Configuration file deleted");
    
    /* Step 5: Reload all configuration (same as startup) */
    LOG_INFO("Reset: Reloading default configuration...");
    g_ForceApplyConfig = TRUE;  /* Force apply all config values */
    ReadConfig();
    g_ForceApplyConfig = FALSE;
    LOG_INFO("Reset: Configuration reloaded successfully");
    
    /* Step 6: Apply startup mode from config */
    HandleStartupMode(hwnd);
    LOG_INFO("Reset: Startup mode applied: %s", CLOCK_STARTUP_MODE);
    
    /* Step 7: Reset UI runtime state */
    CLOCK_EDIT_MODE = FALSE;
    SetClickThrough(hwnd, TRUE);
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    LOG_INFO("Reset: UI runtime state reset");
    
    /* Step 8: Reload font */
    if (IsFontsFolderPath(FONT_FILE_NAME)) {
        const char* relativePath = ExtractRelativePath(FONT_FILE_NAME);
        if (relativePath) {
            LOG_INFO("Reset: Loading font: %s", FONT_FILE_NAME);
            BOOL fontLoaded = LoadFontByNameAndGetRealName(GetModuleHandle(NULL), relativePath,
                                        FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
            if (fontLoaded) {
                LOG_INFO("Reset: Font loaded successfully: %s", FONT_INTERNAL_NAME);
            } else {
                LOG_WARNING("Reset: Font loading failed: %s", FONT_FILE_NAME);
            }
        }
    }
    
    /* Step 9: Refresh UI to match new config */
    RecalculateWindowSize(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    ResetTimerWithInterval(hwnd);
    
    /* Step 10: Re-enable redraw and refresh display */
    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    
    /* Step 11: Re-register hotkeys with new config */
    RegisterGlobalHotkeys(hwnd);
    
    LOG_INFO("========== Reset All Settings operation completed ==========\n");
    return 0;
}

/* ============================================================================
 * Command Dispatch Table
 * ============================================================================ */

typedef struct {
    UINT cmdId;
    CommandHandler handler;
} CommandDispatchEntry;

static const CommandDispatchEntry COMMAND_DISPATCH_TABLE[] = {
    /* Basic */
    {CLOCK_IDM_CUSTOM_COUNTDOWN, CmdCustomCountdown},
    {CLOCK_IDM_EXIT, CmdExit},
    {CLOCK_IDM_RESET_POSITION, CmdResetPosition},
    {CLOCK_IDM_RESET_ALL, CmdResetDefaults},
    
    /* Timer controls */
    {CLOCK_IDM_TIMER_PAUSE_RESUME, CmdPauseResume},
    {CLOCK_IDM_TIMER_RESTART, CmdRestartTimer},
    {CLOCK_IDM_COUNTDOWN_RESET, CmdCountdownReset},
    {CLOCK_IDM_SHOW_CURRENT_TIME, CmdShowCurrentTime},
    {CLOCK_IDM_24HOUR_FORMAT, Cmd24HourFormat},
    {CLOCK_IDM_SHOW_SECONDS, CmdShowSeconds},
    {CLOCK_IDM_COUNT_UP, CmdCountUp},
    {CLOCK_IDM_COUNT_UP_START, CmdCountUpStart},
    {CLOCK_IDM_COUNT_UP_RESET, CmdCountUpReset},
    
    /* Time format - handled specially below */
    {CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS, CmdToggleMilliseconds},
    
    /* Pomodoro */
    {CLOCK_IDM_POMODORO_START, CmdPomodoroStart},
    {CLOCK_IDM_POMODORO_RESET, CmdPomodoroReset},
    {CLOCK_IDM_POMODORO_LOOP_COUNT, CmdPomodoroLoopCount},
    {CLOCK_IDM_POMODORO_COMBINATION, CmdPomodoroCombo},
    
    /* Settings & options */
    {CLOCK_IDC_MODIFY_TIME_OPTIONS, CmdModifyTimeOptions},
    {CLOCK_IDC_MODIFY_DEFAULT_TIME, CmdModifyDefaultTime},
    {CLOCK_IDC_SET_COUNTDOWN_TIME, CmdSetCountdownTime},
    {CLOCK_IDC_AUTO_START, CmdAutoStart},
    {CLOCK_IDC_EDIT_MODE, CmdEditMode},
    {CLOCK_IDC_TOGGLE_VISIBILITY, CmdToggleVisibility},
    {CLOCK_IDC_CUSTOMIZE_LEFT, CmdCustomizeColor},
    {CLOCK_IDC_FONT_LICENSE_AGREE, CmdFontLicense},
    {CLOCK_IDC_FONT_ADVANCED, CmdFontAdvanced},
    {CLOCK_IDM_SYSTEM_FONT_PICKER, CmdSystemFontPicker},
    {CLOCK_IDC_COLOR_VALUE, CmdColorDialog},
    {CLOCK_IDC_COLOR_PANEL, CmdColorPanel},
    {CLOCK_IDC_TIMEOUT_BROWSE, CmdBrowseFile},
    
    /* Menu items */
    {CLOCK_IDM_ABOUT, CmdAbout},
    {CLOCK_IDM_TOPMOST, CmdToggleTopmost},
    {CLOCK_IDM_GLOW_EFFECT, CmdToggleGlowEffect},
    {CLOCK_IDM_GLASS_EFFECT, CmdToggleGlassEffect},
    {CLOCK_IDM_NEON_EFFECT, CmdToggleNeonEffect},
    {CLOCK_IDM_HOLOGRAPHIC_EFFECT, CmdToggleHolographicEffect},
    {CLOCK_IDM_LIQUID_EFFECT, CmdToggleLiquidEffect},
    {CLOCK_IDM_BROWSE_FILE, CmdBrowseFile},
    {CLOCK_IDM_CHECK_UPDATE, CmdCheckUpdate},
    {CLOCK_IDM_OPEN_WEBSITE, CmdOpenWebsite},
    {CLOCK_IDM_CURRENT_WEBSITE, CmdOpenWebsite},
    {CLOCK_IDM_NOTIFICATION_CONTENT, CmdNotificationContent},
    {CLOCK_IDM_NOTIFICATION_DISPLAY, CmdNotificationDisplay},
    {CLOCK_IDM_NOTIFICATION_SETTINGS, CmdNotificationSettings},
    {CLOCK_IDM_HOTKEY_SETTINGS, CmdHotkeySettings},
    {CLOCK_IDM_HELP, CmdHelp},
    {CLOCK_IDM_SUPPORT, CmdSupport},
    {CLOCK_IDM_FEEDBACK, CmdFeedback},
    
    {0, NULL}
};

/* ============================================================================
 * Range Command Handlers
 * ============================================================================ */

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
        extern TimeoutActionType CLOCK_TIMEOUT_ACTION;
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
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

static BOOL HandleFontSelection(HWND hwnd, UINT cmd, int index) {
    (void)index;
    char foundFontPath[MAX_PATH];
    
    if (GetFontPathFromMenuId(cmd, foundFontPath, sizeof(foundFontPath))) {
        LOG_INFO("User selected font from menu: %s", foundFontPath);
        HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
        if (SwitchFont(hInstance, foundFontPath)) {
            LOG_INFO("Font switched successfully: %s", foundFontPath);
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateWindow(hwnd);
        } else {
            LOG_ERROR("Failed to switch font: %s", foundFontPath);
        }
    } else {
        LOG_ERROR("Failed to get font path from menu ID: %u", cmd);
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

    /* Handle animation commands first */
    if (HandleAnimationMenuCommand(hwnd, cmd)) return TRUE;

    /* Animation speed commands */
    if (cmd == CLOCK_IDM_ANIM_SPEED_ORIGINAL) { CmdAnimationSpeed(hwnd, ANIMATION_SPEED_ORIGINAL); return TRUE; }
    if (cmd == CLOCK_IDM_ANIM_SPEED_MEMORY) { CmdAnimationSpeed(hwnd, ANIMATION_SPEED_MEMORY); return TRUE; }
    if (cmd == CLOCK_IDM_ANIM_SPEED_CPU) { CmdAnimationSpeed(hwnd, ANIMATION_SPEED_CPU); return TRUE; }
    if (cmd == CLOCK_IDM_ANIM_SPEED_TIMER) { CmdAnimationSpeed(hwnd, ANIMATION_SPEED_TIMER); return TRUE; }

    /* Time format commands */
    if (cmd == CLOCK_IDM_TIME_FORMAT_DEFAULT) { CmdTimeFormat(hwnd, TIME_FORMAT_DEFAULT); return TRUE; }
    if (cmd == CLOCK_IDM_TIME_FORMAT_ZERO_PADDED) { CmdTimeFormat(hwnd, TIME_FORMAT_ZERO_PADDED); return TRUE; }
    if (cmd == CLOCK_IDM_TIME_FORMAT_FULL_PADDED) { CmdTimeFormat(hwnd, TIME_FORMAT_FULL_PADDED); return TRUE; }

    /* Timeout action commands */
    if (cmd == CLOCK_IDM_TIMEOUT_SHOW_TIME) { CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_SHOW_TIME); return TRUE; }
    if (cmd == CLOCK_IDM_TIMEOUT_COUNT_UP) { CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_COUNT_UP); return TRUE; }
    if (cmd == CLOCK_IDM_SHOW_MESSAGE) { CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_MESSAGE); return TRUE; }
    if (cmd == CLOCK_IDM_LOCK_SCREEN) { CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_LOCK); return TRUE; }
    if (cmd == CLOCK_IDM_SHUTDOWN) { CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_SHUTDOWN); return TRUE; }
    if (cmd == CLOCK_IDM_RESTART) { CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_RESTART); return TRUE; }
    if (cmd == CLOCK_IDM_SLEEP) { CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_SLEEP); return TRUE; }

    /* Startup mode commands */
    if (cmd == CLOCK_IDC_START_SHOW_TIME) { CmdSetStartupMode(hwnd, "SHOW_TIME"); return TRUE; }
    if (cmd == CLOCK_IDC_START_COUNT_UP) { CmdSetStartupMode(hwnd, "COUNT_UP"); return TRUE; }
    if (cmd == CLOCK_IDC_START_NO_DISPLAY) { CmdSetStartupMode(hwnd, "NO_DISPLAY"); return TRUE; }
    if (cmd == CLOCK_IDC_START_POMODORO) { CmdSetStartupMode(hwnd, "POMODORO"); return TRUE; }

    /* Plugin commands */
    if (HandlePluginCommand(hwnd, cmd)) return TRUE;

    /* Range-based commands */
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

    /* Language selection - Check against generated map */
    if (HandleLanguageSelection(hwnd, cmd)) return TRUE;

    /* Pomodoro phase commands */
    if (cmd == CLOCK_IDM_POMODORO_WORK || cmd == CLOCK_IDM_POMODORO_BREAK ||
        cmd == CLOCK_IDM_POMODORO_LBREAK) {
        int idx = (cmd == CLOCK_IDM_POMODORO_WORK) ? 0 :
                 (cmd == CLOCK_IDM_POMODORO_BREAK) ? 1 : 2;
        return HandlePomodoroTime(hwnd, cmd, idx);
    }

    return FALSE;
}

/* ============================================================================
 * Main Command Handler
 * ============================================================================ */

LRESULT HandleCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    WORD cmd = LOWORD(wp);

    #define IDT_MENU_DEBOUNCE 500
    BOOL isAnimationSelectionCommand =
        (cmd >= CLOCK_IDM_ANIMATIONS_BASE && cmd < CLOCK_IDM_ANIMATIONS_END) ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_LOGO ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_CPU ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_MEM ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_NONE;

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
#define X(Enum, Code, Native, Eng, ConfigKey, ResId, MenuId, ...) {MenuId, Enum},
#include "language_def.h"
    LANGUAGE_LIST
#undef X
};

BOOL HandleLanguageSelection(HWND hwnd, UINT menuId) {
    for (size_t i = 0; i < sizeof(LANGUAGE_MAP) / sizeof(LANGUAGE_MAP[0]); i++) {
        if (menuId == LANGUAGE_MAP[i].menuId) {
            SetLanguage(LANGUAGE_MAP[i].language);
            WriteConfigLanguage(LANGUAGE_MAP[i].language);
            InvalidateRect(hwnd, NULL, TRUE);
            return TRUE;
        }
    }
    return FALSE;
}
