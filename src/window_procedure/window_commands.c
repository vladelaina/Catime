/**
 * @file window_commands.c
 * @brief Menu command handlers and dispatch system (core)
 */

#include "window_procedure/window_commands.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_helpers.h"
#include "window_procedure/window_hotkeys.h"
#include "window_procedure/window_message_handlers.h"
#include "timer/timer_events.h"
#include "timer/main_timer.h"
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
#include "font/font_path_manager.h"
#include "color/color.h"
#include "pomodoro.h"
#include "tray/tray.h"
#include "drawing/drawing_effect.h"
#include "text_effect.h"
#include "utils/package_identity.h"
#include "utils/finite_double.h"

extern void HandleStartupMode(HWND hwnd);
#include "dialog/dialog_procedure.h"
#include "hotkey.h"
#include "update_checker.h"
#include "async_update_checker.h"
#include "window_procedure/window_procedure.h"
#include "window_procedure/window_menus.h"
#include "tray/tray_animation_menu.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_animation_speed_input.h"
#include "tray/tray_menu_font.h"
#include "tray/tray_menu_submenus.h"
#include "menu_preview.h"
#include "preview_display.h"
#include "dialog/dialog_font_picker.h"
#include "dialog/dialog_message.h"
#include "../resource/resource.h"
#include "color/color_parser.h"
#include <shlobj.h>
#include <shellapi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

extern TextEffectType CLOCK_TEXT_EFFECT;

extern wchar_t inputText[256];
extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];

static float ParseDefaultScaleOrFallback(const char* value, float fallback) {
    char* end = NULL;
    double parsed = value ? strtod(value, &end) : 0.0;
    if (!value || end == value || !DoubleIsFiniteStrict(parsed) || parsed <= 0.0) {
        return fallback;
    }
    return (float)parsed;
}

/* ============================================================================
 * Simple Command Handlers (kept in core)
 * ============================================================================ */

static LRESULT CmdExit(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    (void)lp;
    /* Route all exits through WM_DESTROY cleanup path to keep shutdown ordering consistent. */
    HideWindowIntentionally(hwnd);
    DestroyWindow(hwnd);
    return 0;
}

static LRESULT CmdCancelTimer(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    
    /* Detener cualquier temporizador en curso */
    MainTimer_Stop();
    CLOCK_SHOW_CURRENT_TIME = true;
    CLOCK_COUNT_UP = false;
    CLOCK_IS_PAUSED = false;
    CLOCK_TOTAL_TIME = 0;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;
    countdown_message_shown = false;
    ResetMillisecondAccumulator();
    
    /* Reiniciar en modo reloj (hora actual) */
    MainTimer_Start(hwnd, GetTimerInterval());
    InvalidateRect(hwnd, NULL, TRUE);
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

/* Toggle effect: if already active, turn off; otherwise switch to it */
static void ToggleTextEffect(HWND hwnd, TextEffectType effect) {
    if (effect != TEXT_EFFECT_NONE && !TextEffect_IsSelectable(effect)) {
        return;
    }

    TextEffectType previousEffect = CLOCK_TEXT_EFFECT;
    TextEffectType newEffect = (previousEffect == effect) ? TEXT_EFFECT_NONE : effect;
    const char* effectConfigValue = TextEffect_ToConfigString(newEffect);

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    if (!WriteIniString(INI_SECTION_DISPLAY, "TEXT_EFFECT",
                        effectConfigValue, config_path)) {
        return;
    }

    CLOCK_TEXT_EFFECT = newEffect;
    g_AppConfig.display.text_effect = newEffect;

    if (TextEffect_UsesSharedEffectBuffer(previousEffect) &&
        !TextEffect_UsesSharedEffectBuffer(newEffect)) {
        CleanupDrawingEffects();
    }

    InvalidateRect(hwnd, NULL, TRUE);
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
    ShowColorDialog(hwnd);
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
    wchar_t wFontsFolderPath[MAX_PATH];
    if (GetFontsFolderW(wFontsFolderPath, MAX_PATH, TRUE)) {
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
    if (IsRunningPackagedApp()) {
        OpenStartupSettings();
        return 0;
    }

    AutoStartStatus status = GetAutoStartStatus();
    if (status == AUTO_START_STATUS_DISABLED_BY_WINDOWS) {
        OpenStartupSettings();
    } else if (status == AUTO_START_STATUS_ENABLED) {
        if (DisableAutoStart()) CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_UNCHECKED);
    } else {
        if (EnableAutoStart()) CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_CHECKED);
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
    AnimationSpeedMetric previousMetric = GetAnimationSpeedMetric();
    if (WriteConfigAnimationSpeedMetric(metric) &&
        previousMetric != GetAnimationSpeedMetric()) {
        TrayAnimation_RecomputeTimerDelay();
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
}

typedef struct {
    double lastMultiplier;
    BOOL hasPreview;
} FixedAnimationSpeedPreviewState;

static void PreviewFixedAnimationSpeed(const wchar_t* text, void* context) {
    FixedAnimationSpeedPreviewState* state =
        (FixedAnimationSpeedPreviewState*)context;
    double multiplier = 0.0;
    if (!state || !TryParseFixedAnimationSpeed(text, &multiplier)) return;

    if (state->hasPreview && fabs(state->lastMultiplier - multiplier) < 0.000001) {
        return;
    }

    SetAnimationSpeedRuntimeState(ANIMATION_SPEED_FIXED, multiplier);
    TrayAnimation_RecomputeTimerDelay();
    state->lastMultiplier = multiplier;
    state->hasPreview = TRUE;
}

static LRESULT CmdAnimationFixedSpeed(HWND hwnd) {
    AnimationSpeedMetric originalMetric = GetAnimationSpeedMetric();
    double originalFixedMultiplier = GetAnimationFixedSpeedMultiplier();
    FixedAnimationSpeedPreviewState previewState = {0};
    wchar_t input[32] = {0};
    _snwprintf_s(input, _countof(input), _TRUNCATE, L"%.10g",
                 originalFixedMultiplier);

    while (InputBoxWithPreview(hwnd,
                               GetLocalizedString(NULL, L"Set Fixed Animation Speed"),
                               GetLocalizedString(NULL, L"Enter a fixed speed from 0.1 to 30 (example: 2):"),
                               input, input, _countof(input),
                               PreviewFixedAnimationSpeed, &previewState)) {
        double multiplier = 0.0;
        if (!TryParseFixedAnimationSpeed(input, &multiplier)) {
            DialogMessage_Show(
                hwnd,
                GetLocalizedString(NULL, L"Invalid input format"),
                GetLocalizedString(
                    NULL,
                    L"Please enter a number from 0.1 to 30 (example: 2)."),
                DIALOG_MESSAGE_WARNING);
            continue;
        }

        if (!WriteConfigAnimationFixedSpeed(multiplier)) {
            SetAnimationSpeedRuntimeState(originalMetric, originalFixedMultiplier);
            TrayAnimation_RecomputeTimerDelay();
            DialogMessage_Show(
                hwnd,
                GetLocalizedString(NULL, L"Error"),
                GetLocalizedString(
                    NULL, L"Failed to save the fixed animation speed."),
                DIALOG_MESSAGE_ERROR);
            return 0;
        }

        TrayAnimation_RecomputeTimerDelay();
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    SetAnimationSpeedRuntimeState(originalMetric, originalFixedMultiplier);
    TrayAnimation_RecomputeTimerDelay();
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

    if (IsRunningPackagedApp()) {
        HINSTANCE result = ShellExecuteW(hwnd, L"open", URL_MICROSOFT_STORE,
                                         NULL, NULL, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32) {
            LOG_WARNING("Failed to open Microsoft Store update page");
        } else {
            LOG_INFO("Opened Microsoft Store update page");
        }
        return 0;
    }

    BOOL started = CheckForUpdateAsync(hwnd, FALSE);
    if (!started) {
        LOG_WARNING("Manual update check was not started");
    }
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

static LRESULT CmdVlaina(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; (void)hwnd;
    extern void OpenVlainaPage(void);
    OpenVlainaPage();
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
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    const IniKeyValue updates[] = {
        {INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", posXStr},
        {INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", posYStr},
        {INI_SECTION_DISPLAY, WINDOW_POSITION_MANUAL_KEY, "FALSE"},
        {INI_SECTION_DISPLAY, WINDOW_MONITOR_ID_KEY, ""},
        {INI_SECTION_DISPLAY, WINDOW_MONITOR_OFFSET_X_KEY, "0"},
        {INI_SECTION_DISPLAY, WINDOW_MONITOR_OFFSET_Y_KEY, "0"},
        {INI_SECTION_DISPLAY, WINDOW_TASKBAR_ANCHORED_KEY, "FALSE"},
        {INI_SECTION_DISPLAY, WINDOW_TASKBAR_AXIS_RATIO_KEY, "0"},
        {INI_SECTION_DISPLAY, WINDOW_TASKBAR_CROSS_OFFSET_KEY, "0"},
        {INI_SECTION_DISPLAY, "WINDOW_SCALE", DEFAULT_WINDOW_SCALE},
        {INI_SECTION_DISPLAY, "PLUGIN_SCALE", DEFAULT_PLUGIN_SCALE},
    };
    if (!WriteIniMultipleAtomic(config_path, updates, sizeof(updates) / sizeof(updates[0]))) {
        LOG_WARNING("Failed to reset window position configuration");
        return 0;
    }
    
    CLOCK_WINDOW_POSITION_MANUAL = FALSE;

    /* Apply position and size together so reset does not visually happen in two steps. */
    float defaultWindowScale = ParseDefaultScaleOrFallback(DEFAULT_WINDOW_SCALE, CLOCK_WINDOW_SCALE);
    float defaultPluginScale = ParseDefaultScaleOrFallback(DEFAULT_PLUGIN_SCALE, PLUGIN_FONT_SCALE_FACTOR);
    int windowWidth = ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_WIDTH, defaultWindowScale);
    int windowHeight = ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_HEIGHT, defaultWindowScale);
    int posX, posY;
    ResolveConfiguredWindowPosition(windowWidth, windowHeight, &posX, &posY);

    CLOCK_WINDOW_SCALE = defaultWindowScale;
    CLOCK_FONT_SCALE_FACTOR = defaultWindowScale;
    PLUGIN_FONT_SCALE_FACTOR = defaultPluginScale;
    CLOCK_WINDOW_POS_X = posX;
    CLOCK_WINDOW_POS_Y = posY;
    SetWindowPos(hwnd, NULL, posX, posY, windowWidth, windowHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(hwnd, NULL, TRUE);

    return 0;
}

static LRESULT CmdResetDefaults(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    
    LOG_INFO("========== User triggered 'Reset All Settings' operation ==========");
    
    /* Step 1: Clean up active state */
    CleanupBeforeTimerAction();
    MainTimer_Stop();
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
    Timer_ClearTimeoutSystemActionArm();
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
    EnsureWindowVisibleWithTopmostState(hwnd);
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
    {CLOCK_IDM_TIMER_CANCEL, CmdCancelTimer},
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
    {CLOCK_IDM_VLAINA, CmdVlaina},
    {CLOCK_IDM_FEEDBACK, CmdFeedback},

    {0, NULL}
};

/* ============================================================================
 * Range Command Handlers
 * ============================================================================ */

static BOOL HandleColorSelection(HWND hwnd, UINT cmd, int index) {
    (void)index;

    char color[COLOR_HEX_BUFFER];
    if (!GetColorMenuColorFromId(cmd, color, sizeof(color))) {
        CancelPreview(hwnd);
        return FALSE;
    }

    if (WriteConfigColor(color)) {
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        CancelPreview(hwnd);
    }
    return TRUE;
}

static BOOL RemoveRecentFileAtIndex(int index) {
    int recentFilesCount = g_AppConfig.recent_files.count;
    if (recentFilesCount < 0) recentFilesCount = 0;
    if (recentFilesCount > MAX_RECENT_FILES) recentFilesCount = MAX_RECENT_FILES;
    if (index < 0 || index >= recentFilesCount) return FALSE;

    RecentFile newRecentFiles[MAX_RECENT_FILES];
    ZeroMemory(newRecentFiles, sizeof(newRecentFiles));

    int newCount = 0;
    for (int i = 0; i < recentFilesCount; i++) {
        if (i == index) {
            continue;
        }
        if (newCount < MAX_RECENT_FILES) {
            newRecentFiles[newCount++] = g_AppConfig.recent_files.files[i];
        }
    }

    char keys[MAX_RECENT_FILES][32];
    IniKeyValue updates[MAX_RECENT_FILES];
    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        snprintf(keys[i], sizeof(keys[i]), "CLOCK_RECENT_FILE_%d", i + 1);
        updates[i].section = INI_SECTION_RECENTFILES;
        updates[i].key = keys[i];
        updates[i].value = (i < newCount) ? newRecentFiles[i].path : "";
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    if (!WriteIniMultipleAtomic(config_path, updates, MAX_RECENT_FILES)) {
        LOG_WARNING("Failed to persist removal of missing recent file at index %d", index);
        return FALSE;
    }

    ZeroMemory(g_AppConfig.recent_files.files, sizeof(g_AppConfig.recent_files.files));
    for (int i = 0; i < newCount; i++) {
        g_AppConfig.recent_files.files[i] = newRecentFiles[i];
    }
    g_AppConfig.recent_files.count = newCount;
    return TRUE;
}

static BOOL HandleRecentFile(HWND hwnd, UINT cmd, int index) {
    (void)cmd;
    int recentFilesCount = g_AppConfig.recent_files.count;
    if (recentFilesCount < 0) recentFilesCount = 0;
    if (recentFilesCount > MAX_RECENT_FILES) recentFilesCount = MAX_RECENT_FILES;
    if (index < 0 || index >= recentFilesCount) return FALSE;

    if (!ValidateAndSetTimeoutFile(hwnd, g_AppConfig.recent_files.files[index].path)) {
        if (RemoveRecentFileAtIndex(index)) {
            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
            Timer_ClearTimeoutSystemActionArm();
            CLOCK_TIMEOUT_FILE_PATH[0] = '\0';
        }
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
            RefreshCustomTextDisplayDialogFont();
            InvalidateRect(hwnd, NULL, TRUE);
        } else {
            LOG_ERROR("Failed to switch font: %s", foundFontPath);
        }
    } else {
        LOG_ERROR("Failed to get font path from menu ID: %u", cmd);
        return FALSE;
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
    if (cmd == CLOCK_IDM_ANIM_SPEED_FIXED) { CmdAnimationFixedSpeed(hwnd); return TRUE; }

    /* Time format commands */
    if (cmd == CLOCK_IDM_TIME_FORMAT_DEFAULT) { CmdTimeFormat(hwnd, TIME_FORMAT_DEFAULT); return TRUE; }
    if (cmd == CLOCK_IDM_TIME_FORMAT_ZERO_PADDED) { CmdTimeFormat(hwnd, TIME_FORMAT_ZERO_PADDED); return TRUE; }
    if (cmd == CLOCK_IDM_TIME_FORMAT_FULL_PADDED) { CmdTimeFormat(hwnd, TIME_FORMAT_FULL_PADDED); return TRUE; }

    TextEffectType textEffect = TextEffect_FromMenuId(cmd);
    if (textEffect != TEXT_EFFECT_NONE) {
        ToggleTextEffect(hwnd, textEffect);
        return TRUE;
    }

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
        {CMD_COLOR_OPTIONS_BASE, CMD_COLOR_OPTIONS_BASE + MAX_COLOR_OPTIONS - 1, HandleColorSelection},
        {CLOCK_IDM_RECENT_FILE_1, CLOCK_IDM_RECENT_FILE_5, HandleRecentFile},
        {CMD_POMODORO_TIME_BASE, CMD_POMODORO_TIME_END, HandlePomodoroTime},
        {CMD_FONT_SELECTION_BASE, CMD_FONT_SELECTION_BASE + FONT_MENU_MAX_ENTRIES - 1, HandleFontSelection},
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

    BOOL isAnimationSelectionCommand =
        (cmd >= CLOCK_IDM_ANIMATIONS_BASE && cmd < CLOCK_IDM_ANIMATIONS_END) ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_LOGO ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_CPU ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_MEM ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_BATTERY ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_CAPSLOCK ||
        cmd == CLOCK_IDM_ANIMATIONS_USE_NONE;

    StopMenuPreviewTrackingForCommand(hwnd);

    if (!isAnimationSelectionCommand) {
        CancelPreview(hwnd);
        RestoreWindowVisibility(hwnd);
    }

    if (DispatchRangeCommand(hwnd, cmd, wp, lp)) {
        if (isAnimationSelectionCommand) {
            MarkAnimationPreviewApplied(hwnd);
        }
        return 0;
    }

    if (isAnimationSelectionCommand) {
        CancelPreview(hwnd);
    }

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
            if (WriteConfigLanguage(LANGUAGE_MAP[i].language) &&
                SetLanguage(LANGUAGE_MAP[i].language)) {
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return TRUE;
        }
    }
    return FALSE;
}
