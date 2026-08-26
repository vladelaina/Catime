/**
 * @file tray_menu.c
 * @brief System tray context menus (left/right-click)
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wctype.h>
#include "log.h"
#include "language.h"
#include "tray/tray_menu.h"
#include "font.h"
#include "color/color.h"
#include "window.h"
#include "drag_scale.h"
#include "pomodoro.h"
#include "timer/timer.h"
#include "config.h"
#include "../resource/resource.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_animation_loader.h"
#include "tray/tray_animation_menu.h"
#include "startup.h"
#include "utils/string_convert.h"
#include "utils/natural_sort.h"
#include "utils/string_format.h"
#include "tray/tray_menu_pomodoro.h"
#include "tray/tray_menu_font.h"
#include "tray/tray_menu_submenus.h"
#include "tray/tray_menu_theme.h"
#include "tray/tray_menu_tracking.h"
#include "color/color_parser.h"
#include "taskbar_monitor.h"
#include "tray/tray.h"
#include "window_procedure/window_message_handlers.h"
#include "dialog/dialog_font_picker.h"

/* External dependencies needed for menu display logic */
extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];
extern BOOL CLOCK_EDIT_MODE;

/* Function to format time string (extern from tray_menu_pomodoro.c or similar) */
extern void FormatPomodoroTime(int minutes, wchar_t* buffer, size_t size);

static POINT ResolveTrayMenuAnchor(HWND hwnd, const POINT* anchor) {
    if (anchor && !(anchor->x == -1 && anchor->y == -1)) return *anchor;
    POINT point = {0};
    if (GetCursorPos(&point)) return point;
    RECT windowRect = {0};
    if (GetWindowRect(hwnd, &windowRect)) {
        point.x = windowRect.left;
        point.y = windowRect.bottom;
    }
    return point;
}

/**
 * @brief Build and display right-click configuration menu (Coordinator)
 * @param hwnd Main window handle
 * @note Delegates to specialized submenu builders for maintainability
 */
void ShowColorMenu(HWND hwnd, const POINT* anchor) {
    TrayMenuTrackingState tracking = {0};
    if (!TrayMenuTracking_Begin(hwnd, &tracking)) {
        LOG_WARNING("Tray color menu tracking initialization failed: hwnd=0x%p "
                    "foreground=0x%p", hwnd, GetForegroundWindow());
    }
    ApplyNativeMenuThemeToWindow(hwnd);
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
    
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) {
        LOG_WINDOWS_ERROR("Failed to create tray color menu");
        TrayMenuTracking_End(&tracking);
        return;
    }
    (void)TaskbarMonitor_BeginMenuPreviewSession();
    RefreshTrayBackgroundWorkState();
    PrefetchSystemFontDialogResources();
    
    /* Edit mode toggle */
    AppendMenuW(hMenu, MF_STRING | (CLOCK_EDIT_MODE ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDC_EDIT_MODE, 
               GetLocalizedString(NULL, L"Edit Mode"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    /* Build submenus using modular functions */
    BuildTimeoutActionSubmenu(hMenu);
    BuildPresetManagementSubmenu(hMenu);
    
    /* Hotkey settings */
    AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_HOTKEY_SETTINGS,
                GetLocalizedString(NULL, L"Hotkey Settings"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    BuildFormatSubmenu(hMenu);
    BuildFontSubmenu(hMenu);
    BuildColorSubmenu(hMenu);
    BuildStyleSubmenu(hMenu);

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    BuildPluginsSubmenu(hMenu);

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    BuildAnimationSubmenu(hMenu);

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    BuildHelpSubmenu(hMenu);

    /* Exit */
    AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_EXIT,
                GetLocalizedString(NULL, L"Exit"));
    
    /* Display menu */
    POINT pt = ResolveTrayMenuAnchor(hwnd, anchor);
    if (!TrayMenuTracking_ReassertForeground(&tracking)) {
        LOG_WARNING("Tray color menu foreground acquisition failed: hwnd=0x%p "
                    "foreground=0x%p", hwnd, GetForegroundWindow());
    }
    SetLastError(ERROR_SUCCESS);
    UINT selectedCommand = TrackPopupMenu(
        hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
        pt.x, pt.y, 0, hwnd, NULL);
    DWORD trackError = GetLastError();
    if (selectedCommand == 0 && trackError != ERROR_SUCCESS) {
        LOG_WARNING("Tray color menu tracking failed (error=%lu)",
                    trackError);
    }
    TrayMenuTracking_End(&tracking);
    BOOL isTaskbarMonitorCommand =
        selectedCommand == CLOCK_IDM_TASKBAR_MONITOR_CPU_MEMORY ||
        selectedCommand == CLOCK_IDM_TASKBAR_MONITOR_NETWORK;
    char selectedAnimationName[MAX_PATH] = {0};
    BOOL isAnimationCommand = GetAnimationNameFromMenuId(
        selectedCommand, selectedAnimationName,
        sizeof(selectedAnimationName));
    BOOL commitsActivePreview =
        isTaskbarMonitorCommand || isAnimationCommand;
    /* Commit preview-backed choices before ending the session. Animation
     * previews can then be promoted directly instead of briefly restoring the
     * previous tray icon and loading the selected icon again. */
    if (commitsActivePreview && IsWindow(hwnd)) {
        SendMessageW(hwnd, WM_COMMAND,
                     MAKEWPARAM(selectedCommand, 0), 0);
    }
    FinishMenuPreviewTracking(hwnd);
    TaskbarMonitor_EndMenuPreviewSession();
    RefreshTrayBackgroundWorkState();
    if (selectedCommand != 0 && !commitsActivePreview &&
        IsWindow(hwnd)) {
        SendMessageW(hwnd, WM_COMMAND,
                     MAKEWPARAM(selectedCommand, 0), 0);
    }
    DestroyMenu(hMenu);
}

/**
 * @brief Build and display left-click timer control menu
 * @param hwnd Main window handle
 * @note Includes timer management, Pomodoro, and quick countdown options
 */
void ShowContextMenu(HWND hwnd, const POINT* anchor) {
    TrayMenuTrackingState tracking = {0};
    if (!TrayMenuTracking_Begin(hwnd, &tracking)) {
        LOG_WARNING("Tray context menu tracking initialization failed: hwnd=0x%p "
                    "foreground=0x%p", hwnd, GetForegroundWindow());
    }
    ApplyNativeMenuThemeToWindow(hwnd);
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
    
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) {
        LOG_WINDOWS_ERROR("Failed to create tray context menu");
        TrayMenuTracking_End(&tracking);
        return;
    }
    
    HMENU hTimerManageMenu = CreatePopupMenu();
    if (!hTimerManageMenu) {
        LOG_WINDOWS_ERROR("Failed to create tray timer-management submenu");
        DestroyMenu(hMenu);
        TrayMenuTracking_End(&tracking);
        return;
    }
    
    BOOL timerRunning = (!CLOCK_SHOW_CURRENT_TIME && 
                         (CLOCK_COUNT_UP || 
                          (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0 && countdown_elapsed_time < CLOCK_TOTAL_TIME)));
    
    const wchar_t* pauseResumeText = CLOCK_IS_PAUSED ? 
                                    GetLocalizedString(NULL, L"Resume") : 
                                    GetLocalizedString(NULL, L"Pause");
    
    AppendMenuW(hTimerManageMenu, MF_STRING | (timerRunning ? MF_ENABLED : MF_GRAYED),
               CLOCK_IDM_TIMER_PAUSE_RESUME, pauseResumeText);
    
    BOOL canRestart = (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || CLOCK_TOTAL_TIME > 0));
    
    AppendMenuW(hTimerManageMenu, MF_STRING | (canRestart ? MF_ENABLED : MF_GRAYED),
               CLOCK_IDM_TIMER_RESTART, 
               GetLocalizedString(NULL, L"Start Over"));
    
    const wchar_t* visibilityText = IsWindowVisible(hwnd) ?
        GetLocalizedString(NULL, L"Hide Window") :
        GetLocalizedString(NULL, L"Show Window");
    
    AppendMenuW(hTimerManageMenu, MF_STRING, CLOCK_IDC_TOGGLE_VISIBILITY, visibilityText);
    
    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimerManageMenu,
                     GetLocalizedString(NULL, L"Timer Control"))) {
        DestroyMenu(hTimerManageMenu);
    }
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    HMENU hTimeMenu = CreatePopupMenu();
    if (hTimeMenu) {
        AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_SHOW_CURRENT_TIME ? MF_CHECKED : MF_UNCHECKED),
                   CLOCK_IDM_SHOW_CURRENT_TIME,
                   GetLocalizedString(NULL, L"Show Current Time"));
        
        AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_USE_24HOUR ? MF_CHECKED : MF_UNCHECKED),
                   CLOCK_IDM_24HOUR_FORMAT,
                   GetLocalizedString(NULL, L"24-Hour Format"));
        
        AppendMenuW(hTimeMenu, MF_STRING | (CLOCK_SHOW_SECONDS ? MF_CHECKED : MF_UNCHECKED),
                   CLOCK_IDM_SHOW_SECONDS,
                   GetLocalizedString(NULL, L"Show Seconds"));
        
        if (!AppendMenuW(hMenu, MF_POPUP,
                         (UINT_PTR)hTimeMenu,
                         GetLocalizedString(NULL, L"Time Display"))) {
            DestroyMenu(hTimeMenu);
        }
    }

    /* Build Pomodoro submenu using dedicated module */
    BuildPomodoroMenu(hMenu);

    AppendMenuW(hMenu, MF_STRING | (CLOCK_COUNT_UP ? MF_CHECKED : MF_UNCHECKED),
               CLOCK_IDM_COUNT_UP_START,
               GetLocalizedString(NULL, L"Count Up"));

    AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_CUSTOM_COUNTDOWN, 
                GetLocalizedString(NULL, L"Countdown"));

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    int timeOptionsCount = time_options_count;
    if (timeOptionsCount < 0) timeOptionsCount = 0;
    if (timeOptionsCount > MAX_TIME_OPTIONS) timeOptionsCount = MAX_TIME_OPTIONS;
    for (int i = 0; i < timeOptionsCount; i++) {
        if (time_options[i] <= 0) continue;
        wchar_t menu_item[20];
        FormatPomodoroTime(time_options[i], menu_item, sizeof(menu_item)/sizeof(wchar_t));
        AppendMenuW(hMenu, MF_STRING, CLOCK_IDM_QUICK_TIME_BASE + i, menu_item);
    }

    POINT pt = ResolveTrayMenuAnchor(hwnd, anchor);
    if (!TrayMenuTracking_ReassertForeground(&tracking)) {
        LOG_WARNING("Tray context menu foreground acquisition failed: hwnd=0x%p "
                    "foreground=0x%p", hwnd, GetForegroundWindow());
    }
    SetLastError(ERROR_SUCCESS);
    UINT selectedCommand = TrackPopupMenu(
        hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RETURNCMD,
        pt.x, pt.y, 0, hwnd, NULL);
    DWORD trackError = GetLastError();
    if (selectedCommand == 0 && trackError != ERROR_SUCCESS) {
        LOG_WARNING("Tray context menu tracking failed (error=%lu)",
                    trackError);
    }
    TrayMenuTracking_End(&tracking);
    DestroyMenu(hMenu);
    if (selectedCommand != 0 && IsWindow(hwnd)) {
        SendMessageW(hwnd, WM_COMMAND,
                     MAKEWPARAM(selectedCommand, 0), 0);
    }
}
