/**
 * @file window_message_system.c
 * @brief Handles system settings, theme, and session shutdown messages.
 */

#include "window_procedure/window_message_handlers_internal.h"
#include "dialog/dialog_common.h"
#include "drag_scale.h"
#include "tray/tray_menu_theme.h"
#include "taskbar_monitor.h"
#include "window.h"

static BOOL g_sessionSettingsPrepared = FALSE;

static BOOL IsThemeSettingChange(WPARAM wp, LPARAM lp) {
    if (wp == SPI_SETHIGHCONTRAST) return TRUE;
    if (!lp) return FALSE;

    static const wchar_t* settingNames[] = {
        L"ImmersiveColorSet",
        L"WindowsThemeElement",
        L"AppsUseLightTheme",
        L"SystemUsesLightTheme",
        L"HighContrast"
    };
    const wchar_t* settingName = (const wchar_t*)lp;
    for (size_t i = 0; i < _countof(settingNames); i++) {
        if (_wcsicmp(settingName, settingNames[i]) == 0) return TRUE;
    }
    return FALSE;
}

LRESULT HandleSettingChange(HWND hwnd, WPARAM wp, LPARAM lp) {
    /* Theme-related setting names vary across Windows releases and Insider
     * builds. Refreshing for every settings notification is inexpensive, and
     * the popup path still performs a final refresh before showing a menu. */
    RefreshNativeMenuTheme();
    if (IsThemeSettingChange(wp, lp)) {
        Dialog_RefreshOpenThemes();
        TaskbarMonitor_Refresh();
    }

    if (wp != SPI_SETWORKAREA || CLOCK_IS_DRAGGING) {
        return DefWindowProc(hwnd, WM_SETTINGCHANGE, wp, lp);
    }

    if (!BeginSystemPositionChangeGuard(hwnd)) {
        RestoreWindowPositionAfterSystemChange(hwnd);
    }
    return DefWindowProc(hwnd, WM_SETTINGCHANGE, wp, lp);
}

LRESULT HandleThemeChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    RefreshNativeMenuTheme();
    Dialog_RefreshOpenThemes();
    TaskbarMonitor_Refresh();
    return DefWindowProc(hwnd, WM_THEMECHANGED, wp, lp);
}

LRESULT HandleQueryEndSession(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    (void)lp;
    CancelScheduledConfigSave(hwnd);
    if (CLOCK_EDIT_MODE) {
        EndEditMode(hwnd);
    }
    g_sessionSettingsPrepared = SaveWindowSettings(hwnd);
    return TRUE;
}

LRESULT HandleEndSession(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp && !g_sessionSettingsPrepared) {
        CancelScheduledConfigSave(hwnd);
        if (CLOCK_EDIT_MODE) {
            EndEditMode(hwnd);
        }
        SaveWindowSettings(hwnd);
    }
    g_sessionSettingsPrepared = FALSE;
    return 0;
}
