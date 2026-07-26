/**
 * @file window_message_handlers.c
 * @brief Handles window lifecycle, painting, timers, placement, and visibility messages.
 */

#include "window_procedure/window_message_handlers_internal.h"
#include "config/config_watcher.h"
#include "drag_scale.h"
#include "drawing.h"
#include "menu_preview.h"
#include "timer/main_timer.h"
#include "timer/timer_events.h"
#include "tray/tray_events.h"
#include "window.h"
#include "window/window_desktop_integration.h"
#include "window/window_placement.h"
#include "window/window_visual_effects.h"
#include "window_procedure/window_events.h"
#include "window_procedure/window_hotkeys.h"
#include "window_procedure/window_utils.h"
#include "../resource/resource.h"

LRESULT HandleCreate(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    RegisterGlobalHotkeys(hwnd);
    HandleWindowCreate(hwnd);
    ConfigWatcher_Start(hwnd);
    return 0;
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
        WindowMessageInternal_CancelTrackedMenuPreview(hwnd);
        return 0;
    }
    if (wp == IDT_ANIMATION_PREVIEW_DELAY) {
        WindowMessageInternal_DispatchPendingMenuPreview(hwnd);
        return 0;
    }
    if (wp == IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD) {
        WindowMessageInternal_StopEditExitRightClickShield(hwnd);
        return 0;
    }
    /* Handle click-through timer for dynamic WS_EX_TRANSPARENT switching */
    if (wp == GetClickThroughTimerId()) {
        UpdateClickThroughState(hwnd);
        return 0;
    }
    HandleTimerEvent(hwnd, wp);
    return 0;
}

LRESULT HandleMainTimerTick(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    LONG generation = (LONG)wp;
    if (MainTimer_ShouldHandleTick(generation)) {
        /* Delegate to main timer event handler */
        HandleTimerEvent(hwnd, TIMER_ID_MAIN);
    }
    /* Release coalescing gate for the next high-precision tick message. */
    MainTimer_NotifyTickHandled(generation);
    return 0;
}

LRESULT HandleDestroy(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    WindowMessageInternal_ResetEditExitRightClickState(hwnd);
    StopMenuPreviewTrackingForCommand(hwnd);
    CancelPreview(hwnd);
    UnregisterGlobalHotkeys(hwnd);
    HandleWindowDestroy(hwnd);
    ConfigWatcher_Stop();

    return 0;
}

LRESULT HandleTrayIcon(HWND hwnd, WPARAM wp, LPARAM lp) {
    HandleTrayIconMessage(hwnd, (UINT)wp, (UINT)lp);
    return 0;
}

LRESULT HandleWindowPosChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    const WINDOWPOS* pwp = (const WINDOWPOS*)lp;
    HandleTopmostVisibilityChange(hwnd, pwp);
    if (!(pwp->flags & SWP_NOSIZE)) {
        if (CLOCK_EDIT_MODE) {
            // Region update logic removed - relying on UpdateLayeredWindow alpha channel
        }
    }
    return 0;
}

LRESULT HandleShowWindow(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp) {
        HandleTopmostShownEvent(hwnd);
    } else {
        HandleTopmostHiddenEvent(hwnd);
    }
    return DefWindowProc(hwnd, WM_SHOWWINDOW, wp, lp);
}

LRESULT HandleDisplayChange(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (CLOCK_IS_DRAGGING) {
        return 0;
    }

    if (!BeginSystemPositionChangeGuard(hwnd)) {
        RestoreWindowPositionAfterSystemChange(hwnd);
    }
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
}

LRESULT HandleDpiChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    if (CLOCK_IS_DRAGGING) {
        return 0;
    }

    RECT* suggested = (RECT*)lp;
    if (CLOCK_EDIT_MODE) {
        if (suggested) {
            RECT manualRect = {0};
            POINT position = {suggested->left, suggested->top};
            if (GetWindowRect(hwnd, &manualRect)) {
                POINT restorePosition = {0};
                if (WindowPlacement_GetManualTopLeftRestore(
                        &manualRect, suggested, &restorePosition)) {
                    position = restorePosition;
                }
            }
            SetWindowPos(hwnd, NULL,
                         position.x,
                         position.y,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        ClearPendingSystemPositionRestore();
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    BOOL restoreScheduled = BeginSystemPositionChangeGuard(hwnd);

    if (suggested) {
        SetWindowPos(hwnd, NULL,
                     suggested->left,
                     suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (!restoreScheduled) {
        RestoreWindowPositionAfterSystemChange(hwnd);
    }
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

LRESULT HandleClose(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    CancelScheduledConfigSave(hwnd);
    if (CLOCK_EDIT_MODE) {
        EndEditMode(hwnd);
    } else {
        SaveWindowSettings(hwnd);
    }
    HideWindowIntentionally(hwnd);
    DestroyWindow(hwnd);
    return 0;
}

LRESULT HandleSysCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    if (HandleTopmostMinimizeCommand(hwnd, (UINT)wp)) {
        return 0;
    }

    return DefWindowProc(hwnd, WM_SYSCOMMAND, wp, lp);
}

LRESULT HandleSize(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (HandleTopmostSizeEvent(hwnd, wp)) {
        return 0;
    }

    return DefWindowProc(hwnd, WM_SIZE, wp, lp);
}
