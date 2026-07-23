/** @file window_hotkeys.c @brief Global hotkey dispatch. */

#include "window_procedure/window_hotkeys.h"
#include "window_procedure/window_utils.h"
#include "timer/timer_events.h"
#include "tray/tray_events.h"
#include "window.h"
#include "window_procedure/window_procedure.h"
#include "dialog/dialog_procedure.h"
#include "notification.h"
#include "audio_player.h"
#include "config.h"
#include "hotkey.h"
#include "../resource/resource.h"

extern wchar_t inputText[256];
extern HWND g_hwndInputDialog;

typedef void (*HotkeyAction)(HWND);

static void HotkeyCustomCountdown(HWND hwnd) {
    if (g_hwndInputDialog && IsWindow(g_hwndInputDialog)) {
        SetForegroundWindow(g_hwndInputDialog);
        return;
    }
    ClearInputBuffer(inputText, sizeof(inputText));
    ShowCountdownInputDialog(hwnd);
}

static void HotkeyPauseResume(HWND hwnd) { TogglePauseResumeTimer(hwnd); }

typedef struct {
    int id;
    HotkeyAction action;
    int quickCountdownIndex;
} HotkeyDescriptor;

static const HotkeyDescriptor HOTKEY_DISPATCH_TABLE[] = {
    {HOTKEY_ID_SHOW_TIME, ToggleShowTimeMode, 0},
    {HOTKEY_ID_COUNT_UP, StartCountUp, 0},
    {HOTKEY_ID_COUNTDOWN, StartDefaultCountDown, 0},
    {HOTKEY_ID_QUICK_COUNTDOWN1, NULL, 1},
    {HOTKEY_ID_QUICK_COUNTDOWN2, NULL, 2},
    {HOTKEY_ID_QUICK_COUNTDOWN3, NULL, 3},
    {HOTKEY_ID_POMODORO, StartPomodoroTimer, 0},
    {HOTKEY_ID_TOGGLE_VISIBILITY, ToggleWindowVisibility, 0},
    {HOTKEY_ID_EDIT_MODE, ToggleEditMode, 0},
    {HOTKEY_ID_PAUSE_RESUME, HotkeyPauseResume, 0},
    {HOTKEY_ID_RESTART_TIMER, RestartCurrentTimer, 0},
    {HOTKEY_ID_CUSTOM_COUNTDOWN, HotkeyCustomCountdown, 0},
    {HOTKEY_ID_TOGGLE_MILLISECONDS, ToggleMilliseconds, 0},
    {HOTKEY_ID_TOPMOST, ToggleTopmost, 0}
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

BOOL DispatchHotkey(HWND hwnd, int hotkeyId) {
    for (size_t i = 0; i < ARRAY_SIZE(HOTKEY_DISPATCH_TABLE); ++i) {
        const HotkeyDescriptor* descriptor = &HOTKEY_DISPATCH_TABLE[i];
        if (descriptor->id != hotkeyId) continue;

        StopNotificationSound();
        if (descriptor->quickCountdownIndex > 0) {
            StartQuickCountdownByIndex(hwnd, descriptor->quickCountdownIndex);
        } else if (descriptor->action) {
            descriptor->action(hwnd);
        }
        return TRUE;
    }
    return FALSE;
}
