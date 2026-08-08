/**
 * @file taskbar_monitor_parent.c
 * @brief Idempotent Explorer parent and style transitions.
 */

#include "taskbar_monitor_internal.h"

#include "log.h"

BOOL TaskbarMonitor_IsWindowShown(HWND window) {
    return IsWindow(window) &&
           (GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE) != 0;
}

static BOOL SetWindowLongChecked(
    HWND window, int index, LONG_PTR value, const char* operation) {
    SetLastError(ERROR_SUCCESS);
    LONG_PTR previous = SetWindowLongPtrW(window, index, value);
    DWORD error = GetLastError();
    if (previous == 0 && error != ERROR_SUCCESS) {
        LOG_WARNING("Taskbar monitor %s failed (error=%lu)",
                    operation, error);
        return FALSE;
    }
    return TRUE;
}

BOOL TaskbarMonitor_EnsureWindowAtTop(void) {
    HWND window = g_taskbarMonitor.window;
    if (!IsWindow(window)) return FALSE;
    if (!GetWindow(window, GW_HWNDPREV)) return TRUE;
    if (SetWindowPos(
            window, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
            SWP_NOOWNERZORDER | SWP_NOSENDCHANGING)) {
        return TRUE;
    }
    LOG_WARNING(
        "Taskbar monitor z-order restore failed (error=%lu)",
        GetLastError());
    return FALSE;
}

BOOL TaskbarMonitor_SetWindowParent(HWND parent, BOOL childStyle) {
    HWND window = g_taskbarMonitor.window;
    if (!IsWindow(window) || !IsWindow(parent)) return FALSE;

    BOOL parentChanged = GetAncestor(window, GA_PARENT) != parent;
    if (parentChanged) {
        SetLastError(ERROR_SUCCESS);
        HWND previous = SetParent(window, parent);
        DWORD error = GetLastError();
        if (!previous && error != ERROR_SUCCESS) {
            LOG_WARNING(
                "Taskbar monitor shell attachment failed (error=%lu)",
                error);
            return FALSE;
        }
    }

    LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    LONG_PTR desiredStyle = childStyle
        ? (style & ~WS_POPUP) | WS_CHILD | WS_CLIPSIBLINGS
        : (style & ~WS_CHILD) | WS_POPUP | WS_CLIPSIBLINGS;
    BOOL styleChanged = desiredStyle != style;
    if (styleChanged && !SetWindowLongChecked(
            window, GWL_STYLE, desiredStyle, "style update")) {
        return FALSE;
    }

    LONG_PTR extendedStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    LONG_PTR desiredExtendedStyle =
        (extendedStyle & ~WS_EX_TOPMOST) |
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED |
        WS_EX_TRANSPARENT;
    if (desiredExtendedStyle != extendedStyle &&
        !SetWindowLongChecked(
            window, GWL_EXSTYLE, desiredExtendedStyle,
            "extended-style update")) {
        return FALSE;
    }

    if ((parentChanged || styleChanged) &&
        !SetWindowPos(
            window, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
            SWP_FRAMECHANGED)) {
        LOG_WARNING(
            "Taskbar monitor frame synchronization failed (error=%lu)",
            GetLastError());
        return FALSE;
    }

    extendedStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if ((extendedStyle & WS_EX_TRANSPARENT) == 0 &&
        !SetWindowLongChecked(
            window, GWL_EXSTYLE,
            extendedStyle | WS_EX_TRANSPARENT,
            "mouse pass-through enable")) {
        return FALSE;
    }
    return TRUE;
}
