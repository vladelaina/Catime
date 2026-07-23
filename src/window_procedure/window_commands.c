/**
 * @file window_commands.c
 * @brief Command dispatch table and range routing
 */

#include "window_commands_internal.h"

typedef struct {
    UINT commandId;
    CommandHandler handler;
} CommandDispatchEntry;

static const CommandDispatchEntry COMMAND_DISPATCH_TABLE[] = {
    {CLOCK_IDM_CUSTOM_COUNTDOWN, CmdCustomCountdown},
    {CLOCK_IDM_EXIT, CmdExit},
    {CLOCK_IDM_RESET_POSITION, CmdResetPosition},
    {CLOCK_IDM_RESET_ALL, CmdResetDefaults},
    {CLOCK_IDM_TIMER_PAUSE_RESUME, CmdPauseResume},
    {CLOCK_IDM_TIMER_RESTART, CmdRestartTimer},
    {CLOCK_IDM_COUNTDOWN_RESET, CmdCountdownReset},
    {CLOCK_IDM_SHOW_CURRENT_TIME, CmdShowCurrentTime},
    {CLOCK_IDM_24HOUR_FORMAT, Cmd24HourFormat},
    {CLOCK_IDM_SHOW_SECONDS, CmdShowSeconds},
    {CLOCK_IDM_COUNT_UP, CmdCountUp},
    {CLOCK_IDM_COUNT_UP_START, CmdCountUpStart},
    {CLOCK_IDM_COUNT_UP_RESET, CmdCountUpReset},
    {CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS, CmdToggleMilliseconds},
    {CLOCK_IDM_POMODORO_START, CmdPomodoroStart},
    {CLOCK_IDM_POMODORO_RESET, CmdPomodoroReset},
    {CLOCK_IDM_POMODORO_LOOP_COUNT, CmdPomodoroLoopCount},
    {CLOCK_IDM_POMODORO_COMBINATION, CmdPomodoroCombo},
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

static BOOL RemoveRecentFileAtIndex(int index) {
    int count = g_AppConfig.recent_files.count;
    if (count < 0) count = 0;
    if (count > MAX_RECENT_FILES) count = MAX_RECENT_FILES;
    if (index < 0 || index >= count) return FALSE;
    RecentFile replacement[MAX_RECENT_FILES];
    ZeroMemory(replacement, sizeof(replacement));
    int newCount = 0;
    for (int i = 0; i < count; i++) {
        if (i != index && newCount < MAX_RECENT_FILES) {
            replacement[newCount++] = g_AppConfig.recent_files.files[i];
        }
    }
    char keys[MAX_RECENT_FILES][32];
    IniKeyValue updates[MAX_RECENT_FILES];
    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        snprintf(keys[i], sizeof(keys[i]), "CLOCK_RECENT_FILE_%d", i + 1);
        updates[i].section = INI_SECTION_RECENTFILES;
        updates[i].key = keys[i];
        updates[i].value = i < newCount ? replacement[i].path : "";
    }
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    if (!WriteIniMultipleAtomic(
            configPath, updates, MAX_RECENT_FILES)) {
        LOG_WARNING(
            "Failed to persist removal of missing recent file at index %d",
            index);
        return FALSE;
    }
    ZeroMemory(g_AppConfig.recent_files.files,
               sizeof(g_AppConfig.recent_files.files));
    for (int i = 0; i < newCount; i++) {
        g_AppConfig.recent_files.files[i] = replacement[i];
    }
    g_AppConfig.recent_files.count = newCount;
    return TRUE;
}

static BOOL HandleRecentFileInternal(HWND hwnd, int index) {
    int count = g_AppConfig.recent_files.count;
    if (count < 0) count = 0;
    if (count > MAX_RECENT_FILES) count = MAX_RECENT_FILES;
    if (index < 0 || index >= count) return FALSE;
    if (!ValidateAndSetTimeoutFile(
            hwnd, g_AppConfig.recent_files.files[index].path) &&
        RemoveRecentFileAtIndex(index)) {
        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
        Timer_ClearTimeoutSystemActionArm();
        CLOCK_TIMEOUT_FILE_PATH[0] = '\0';
    }
    return TRUE;
}

static BOOL HandleColorSelectionInternal(HWND hwnd, UINT command) {
    char color[COLOR_HEX_BUFFER];
    if (!GetColorMenuColorFromId(command, color, sizeof(color))) {
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

BOOL HandleColorSelection(HWND hwnd, UINT command, int index) {
    (void)index;
    return HandleColorSelectionInternal(hwnd, command);
}

BOOL HandleRecentFile(HWND hwnd, UINT command, int index) {
    (void)command;
    return HandleRecentFileInternal(hwnd, index);
}

BOOL HandleFontSelection(HWND hwnd, UINT command, int index) {
    (void)index;
    char fontPath[MAX_PATH];
    if (!GetFontPathFromMenuId(command, fontPath, sizeof(fontPath))) {
        LOG_ERROR("Failed to get font path from menu ID: %u", command);
        return FALSE;
    }
    LOG_INFO("User selected font from menu: %s", fontPath);
    HINSTANCE instance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    if (!SwitchFont(instance, fontPath)) {
        LOG_ERROR("Failed to switch font: %s", fontPath);
        return TRUE;
    }
    LOG_INFO("Font switched successfully: %s", fontPath);
    RefreshCustomTextDisplayDialogFont();
    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

typedef BOOL (*RangeCommandHandler)(HWND hwnd, UINT command, int index);
typedef struct {
    UINT rangeStart;
    UINT rangeEnd;
    RangeCommandHandler handler;
} RangeCommandDescriptor;

BOOL DispatchRangeCommand(HWND hwnd, UINT command, WPARAM wp, LPARAM lp) {
    (void)wp;
    (void)lp;
    if (HandleAnimationMenuCommand(hwnd, command)) return TRUE;
    if (command == CLOCK_IDM_ANIM_SPEED_ORIGINAL) {
        CmdAnimationSpeed(hwnd, ANIMATION_SPEED_ORIGINAL); return TRUE;
    }
    if (command == CLOCK_IDM_ANIM_SPEED_MEMORY) {
        CmdAnimationSpeed(hwnd, ANIMATION_SPEED_MEMORY); return TRUE;
    }
    if (command == CLOCK_IDM_ANIM_SPEED_CPU) {
        CmdAnimationSpeed(hwnd, ANIMATION_SPEED_CPU); return TRUE;
    }
    if (command == CLOCK_IDM_ANIM_SPEED_TIMER) {
        CmdAnimationSpeed(hwnd, ANIMATION_SPEED_TIMER); return TRUE;
    }
    if (command == CLOCK_IDM_ANIM_SPEED_FIXED) {
        CmdAnimationFixedSpeed(hwnd); return TRUE;
    }
    if (command == CLOCK_IDM_TIME_FORMAT_DEFAULT) {
        CmdTimeFormat(hwnd, TIME_FORMAT_DEFAULT); return TRUE;
    }
    if (command == CLOCK_IDM_TIME_FORMAT_ZERO_PADDED) {
        CmdTimeFormat(hwnd, TIME_FORMAT_ZERO_PADDED); return TRUE;
    }
    if (command == CLOCK_IDM_TIME_FORMAT_FULL_PADDED) {
        CmdTimeFormat(hwnd, TIME_FORMAT_FULL_PADDED); return TRUE;
    }
    TextEffectType effect = TextEffect_FromMenuId(command);
    if (effect != TEXT_EFFECT_NONE) {
        ToggleTextEffect(hwnd, effect);
        return TRUE;
    }
    if (command == CLOCK_IDM_TIMEOUT_SHOW_TIME) {
        CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_SHOW_TIME); return TRUE;
    }
    if (command == CLOCK_IDM_TIMEOUT_COUNT_UP) {
        CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_COUNT_UP); return TRUE;
    }
    if (command == CLOCK_IDM_SHOW_MESSAGE) {
        CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_MESSAGE); return TRUE;
    }
    if (command == CLOCK_IDM_LOCK_SCREEN) {
        CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_LOCK); return TRUE;
    }
    if (command == CLOCK_IDM_SHUTDOWN) {
        CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_SHUTDOWN); return TRUE;
    }
    if (command == CLOCK_IDM_RESTART) {
        CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_RESTART); return TRUE;
    }
    if (command == CLOCK_IDM_SLEEP) {
        CmdSetTimeoutAction(hwnd, TIMEOUT_ACTION_SLEEP); return TRUE;
    }
    if (command == CLOCK_IDC_START_SHOW_TIME) {
        CmdSetStartupMode(hwnd, "SHOW_TIME"); return TRUE;
    }
    if (command == CLOCK_IDC_START_COUNT_UP) {
        CmdSetStartupMode(hwnd, "COUNT_UP"); return TRUE;
    }
    if (command == CLOCK_IDC_START_NO_DISPLAY) {
        CmdSetStartupMode(hwnd, "NO_DISPLAY"); return TRUE;
    }
    if (command == CLOCK_IDC_START_POMODORO) {
        CmdSetStartupMode(hwnd, "POMODORO"); return TRUE;
    }
    if (HandlePluginCommand(hwnd, command)) return TRUE;

    const RangeCommandDescriptor ranges[] = {
        {CMD_QUICK_COUNTDOWN_BASE, CMD_QUICK_COUNTDOWN_END,
         HandleQuickCountdown},
        {CLOCK_IDM_QUICK_TIME_BASE,
         CLOCK_IDM_QUICK_TIME_BASE + MAX_TIME_OPTIONS - 1,
         HandleQuickCountdown},
        {CMD_COLOR_OPTIONS_BASE,
         CMD_COLOR_OPTIONS_BASE + MAX_COLOR_OPTIONS - 1,
         HandleColorSelection},
        {CLOCK_IDM_RECENT_FILE_1, CLOCK_IDM_RECENT_FILE_5,
         HandleRecentFile},
        {CMD_POMODORO_TIME_BASE, CMD_POMODORO_TIME_END,
         HandlePomodoroTime},
        {CMD_FONT_SELECTION_BASE,
         CMD_FONT_SELECTION_BASE + FONT_MENU_MAX_ENTRIES - 1,
         HandleFontSelection},
        {0, 0, NULL}
    };
    for (const RangeCommandDescriptor* range = ranges;
         range->handler; range++) {
        if (command >= range->rangeStart && command <= range->rangeEnd) {
            return range->handler(
                hwnd, command, (int)(command - range->rangeStart));
        }
    }
    if (HandleLanguageSelection(hwnd, command)) return TRUE;
    if (command == CLOCK_IDM_POMODORO_WORK ||
        command == CLOCK_IDM_POMODORO_BREAK ||
        command == CLOCK_IDM_POMODORO_LBREAK) {
        int index = command == CLOCK_IDM_POMODORO_WORK ? 0 :
                    command == CLOCK_IDM_POMODORO_BREAK ? 1 : 2;
        return HandlePomodoroTime(hwnd, command, index);
    }
    return FALSE;
}

LRESULT HandleCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    WORD command = LOWORD(wp);
    BOOL animationSelection =
        (command >= CLOCK_IDM_ANIMATIONS_BASE &&
         command < CLOCK_IDM_ANIMATIONS_END) ||
        command == CLOCK_IDM_ANIMATIONS_USE_LOGO ||
        command == CLOCK_IDM_ANIMATIONS_USE_CPU ||
        command == CLOCK_IDM_ANIMATIONS_USE_MEM ||
        command == CLOCK_IDM_ANIMATIONS_USE_BATTERY ||
        command == CLOCK_IDM_ANIMATIONS_USE_CAPSLOCK ||
        command == CLOCK_IDM_ANIMATIONS_USE_NONE;
    StopMenuPreviewTrackingForCommand(hwnd);
    if (!animationSelection) {
        CancelPreview(hwnd);
        RestoreWindowVisibility(hwnd);
    }
    if (DispatchRangeCommand(hwnd, command, wp, lp)) {
        if (animationSelection) MarkAnimationPreviewApplied(hwnd);
        return 0;
    }
    if (animationSelection) CancelPreview(hwnd);
    for (const CommandDispatchEntry* entry = COMMAND_DISPATCH_TABLE;
         entry->handler; entry++) {
        if (entry->commandId == command) {
            return entry->handler(hwnd, wp, lp);
        }
    }
    return 0;
}
