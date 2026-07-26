/**
 * @file window_message_mouse.c
 * @brief Handles mouse, edit-mode, and pointer interaction messages.
 */

#include "window_procedure/window_message_handlers_internal.h"
#include "config.h"
#include "drag_scale.h"
#include "log.h"
#include "markdown/markdown_interactive.h"
#include "window.h"
#include "window/window_visual_effects.h"
#include "window_procedure/window_events.h"
#include "window_procedure/window_procedure.h"
#include "window_procedure/window_utils.h"

#include <windowsx.h>

#define EDIT_EXIT_RIGHT_CLICK_SHIELD_MS 250u

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

void WindowMessageInternal_StopEditExitRightClickShield(HWND hwnd) {
    KillTimer(hwnd, IDT_EDIT_EXIT_RIGHT_CLICK_SHIELD);
    g_editExitRightClickShieldUntilTick = 0;
    if (!CLOCK_EDIT_MODE) {
        SetClickThrough(hwnd, TRUE);
    }
}

void WindowMessageInternal_ResetEditExitRightClickState(HWND hwnd) {
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

LRESULT HandleRButtonUp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (g_pendingEditExitRightClick) {
        /* End the left-drag transaction before releasing right-click capture. */
        g_pendingEditExitRightClick = FALSE;
        if (CLOCK_EDIT_MODE) {
            EndEditMode(hwnd);
        }
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        SuppressContextMenuBriefly();
        StartEditExitRightClickShield(hwnd);
        return 0;
    }
    if (CLOCK_EDIT_MODE) {
        EndEditMode(hwnd);
        SuppressContextMenuBriefly();
        StartEditExitRightClickShield(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_RBUTTONUP, wp, lp);
}

LRESULT HandleRButtonDown(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (CLOCK_EDIT_MODE) {
        g_pendingEditExitRightClick = TRUE;
        SetCapture(hwnd);
        return 0;
    }

    ClearPendingEditExitRightClick(hwnd);

    if (GetKeyState(VK_CONTROL) & 0x8000) {
        ToggleEditMode(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_RBUTTONDOWN, wp, lp);
}

LRESULT HandleContextMenu(HWND hwnd, WPARAM wp, LPARAM lp) {
    BOOL suppressed = CLOCK_EDIT_MODE || IsContextMenuSuppressed();
    if (suppressed) {
        return 0;
    }
    return DefWindowProc(hwnd, WM_CONTEXTMENU, wp, lp);
}

LRESULT HandleCaptureChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    if ((HWND)lp != hwnd && CLOCK_IS_DRAGGING) {
        EndDragWindow(hwnd);
    }
    if ((HWND)lp != hwnd && g_pendingEditExitRightClick) {
        g_pendingEditExitRightClick = FALSE;
    }
    return DefWindowProc(hwnd, WM_CAPTURECHANGED, wp, lp);
}

LRESULT HandleCancelMode(HWND hwnd, WPARAM wp, LPARAM lp) {
    if (CLOCK_IS_DRAGGING) {
        EndDragWindow(hwnd);
    }
    ClearPendingEditExitRightClick(hwnd);
    return DefWindowProc(hwnd, WM_CANCELMODE, wp, lp);
}

LRESULT HandleLButtonDblClk(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (!CLOCK_EDIT_MODE) {
        StartEditMode(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, WM_LBUTTONDBLCLK, wp, lp);
}
