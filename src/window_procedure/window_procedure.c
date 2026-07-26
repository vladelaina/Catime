#include "window_procedure/window_procedure.h"
#include "taskbar_monitor.h"
#include "window_procedure/window_message_handlers.h"
#include "window_procedure/window_commands.h"
#include "window_procedure/window_config_handlers.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_helpers.h"
#include "tray/tray_events.h"
#include "tray/tray_animation_core.h"
#include "tray/tray.h"
#include "config.h"
#include "timer/timer.h"
#include "timer/timer_events.h"
#include "timer/main_timer.h"
#include "audio_player.h"
#include "window.h"
#include "pomodoro.h"
#include "notification.h"
#include "drawing.h"
#include "../resource/resource.h"
#include "log.h"
#include <string.h>
#include <windowsx.h>
#include "window_procedure/window_drop_target.h"
#include "window_procedure/window_events.h"
#include "color/color_parser.h"
#include "plugin/plugin_manager.h"
#include "plugin/plugin_data.h"
#include "markdown/markdown_interactive.h"
#include "drag_scale.h" // Added this line
extern UINT WM_TASKBARCREATED;
extern BOOL PREVIOUS_TOPMOST_STATE;
#define OPACITY_FULL 255
static LRESULT HandlePowerBroadcast(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    static volatile LONG s_handling = 0;
    if (wp == PBT_APMSUSPEND) {
        Timer_OnSystemSuspend();
        return TRUE;
    }
    if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND || wp == PBT_APMRESUMECRITICAL) {
        if (InterlockedCompareExchange(&s_handling, 1, 0) != 0) {
            return TRUE;
        }
        Timer_OnSystemResume();
        LOG_INFO("System resumed from sleep/hibernate, reinitializing tray icon animation");
        TrayAnimation_ClearCurrentName();
        HandleAppAnimPathChanged(hwnd);
        RecreateTaskbarIcon(hwnd, GetModuleHandle(NULL));
        if (!BeginSystemPositionChangeGuard(hwnd)) {
            RestoreWindowPositionAfterSystemChange(hwnd);
        }
        InterlockedExchange(&s_handling, 0);
    }
    return TRUE;
}
typedef LRESULT (*AppMessageHandler)(HWND hwnd);
typedef struct {
    UINT msgId;
    AppMessageHandler handler;
} AppMessageDispatchEntry;
static const AppMessageDispatchEntry APP_MESSAGE_DISPATCH_TABLE[] = {
    {WM_APP_CONFIG_CHANGED, HandleAppConfigChanged},
    {WM_APP_DISPLAY_CHANGED, HandleAppDisplayChanged},
    {WM_APP_TIMER_CHANGED, HandleAppTimerChanged},
    {WM_APP_POMODORO_CHANGED, HandleAppPomodoroChanged},
    {WM_APP_NOTIFICATION_CHANGED, HandleAppNotificationChanged},
    {WM_APP_HOTKEYS_CHANGED, HandleAppHotkeysChanged},
    {WM_APP_RECENTFILES_CHANGED, HandleAppRecentFilesChanged},
    {WM_APP_COLORS_CHANGED, HandleAppColorsChanged},
    {WM_APP_ANIM_SPEED_CHANGED, HandleAppAnimSpeedChanged},
    {WM_APP_ANIM_PATH_CHANGED, HandleAppAnimPathChanged},
    {0,                             NULL}
};
static inline BOOL DispatchAppMessage(HWND hwnd, UINT msg) {
    for (const AppMessageDispatchEntry* entry = APP_MESSAGE_DISPATCH_TABLE; entry->handler; entry++) {
        if (entry->msgId == msg) {
            entry->handler(hwnd);
            return TRUE;
        }
    }
    return FALSE;
}
static LRESULT HandlePluginExitMessage(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    HandlePluginExit(hwnd);
    return 0;
}
static LRESULT HandlePluginNotifyMessage(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    PluginData_ProcessPendingNotification(hwnd);
    return 0;
}
typedef LRESULT (*MessageHandler)(HWND hwnd, WPARAM wp, LPARAM lp);
typedef struct {
    UINT msg;
    MessageHandler handler;
} MessageDispatchEntry;
static const MessageDispatchEntry MESSAGE_DISPATCH_TABLE[] = {
    {WM_CREATE, HandleCreate},
    {WM_SETCURSOR, HandleSetCursor},
    {WM_LBUTTONDOWN, HandleLButtonDown},
    {WM_LBUTTONUP, HandleLButtonUp},
    {WM_LBUTTONDBLCLK, HandleLButtonDblClk},
    {WM_RBUTTONDOWN, HandleRButtonDown},
    {WM_RBUTTONUP, HandleRButtonUp},
    {WM_CONTEXTMENU, HandleContextMenu},
    {WM_CAPTURECHANGED, HandleCaptureChanged},
    {WM_CANCELMODE, HandleCancelMode},
    {WM_MOUSEWHEEL, HandleMouseWheel},
    {WM_MOUSEMOVE, HandleMouseMove},
    {WM_PAINT, HandlePaint},
    {WM_ERASEBKGND, HandleEraseBkgnd},
    {WM_TIMER, HandleTimer},
    {WM_DESTROY, HandleDestroy},
    {CLOCK_WM_TRAYICON, HandleTrayIcon},
    {WM_COMMAND, HandleCommand},
    {WM_WINDOWPOSCHANGED, HandleWindowPosChanged},
    {WM_SHOWWINDOW, HandleShowWindow},
    {WM_DISPLAYCHANGE, HandleDisplayChange},
    {WM_DPICHANGED, HandleDpiChanged},
    {WM_SETTINGCHANGE, HandleSettingChange},
    {WM_THEMECHANGED, HandleThemeChanged},
    {WM_INITMENUPOPUP, HandleInitMenuPopup},
    {WM_MENUSELECT, HandleMenuSelect},
    {WM_MEASUREITEM, HandleMeasureItem},
    {WM_DRAWITEM, HandleDrawItem},
    {WM_EXITMENULOOP, HandleExitMenuLoop},
    {WM_SYSCOMMAND, HandleSysCommand},
    {WM_SIZE, HandleSize},
    {WM_CLOSE, HandleClose},
    {WM_QUERYENDSESSION, HandleQueryEndSession},
    {WM_ENDSESSION, HandleEndSession},
    {WM_KEYDOWN, HandleKeyDown},
    {WM_HOTKEY, HandleHotkey},
    {WM_COPYDATA, HandleCopyData},
    {WM_POWERBROADCAST, HandlePowerBroadcast},
    {WM_APP_QUICK_COUNTDOWN_INDEX, HandleQuickCountdownIndex},
    {WM_APP_SHOW_CLI_HELP, HandleShowCliHelp},
    {WM_USER + 100, HandleTrayUpdateIcon},
    {WM_APP + 1, HandleAppReregisterHotkeys},
    {CLOCK_WM_ANIMATION_PREVIEW_LOADED, HandleAnimationPreviewLoaded},
    {CLOCK_WM_PLUGIN_EXIT, HandlePluginExitMessage},
    {CLOCK_WM_MAIN_TIMER_TICK, HandleMainTimerTick},
    {WM_DIALOG_COUNTDOWN, HandleDialogCountdown},
    {WM_DIALOG_SHORTCUT, HandleDialogShortcut},
    {WM_DIALOG_COLOR, HandleDialogColor},
    {WM_DIALOG_UPDATE, HandleDialogUpdate},
    {WM_UPDATE_CHECK_RESULT, HandleUpdateCheckResult},
    {WM_DIALOG_FONT_LICENSE, HandleDialogFontLicense},
    {WM_DIALOG_PLUGIN_SECURITY, HandleDialogPluginSecurity},
    {WM_PLUGIN_HOT_RELOAD, HandlePluginHotReload},
    {WM_PLUGIN_NOTIFY, HandlePluginNotifyMessage},
    {0, NULL}
};
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_TASKBARCREATED) {
        RecreateTaskbarIcon(hwnd, GetModuleHandle(NULL));
        TaskbarMonitor_OnTaskbarCreated();
        RefreshWindowTopmostState(hwnd);
        if (!BeginSystemPositionChangeGuard(hwnd)) {
            RestoreWindowPositionAfterSystemChange(hwnd);
        }
        return 0;
    }
    if (msg == CLOCK_WM_TRAY_OPACITY_WHEEL) {
        HandleTrayOpacityWheel(hwnd, (int)wp, (BOOL)lp);
        return 0;
    }
    if (msg == CLOCK_WM_PLUGIN_DATA_REDRAW) {
        PluginData_HandleRedrawRequest(hwnd);
        return 0;
    }
    if (msg == WM_MOUSEACTIVATE) {
        if (!CLOCK_EDIT_MODE && !CLOCK_WINDOW_TOPMOST) {
            return MA_NOACTIVATE;  /* Don't activate window on click */
        }
    }
    if (msg == WM_NCHITTEST) {
        if (IsEditExitRightClickShieldActive()) {
            return HTCLIENT;
        }
        if (!CLOCK_EDIT_MODE) {
            if (!HasClickableRegions()) {
                return HTTRANSPARENT;  /* Pass through */
            }
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            RECT rcWindow;
            GetWindowRect(hwnd, &rcWindow);
            UpdateRegionPositions(rcWindow.left, rcWindow.top);
            if (IsClickableRegionAt(pt)) {
                return HTCLIENT;  /* Allow click */
            }
            return HTTRANSPARENT;  /* Pass through */
        }
    }
    if (DispatchAppMessage(hwnd, msg)) {
        return 0;
    }
    for (const MessageDispatchEntry* entry = MESSAGE_DISPATCH_TABLE; entry->handler; entry++) {
        if (entry->msg == msg) {
            return entry->handler(hwnd, wp, lp);
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
