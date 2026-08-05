/**
 * @file window_topmost_visibility.c
 * @brief Intentional hiding and recovery from external minimize/hide events
 */
#include "window/window_desktop_integration.h"
#include "window_desktop_integration_internal.h"

#include "config.h"
#include "drawing/drawing_render.h"
#include "log.h"
#include "plugin/plugin_data.h"
#include "timer/timer.h"
#include "window/window_core.h"
#include "../../resource/resource.h"

static BOOL s_restoreActive = FALSE;
static BOOL s_intentionallyHidden = FALSE;
static BOOL s_restoringMinimize = FALSE;

static void CancelVisibilityRestore(HWND hwnd) {
    if (s_restoreActive && hwnd && IsWindow(hwnd)) {
        KillTimer(hwnd, TIMER_ID_TOPMOST_VISIBILITY_RESTORE);
    }
    s_restoreActive = FALSE;
}

static BOOL WindowShouldBeVisible(void) {
    if (CLOCK_EDIT_MODE || CLOCK_SHOW_CURRENT_TIME || CLOCK_COUNT_UP ||
        PluginData_IsActive()) {
        return TRUE;
    }
    return CLOCK_TOTAL_TIME > 0 &&
           countdown_elapsed_time < CLOCK_TOTAL_TIME;
}

static BOOL ShouldRecoverVisibility(HWND hwnd) {
    return WindowDesktop_IsValid(hwnd, "ShouldRecoverTopmostVisibility") &&
           CLOCK_WINDOW_EFFECTIVE_TOPMOST &&
           !s_intentionallyHidden && WindowShouldBeVisible();
}

static BOOL ScheduleVisibilityRestore(HWND hwnd) {
    if (!ShouldRecoverVisibility(hwnd)) return FALSE;
    if (s_restoreActive) return TRUE;
    if (!SetTimer(hwnd, TIMER_ID_TOPMOST_VISIBILITY_RESTORE, 100, NULL)) {
        LOG_WARNING("Failed to schedule topmost visibility restore (error=%lu)",
                    GetLastError());
        return FALSE;
    }
    s_restoreActive = TRUE;
    return TRUE;
}

void EnsureWindowVisibleWithTopmostState(HWND hwnd) {
    if (!WindowDesktop_IsValid(hwnd,
                               "EnsureWindowVisibleWithTopmostState")) {
        return;
    }
    s_intentionallyHidden = FALSE;
    CancelVisibilityRestore(hwnd);
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }
    RefreshWindowTopmostState(hwnd);
    if (CLOCK_WINDOW_EFFECTIVE_TOPMOST) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
                     SWP_SHOWWINDOW);
    }
    TryRestorePendingWindowPosition(hwnd);
}

void HideWindowIntentionally(HWND hwnd) {
    if (!WindowDesktop_IsValid(hwnd, "HideWindowIntentionally")) return;
    s_intentionallyHidden = TRUE;
    CancelVisibilityRestore(hwnd);
    StopDrawingRenderAnimationTimer(hwnd);
    ShowWindow(hwnd, SW_HIDE);
}

BOOL HandleTopmostVisibilityChange(HWND hwnd, const WINDOWPOS* pwp) {
    BOOL hiddenByPosition;
    BOOL hiddenNow;

    if (!WindowDesktop_IsValid(hwnd,
                               "HandleTopmostVisibilityChange")) {
        return FALSE;
    }
    if (!pwp) s_restoreActive = FALSE;
    if (pwp && (pwp->flags & SWP_SHOWWINDOW)) {
        s_intentionallyHidden = FALSE;
        CancelVisibilityRestore(hwnd);
        return FALSE;
    }

    hiddenByPosition = pwp && (pwp->flags & SWP_HIDEWINDOW);
    hiddenNow = !IsWindowVisible(hwnd) || IsIconic(hwnd);
    if (pwp && !hiddenByPosition) return FALSE;
    if (!ShouldRecoverVisibility(hwnd)) return FALSE;
    if (!hiddenByPosition && !hiddenNow) return FALSE;
    if (pwp) return ScheduleVisibilityRestore(hwnd);

    EnsureWindowVisibleWithTopmostState(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

BOOL HandleTopmostHiddenEvent(HWND hwnd) {
    if (!WindowDesktop_IsValid(hwnd, "HandleTopmostHiddenEvent")) {
        return FALSE;
    }
    return ScheduleVisibilityRestore(hwnd);
}

void HandleTopmostShownEvent(HWND hwnd) {
    if (!WindowDesktop_IsValid(hwnd, "HandleTopmostShownEvent")) return;
    s_intentionallyHidden = FALSE;
    CancelVisibilityRestore(hwnd);
    TryRestorePendingWindowPosition(hwnd);
}

BOOL HandleTopmostMinimizeCommand(HWND hwnd, UINT sysCommand) {
    if (!WindowDesktop_IsValid(hwnd,
                               "HandleTopmostMinimizeCommand")) {
        return FALSE;
    }
    if ((sysCommand & 0xFFF0) != SC_MINIMIZE ||
        !CLOCK_WINDOW_EFFECTIVE_TOPMOST) {
        return FALSE;
    }
    EnsureWindowVisibleWithTopmostState(hwnd);
    return TRUE;
}

BOOL HandleTopmostSizeEvent(HWND hwnd, WPARAM sizeType) {
    if (!WindowDesktop_IsValid(hwnd, "HandleTopmostSizeEvent")) return FALSE;
    if (sizeType != SIZE_MINIMIZED ||
        !CLOCK_WINDOW_EFFECTIVE_TOPMOST) {
        return FALSE;
    }
    if (s_restoringMinimize) return TRUE;
    s_restoringMinimize = TRUE;
    EnsureWindowVisibleWithTopmostState(hwnd);
    s_restoringMinimize = FALSE;
    return TRUE;
}

void WindowTopmostVisibility_Cleanup(HWND hwnd) {
    CancelVisibilityRestore(hwnd);
    s_intentionallyHidden = FALSE;
    s_restoringMinimize = FALSE;
}
