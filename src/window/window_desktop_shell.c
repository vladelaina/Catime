/**
 * @file window_desktop_shell.c
 * @brief Desktop shell discovery, ownership, and taskbar geometry
 */
#include "window/window_desktop_integration.h"
#include "window_desktop_integration_internal.h"

#include "log.h"

#include <wchar.h>

#define PROGMAN_CLASS L"Progman"
#define WORKERW_CLASS L"WorkerW"
#define SHELLDLL_CLASS L"SHELLDLL_DefView"
#define TASKBAR_CLASS L"Shell_TrayWnd"
#define SECONDARY_TASKBAR_CLASS L"Shell_SecondaryTrayWnd"

BOOL WindowDesktop_IsValid(HWND hwnd, const char* caller) {
    if (!hwnd || !IsWindow(hwnd)) {
        LOG_WARNING("%s called with invalid window handle",
                    caller ? caller : "Desktop integration");
        return FALSE;
    }
    return TRUE;
}

HWND WindowDesktop_FindWorkerWindow(void) {
    HWND progman = FindWindowW(PROGMAN_CLASS, NULL);
    HWND worker;

    if (!progman) return NULL;
    worker = FindWindowExW(NULL, NULL, WORKERW_CLASS, NULL);
    while (worker) {
        if (FindWindowExW(worker, NULL, SHELLDLL_CLASS, NULL)) {
            return worker;
        }
        worker = FindWindowExW(NULL, worker, WORKERW_CLASS, NULL);
    }
    return progman;
}

static BOOL IsWindowOfClass(HWND hwnd, const wchar_t* className) {
    wchar_t actualClass[64] = {0};

    if (!hwnd || !IsWindow(hwnd) || !className) return FALSE;
    if (GetClassNameW(hwnd, actualClass, _countof(actualClass)) == 0) {
        return FALSE;
    }
    return wcscmp(actualClass, className) == 0;
}

BOOL WindowDesktop_TrySetOwner(HWND hwnd, HWND owner) {
    LONG_PTR result;
    DWORD error;

    if (owner == NULL) {
        SetLastError(0);
        if ((HWND)GetWindowLongPtr(hwnd, GWLP_HWNDPARENT) == NULL &&
            GetLastError() == 0) {
            return TRUE;
        }
    }

    SetLastError(0);
    result = SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (LONG_PTR)owner);
    error = GetLastError();
    if (result == 0 && error != 0) {
        if (owner == NULL) {
            SetLastError(0);
            if ((HWND)GetWindowLongPtr(hwnd, GWLP_HWNDPARENT) == NULL &&
                GetLastError() == 0) {
                return TRUE;
            }
        }
        LOG_WARNING("Failed to update desktop window owner (error=%lu)",
                    error);
        return FALSE;
    }
    return TRUE;
}

BOOL WindowDesktop_TrySetNoActivate(HWND hwnd, BOOL noActivate) {
    LONG style;
    LONG desiredStyle;

    SetLastError(0);
    style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (style == 0 && GetLastError() != 0) {
        LOG_WARNING("Failed to read window extended style (error=%lu)",
                    GetLastError());
        return FALSE;
    }
    desiredStyle = noActivate
        ? style | WS_EX_NOACTIVATE
        : style & ~WS_EX_NOACTIVATE;
    if (desiredStyle == style) return TRUE;

    SetLastError(0);
    if (SetWindowLong(hwnd, GWL_EXSTYLE, desiredStyle) == 0 &&
        GetLastError() != 0) {
        LOG_WARNING("Failed to update window extended style (error=%lu)",
                    GetLastError());
        return FALSE;
    }
    return TRUE;
}

BOOL WindowDesktop_GetTopmostState(HWND hwnd, BOOL* topmost) {
    LONG style;

    if (!topmost) return FALSE;
    SetLastError(0);
    style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (style == 0 && GetLastError() != 0) {
        LOG_WARNING("Failed to read topmost state (error=%lu)",
                    GetLastError());
        return FALSE;
    }
    *topmost = (style & WS_EX_TOPMOST) != 0;
    return TRUE;
}

BOOL WindowDesktop_IsTopmostStateApplied(HWND hwnd, BOOL topmost) {
    LONG style;
    HWND owner;

    SetLastError(0);
    style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (style == 0 && GetLastError() != 0) return FALSE;
    if (((style & WS_EX_TOPMOST) != 0) != topmost) return FALSE;

    SetLastError(0);
    owner = (HWND)GetWindowLongPtr(hwnd, GWLP_HWNDPARENT);
    if (GetLastError() != 0) return FALSE;
    if (topmost) {
        return owner == NULL && (style & WS_EX_NOACTIVATE) == 0;
    }
    return owner != NULL && (style & WS_EX_NOACTIVATE) != 0 &&
           (IsWindowOfClass(owner, WORKERW_CLASS) ||
            IsWindowOfClass(owner, PROGMAN_CLASS));
}

static BOOL RectanglesOverlap(const RECT* first, const RECT* second) {
    return first && second &&
           first->left < second->right && first->right > second->left &&
           first->top < second->bottom && first->bottom > second->top;
}

static BOOL OverlapsTaskbarClass(const RECT* windowRect,
                                 const wchar_t* className) {
    HWND taskbar = NULL;
    while ((taskbar = FindWindowExW(NULL, taskbar, className, NULL)) != NULL) {
        RECT taskbarRect = {0};
        if (IsWindowVisible(taskbar) &&
            GetWindowRect(taskbar, &taskbarRect) &&
            RectanglesOverlap(windowRect, &taskbarRect)) {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL WindowDesktop_OverlapsAnyTaskbar(const RECT* windowRect) {
    return OverlapsTaskbarClass(windowRect, TASKBAR_CLASS) ||
           OverlapsTaskbarClass(windowRect, SECONDARY_TASKBAR_CLASS);
}

static BOOL FindTaskbarRect(HMONITOR monitor, const wchar_t* className,
                            RECT* output) {
    HWND taskbar = NULL;
    while ((taskbar = FindWindowExW(NULL, taskbar, className, NULL)) != NULL) {
        if (MonitorFromWindow(taskbar, MONITOR_DEFAULTTONULL) != monitor) {
            continue;
        }
        if (GetWindowRect(taskbar, output)) return TRUE;
    }
    return FALSE;
}

BOOL GetTaskbarRectForMonitor(HMONITOR monitor, RECT* output) {
    if (!monitor || !output) return FALSE;
    return FindTaskbarRect(monitor, TASKBAR_CLASS, output) ||
           FindTaskbarRect(monitor, SECONDARY_TASKBAR_CLASS, output);
}

void ReattachToDesktop(HWND hwnd) {
    HWND desktop;

    if (!WindowDesktop_IsValid(hwnd, "ReattachToDesktop")) return;
    desktop = WindowDesktop_FindWorkerWindow();
    if (desktop) {
        WindowDesktop_TrySetOwner(hwnd, desktop);
    } else {
        WindowDesktop_TrySetOwner(hwnd, NULL);
        LOG_WARNING("Desktop anchor unavailable; window owner was cleared");
    }
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);
}
