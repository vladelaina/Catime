/**
 * @file window_command_dispatch.c
 * @brief Top-level command dispatch and preview lifecycle handling.
 */

#include "window_commands_internal.h"
#include "window_procedure/window_preview_policy.h"

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
    {CLOCK_IDM_RESUME_POMODORO, CmdResumePomodoro},
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

static BOOL IsAnimationSelection(WORD command) {
    return (command >= CLOCK_IDM_ANIMATIONS_BASE &&
            command < CLOCK_IDM_ANIMATIONS_END) ||
           command == CLOCK_IDM_ANIMATIONS_USE_LOGO ||
           command == CLOCK_IDM_ANIMATIONS_USE_CPU ||
           command == CLOCK_IDM_ANIMATIONS_USE_MEM ||
           command == CLOCK_IDM_ANIMATIONS_USE_BATTERY ||
           command == CLOCK_IDM_ANIMATIONS_USE_CAPSLOCK ||
           command == CLOCK_IDM_ANIMATIONS_USE_NONE;
}

LRESULT HandleCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    WORD command = LOWORD(wp);
    BOOL animationSelection = IsAnimationSelection(command);
    BOOL keepFontPickerPreview = WindowPreview_ShouldKeepForCommand(
        command, GetActivePreviewSource());
    StopMenuPreviewTrackingForCommand(hwnd);
    BOOL previewCommand =
        !animationSelection && DispatchMenuPreview(hwnd, command);
    if (previewCommand) {
        if (!ApplyPreview(hwnd)) CancelPreview(hwnd);
        RestoreWindowVisibility(hwnd);
        return 0;
    }
    if (!animationSelection && !keepFontPickerPreview) {
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
