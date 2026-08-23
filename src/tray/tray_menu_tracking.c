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

static BOOL IsWindowOwnedByShell(HWND window) {
    HWND shellWindow = GetShellWindow();
    if (!window || !shellWindow) return FALSE;

    DWORD windowProcessId = 0;
    DWORD shellProcessId = 0;
    GetWindowThreadProcessId(window, &windowProcessId);
    GetWindowThreadProcessId(shellWindow, &shellProcessId);
    return windowProcessId != 0 && windowProcessId == shellProcessId;
}

static DWORD GetShellInteractionThread(void) {
    HWND notificationArea = FindWindowW(L"Shell_TrayWnd", NULL);
    if (IsWindowOwnedByShell(notificationArea)) {
        return GetWindowThreadProcessId(notificationArea, NULL);
    }

    HWND shellWindow = GetShellWindow();
    return shellWindow
        ? GetWindowThreadProcessId(shellWindow, NULL) : 0;
}

static BOOL TryAcquireForeground(HWND window) {
    if (!window || !IsWindow(window)) return FALSE;
    if (IsForegroundWindow(window)) return TRUE;

    (void)SetActiveWindow(window);
    (void)SetForegroundWindow(window);
    if (IsForegroundWindow(window)) return TRUE;

    DWORD shellThread = GetShellInteractionThread();
    DWORD currentThread = GetCurrentThreadId();
    /* A tray click may leave a fullscreen game as the foreground process.
     * Never join Catime's input queue to that process.  The fallback targets
     * only Explorer/the configured Windows shell thread that owns the
     * notification area. */
    BOOL attached = shellThread != 0 &&
                    shellThread != currentThread &&
                    AttachThreadInput(currentThread, shellThread, TRUE);
    if (attached) {
        (void)BringWindowToTop(window);
        (void)SetActiveWindow(window);
        (void)SetForegroundWindow(window);
        (void)SetFocus(window);
        (void)AttachThreadInput(currentThread, shellThread, FALSE);
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
    if (ReadExtendedStyle(owner, &style)) {
        state->restoreNoActivate = (style & WS_EX_NOACTIVATE) != 0;
        state->restoreTransparent = (style & WS_EX_TRANSPARENT) != 0;
        LONG_PTR activeStyle =
            style & ~(WS_EX_NOACTIVATE | WS_EX_TRANSPARENT);
        if (activeStyle != style &&
            !WriteExtendedStyle(owner, activeStyle)) {
            state->restoreNoActivate = FALSE;
            state->restoreTransparent = FALSE;
        }
    }
    return TrayMenuTracking_ReassertForeground(state);
}

void TrayMenuTracking_End(TrayMenuTrackingState* state) {
    if (!state || !state->initialized) return;
    HWND owner = state->owner;
    if (owner && IsWindow(owner)) {
        (void)PostMessageW(owner, WM_NULL, 0, 0);
        if (state->restoreNoActivate || state->restoreTransparent) {
            LONG_PTR style = 0;
            if (ReadExtendedStyle(owner, &style)) {
                if (state->restoreNoActivate) {
                    style |= WS_EX_NOACTIVATE;
                }
                if (state->restoreTransparent) {
                    style |= WS_EX_TRANSPARENT;
                }
                (void)WriteExtendedStyle(owner, style);
            }
        }
    }
    memset(state, 0, sizeof(*state));
}
