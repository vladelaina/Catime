/**
 * @file window_commands.c
 * @brief Command dispatch table and range routing
 */

#include "window_commands_internal.h"

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
