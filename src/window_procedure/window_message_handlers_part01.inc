/**
 * @file window_message_handlers.c
 * @brief Windows message handlers implementation
 */

#include "window_procedure/window_message_handlers.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/window_hotkeys.h"
#include "window_procedure/window_events.h"
#include "window_procedure/window_menus.h"
#include "timer/timer_events.h"
#include "timer/main_timer.h"
#include "tray/tray_events.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_menu_font.h"
#include "tray/tray_menu_submenus.h"
#include "tray/tray_menu_theme.h"
#include "config/config_watcher.h"
#include "update/update_internal.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_plugin_security.h"
#include "plugin/plugin_process.h"
#include "audio_player.h"
#include "timer/timer.h"
#include "drag_scale.h"
#include "window_procedure/window_procedure.h"
#include "cli.h"
#include "window.h"
#include "config.h"
#include "color/color.h"
#include "drawing.h"
#include "menu_preview.h"
#include "preview_display.h"
#include "text_effect.h"
#include "window/window_visual_effects.h"
#include "window/window_desktop_integration.h"
#include "window/window_placement.h"
#include "../resource/resource.h"
#include "window_procedure/window_drop_target.h"
#include "plugin/plugin_data.h"
#include "plugin/plugin_manager.h"
#include "color/gradient.h"
#include "markdown/markdown_interactive.h"
#include "pomodoro.h"
#include "log.h"
#include <stdio.h>
#include <windowsx.h>

/* 50ms menu debounce prevents accidental double-clicks during menu interaction */
#define MENU_DEBOUNCE_DELAY_MS 50
#define ANIMATION_PREVIEW_DELAY_MS 30
#define BUFFER_SIZE_CLI_INPUT 256
#define BUFFER_SIZE_MENU_ITEM 100
#define IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD 42427
#define EDIT_EXIT_RIGHT_CLICK_SHIELD_MS 250u

UINT GetPendingAnimationPreviewItem(void);
static void ClearPendingMenuPreview(HWND hwnd);
static void DispatchPendingMenuPreview(HWND hwnd);
static void ResetMenuPreviewTracking(HWND hwnd);
static void StartMenuDebounceTimer(HWND hwnd);
static void StartAnimationPreviewDelayTimer(HWND hwnd);

extern UINT WM_TASKBARCREATED;
extern BOOL CLOCK_EDIT_MODE;

static BOOL g_pendingEditExitRightClick = FALSE;
static DWORD g_suppressContextMenuUntilTick = 0;
static DWORD g_editExitRightClickShieldUntilTick = 0;

static BOOL IsTickActive(DWORD untilTick) {
    if (untilTick == 0) {
        return FALSE;
    }
    return (LONG)(GetTickCount() - untilTick) < 0;
}

static void SuppressContextMenuBriefly(void) {
    DWORD until = GetTickCount() + 500u;
    g_suppressContextMenuUntilTick = until ? until : 1u;
}

static BOOL IsContextMenuSuppressed(void) {
    if (g_pendingEditExitRightClick) {
        return TRUE;
    }

    if (g_suppressContextMenuUntilTick == 0) {
        return FALSE;
    }

    if (IsTickActive(g_suppressContextMenuUntilTick)) {
        return TRUE;
    }

    g_suppressContextMenuUntilTick = 0;
    return FALSE;
}

BOOL IsEditExitRightClickShieldActive(void) {
    if (g_pendingEditExitRightClick) {
        return TRUE;
    }

    if (IsTickActive(g_editExitRightClickShieldUntilTick)) {
        return TRUE;
    }

    g_editExitRightClickShieldUntilTick = 0;
    return FALSE;
}

static void StartEditExitRightClickShield(HWND hwnd) {
    DWORD until = GetTickCount() + EDIT_EXIT_RIGHT_CLICK_SHIELD_MS;
    g_editExitRightClickShieldUntilTick = until ? until : 1u;
    SetClickThrough(hwnd, FALSE);
    if (!SetTimer(hwnd,
                  IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD,
                  EDIT_EXIT_RIGHT_CLICK_SHIELD_MS,
                  NULL)) {
        g_editExitRightClickShieldUntilTick = 0;
        if (!CLOCK_EDIT_MODE) {
            SetClickThrough(hwnd, TRUE);
        }
        LOG_WARNING("Failed to start edit-exit right-click shield timer (error=%lu)",
                    GetLastError());
    }
}

static void StopEditExitRightClickShield(HWND hwnd) {
    KillTimer(hwnd, IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD);
    g_editExitRightClickShieldUntilTick = 0;
    if (!CLOCK_EDIT_MODE) {
        SetClickThrough(hwnd, TRUE);
    }
}

static void ResetEditExitRightClickState(HWND hwnd) {
    KillTimer(hwnd, IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD);
    g_pendingEditExitRightClick = FALSE;
    g_suppressContextMenuUntilTick = 0;
    g_editExitRightClickShieldUntilTick = 0;
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
}

static void ClearPendingEditExitRightClick(HWND hwnd) {
    g_pendingEditExitRightClick = FALSE;
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
}

/* ============================================================================
 * Message Handler Implementations
 * ============================================================================ */

LRESULT HandleCreate(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    RegisterGlobalHotkeys(hwnd);
    HandleWindowCreate(hwnd);
    ConfigWatcher_Start(hwnd);
    return 0;
}

LRESULT HandleSetCursor(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;

    /* In non-edit mode, show hand cursor for clickable regions */
    if (!CLOCK_EDIT_MODE && LOWORD(lp) == HTCLIENT && HasClickableRegions()) {
        POINT pt;
        GetCursorPos(&pt);

        RECT rcWindow;
        GetWindowRect(hwnd, &rcWindow);
        UpdateRegionPositions(rcWindow.left, rcWindow.top);

        if (IsClickableRegionAt(pt)) {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
    }

    if (CLOCK_EDIT_MODE && LOWORD(lp) == HTCLIENT) {
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return TRUE;
    }
    if (LOWORD(lp) == HTCLIENT || lp == CLOCK_WM_TRAYICON) {
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return TRUE;
    }
    return DefWindowProc(hwnd, WM_SETCURSOR, wp, lp);
}

LRESULT HandleLButtonDown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;

    /* In non-edit mode, check for clickable region clicks */
    if (!CLOCK_EDIT_MODE && HasClickableRegions()) {
        POINT pt;
        GetCursorPos(&pt);

        /* Update region positions */
        RECT rcWindow;
        GetWindowRect(hwnd, &rcWindow);
        UpdateRegionPositions(rcWindow.left, rcWindow.top);

        if (HandleRegionClickAt(pt, hwnd)) {
            return 0;
        }
    }

    StartDragWindow(hwnd);
    return 0;
}

LRESULT HandleLButtonUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    EndDragWindow(hwnd);
    return 0;
}

LRESULT HandleMouseWheel(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    int delta = GET_WHEEL_DELTA_WPARAM(wp);
    HandleScaleWindow(hwnd, delta);
    return 0;
}

LRESULT HandleMouseMove(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    TryStartDragWindowFromMouseMove(hwnd);
    if (HandleDragWindowWithButtonState(
            hwnd, (wp & MK_LBUTTON) != 0)) return 0;
    return DefWindowProc(hwnd, WM_MOUSEMOVE, wp, lp);
}

LRESULT HandlePaint(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    HandleWindowPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
    return 0;
}

LRESULT HandleEraseBkgnd(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd;
    (void)wp;
    (void)lp;
    return 1;
}

LRESULT HandleTimer(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp == TIMER_ID_DISPLAY_RESTORE) {
        KillTimer(hwnd, TIMER_ID_DISPLAY_RESTORE);
        RestoreWindowPositionAfterSystemChange(hwnd);
        return 0;
    }
    if (wp == IDT_MENU_DEBOUNCE) {
        KillTimer(hwnd, IDT_MENU_DEBOUNCE);
        CancelPreview(hwnd);
        RestoreWindowVisibility(hwnd);
        return 0;
    }
    if (wp == IDT_ANIMATION_PREVIEW_DELAY) {
        DispatchPendingMenuPreview(hwnd);
        return 0;
    }
    if (wp == IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD) {
        StopEditExitRightClickShield(hwnd);
        return 0;
    }
    /* Handle click-through timer for dynamic WS_EX_TRANSPARENT switching */
    if (wp == GetClickThroughTimerId()) {
        UpdateClickThroughState(hwnd);
        return 0;
    }
