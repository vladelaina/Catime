#include "window_commands_internal.h"

LRESULT CmdResetPosition(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    StopScaleApplyTimer(hwnd);
    ConsumePendingScaleResizeAnchor(hwnd);
    char posX[32], posY[32];
    snprintf(posX, sizeof(posX), "%d", DEFAULT_WINDOW_POS_X);
    snprintf(posY, sizeof(posY), "%d", DEFAULT_WINDOW_POS_Y);
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    const IniKeyValue updates[] = {
        {INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", posX},
        {INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", posY},
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
    if (!WriteIniMultipleAtomic(
            configPath, updates, sizeof(updates) / sizeof(updates[0]))) {
        LOG_WARNING("Failed to reset window position configuration");
        return 0;
    }
    CancelScheduledConfigSave(hwnd);
    ClearWindowSettingsDirty(WINDOW_SETTINGS_DIRTY_ALL);
    CLOCK_WINDOW_POSITION_MANUAL = FALSE;
    CLOCK_WINDOW_TASKBAR_ANCHORED = FALSE;
    float windowScale = ParseDefaultScaleOrFallback(
        DEFAULT_WINDOW_SCALE, CLOCK_WINDOW_SCALE);
    float pluginScale = ParseDefaultScaleOrFallback(
        DEFAULT_PLUGIN_SCALE, PLUGIN_FONT_SCALE_FACTOR);
    int width = ScaleWindowDimensionClamped(
        CLOCK_BASE_WINDOW_WIDTH, windowScale);
    int height = ScaleWindowDimensionClamped(
        CLOCK_BASE_WINDOW_HEIGHT, windowScale);
    int x, y;
    ResolveConfiguredWindowPosition(width, height, &x, &y);
    CLOCK_WINDOW_SCALE = windowScale;
    CLOCK_FONT_SCALE_FACTOR = windowScale;
    PLUGIN_FONT_SCALE_FACTOR = pluginScale;
    CLOCK_WINDOW_POS_X = x;
    CLOCK_WINDOW_POS_Y = y;
    SetWindowPos(hwnd, NULL, x, y, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

LRESULT CmdResetDefaults(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    StopScaleApplyTimer(hwnd);
    ConsumePendingScaleResizeAnchor(hwnd);
    CancelScheduledConfigSave(hwnd);
    ClearWindowSettingsDirty(WINDOW_SETTINGS_DIRTY_ALL);
    CleanupBeforeTimerAction(hwnd);
    MainTimer_Stop();
    UnregisterGlobalHotkeys(hwnd);
    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
    ResetTimerStateToDefaults();
    ResetConfigurationFile();
    g_ForceApplyConfig = TRUE;
    ReadConfig();
    g_ForceApplyConfig = FALSE;
    ApplyAnimationPathValueNoPersist("__logo__");
    TrayAnimation_RecomputeTimerDelay();
    HandleStartupMode(hwnd);
    CLOCK_EDIT_MODE = FALSE;
    SetClickThrough(hwnd, TRUE);
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
    Timer_ClearTimeoutSystemActionArm();
    if (IsFontsFolderPath(FONT_FILE_NAME)) {
        const char* relativePath = ExtractRelativePath(FONT_FILE_NAME);
        if (relativePath && !LoadFontByNameAndGetRealName(
                GetModuleHandle(NULL), relativePath,
                FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
            LOG_WARNING("Reset: Font loading failed: %s", FONT_FILE_NAME);
        }
    }
    RecalculateWindowSize(hwnd);
    EnsureWindowVisibleWithTopmostState(hwnd);
    ResetTimerWithInterval(hwnd);
    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(hwnd, NULL, NULL,
                 RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    RegisterGlobalHotkeys(hwnd);
    LOG_INFO("All settings reset to defaults");
    return 0;
}
