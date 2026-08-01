/**
 * @file tray_menu_tracking.c
 * @brief Reliable foreground ownership around TrackPopupMenu.
 */

#include "tray/tray_menu_tracking.h"

#include <string.h>

static BOOL ReadExtendedStyle(HWND window, LONG_PTR* style) {
    if (!window || !IsWindow(window) || !style) return FALSE;
    SetLastError(ERROR_SUCCESS);
    LONG_PTR value = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if (value == 0 && GetLastError() != ERROR_SUCCESS) return FALSE;
    *style = value;
    return TRUE;
}

static BOOL WriteExtendedStyle(HWND window, LONG_PTR style) {
    SetLastError(ERROR_SUCCESS);
    LONG_PTR previous = SetWindowLongPtrW(window, GWL_EXSTYLE, style);
    if (previous == 0 && GetLastError() != ERROR_SUCCESS) return FALSE;
    /* The style write already succeeded. Frame refresh is best-effort and
     * must not suppress restoring WS_EX_NOACTIVATE at the end of tracking. */
    (void)SetWindowPos(
        window, NULL, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
        SWP_NOACTIVATE | SWP_FRAMECHANGED);
    return TRUE;
}

static BOOL IsForegroundWindow(HWND window) {
    return window && GetForegroundWindow() == window;
}

static BOOL TryAcquireForeground(HWND window) {
    if (!window || !IsWindow(window)) return FALSE;
    if (IsForegroundWindow(window)) return TRUE;

    (void)SetActiveWindow(window);
    (void)SetForegroundWindow(window);
    if (IsForegroundWindow(window)) return TRUE;

    HWND foreground = GetForegroundWindow();
    DWORD foregroundThread = foreground
        ? GetWindowThreadProcessId(foreground, NULL) : 0;
    DWORD currentThread = GetCurrentThreadId();
    BOOL attached = foregroundThread != 0 &&
                    foregroundThread != currentThread &&
                    AttachThreadInput(currentThread, foregroundThread, TRUE);
    if (attached) {
        (void)BringWindowToTop(window);
        (void)SetActiveWindow(window);
        (void)SetForegroundWindow(window);
        (void)SetFocus(window);
        (void)AttachThreadInput(currentThread, foregroundThread, FALSE);
    }
    return IsForegroundWindow(window);
}

BOOL TrayMenuTracking_ReassertForeground(TrayMenuTrackingState* state) {
    if (!state || !state->initialized ||
        !state->owner || !IsWindow(state->owner)) {
        return FALSE;
    }
    if (TryAcquireForeground(state->owner)) {
        state->foregroundAcquired = TRUE;
        return TRUE;
    }
    SwitchToThread();
    state->foregroundAcquired = TryAcquireForeground(state->owner);
    return state->foregroundAcquired;
}

BOOL TrayMenuTracking_Begin(HWND owner, TrayMenuTrackingState* state) {
    if (!state) return FALSE;
    memset(state, 0, sizeof(*state));
    if (!owner || !IsWindow(owner)) return FALSE;

    state->owner = owner;
    state->initialized = TRUE;
    LONG_PTR style = 0;
    if (ReadExtendedStyle(owner, &style) &&
        (style & WS_EX_NOACTIVATE)) {
        state->restoreNoActivate = TRUE;
        if (!WriteExtendedStyle(owner, style & ~WS_EX_NOACTIVATE)) {
            state->restoreNoActivate = FALSE;
        }
    }
    return TrayMenuTracking_ReassertForeground(state);
}

void TrayMenuTracking_End(TrayMenuTrackingState* state) {
    if (!state || !state->initialized) return;
    HWND owner = state->owner;
    if (owner && IsWindow(owner)) {
        (void)PostMessageW(owner, WM_NULL, 0, 0);
        if (state->restoreNoActivate) {
            LONG_PTR style = 0;
            if (ReadExtendedStyle(owner, &style)) {
                (void)WriteExtendedStyle(
                    owner, style | WS_EX_NOACTIVATE);
            }
        }
    }
    memset(state, 0, sizeof(*state));
}
