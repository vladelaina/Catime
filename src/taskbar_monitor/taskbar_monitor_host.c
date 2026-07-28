/**
 * @file taskbar_monitor_host.c
 * @brief Explorer taskbar discovery, attachment, and slot management.
 */

#include "taskbar_monitor_internal.h"

#include "log.h"

#include <wchar.h>

static HWND FindDescendantByClass(HWND parent, const wchar_t* className) {
    HWND child = NULL;
    if (!parent || !className) return NULL;
    while ((child = FindWindowExW(parent, child, NULL, NULL)) != NULL) {
        wchar_t actualClass[64] = {0};
        if (GetClassNameW(child, actualClass, _countof(actualClass)) > 0 &&
            wcscmp(actualClass, className) == 0) {
            return child;
        }
        HWND nested = FindDescendantByClass(child, className);
        if (nested) return nested;
    }
    return NULL;
}

static BOOL WindowRectMatchesParent(
    HWND window, HWND parent, const RECT* expected) {
    RECT current = {0};
    return IsWindowVisible(window) &&
           TaskbarMonitor_GetWindowRectInParent(window, parent, &current) &&
           TaskbarMonitor_RectsNearEqual(&current, expected);
}

void TaskbarMonitor_RestoreClassicTaskList(void) {
    if (!g_taskbarMonitor.taskListReserved ||
        !IsWindow(g_taskbarMonitor.taskList) ||
        !IsWindow(g_taskbarMonitor.host)) {
        g_taskbarMonitor.taskListReserved = FALSE;
        return;
    }
    RECT current = {0};
    if (TaskbarMonitor_GetWindowRectInParent(
            g_taskbarMonitor.taskList, g_taskbarMonitor.host, &current) &&
        TaskbarMonitor_RectsNearEqual(
            &current, &g_taskbarMonitor.reservedTaskList)) {
        MoveWindow(g_taskbarMonitor.taskList,
                   g_taskbarMonitor.originalTaskList.left,
                   g_taskbarMonitor.originalTaskList.top,
                   g_taskbarMonitor.originalTaskList.right -
                       g_taskbarMonitor.originalTaskList.left,
                   g_taskbarMonitor.originalTaskList.bottom -
                       g_taskbarMonitor.originalTaskList.top, TRUE);
    }
    g_taskbarMonitor.taskListReserved = FALSE;
}

static BOOL SetMonitorParent(HWND parent, BOOL childStyle) {
    LONG_PTR style;
    LONG_PTR extendedStyle;
    DWORD error;
    if (!IsWindow(g_taskbarMonitor.window) || !IsWindow(parent)) return FALSE;
    SetLastError(ERROR_SUCCESS);
    HWND previous = SetParent(g_taskbarMonitor.window, parent);
    error = GetLastError();
    if (!previous && error != ERROR_SUCCESS) {
        LOG_WARNING("Taskbar monitor shell attachment failed (error=%lu)",
                    error);
        return FALSE;
    }
    style = GetWindowLongPtrW(g_taskbarMonitor.window, GWL_STYLE);
    if (childStyle) {
        style = (style & ~WS_POPUP) | WS_CHILD | WS_CLIPSIBLINGS;
    } else {
        style = (style & ~WS_CHILD) | WS_POPUP | WS_CLIPSIBLINGS;
    }
    SetWindowLongPtrW(g_taskbarMonitor.window, GWL_STYLE, style);
    extendedStyle = GetWindowLongPtrW(g_taskbarMonitor.window, GWL_EXSTYLE);
    extendedStyle = (extendedStyle & ~WS_EX_TOPMOST) |
                    WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                    WS_EX_LAYERED;
    if (g_taskbarMonitor.menuPreviewSessionActive) {
        extendedStyle |= WS_EX_TRANSPARENT;
    } else {
        extendedStyle &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtrW(
        g_taskbarMonitor.window, GWL_EXSTYLE, extendedStyle);
    SetWindowPos(g_taskbarMonitor.window, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    return TRUE;
}

static BOOL ReserveClassicSlot(RECT* monitorRect) {
    RECT taskRect = {0};
    RECT baseRect;
    RECT reserved = {0};
    int gap = TaskbarMonitor_ScaleForDpi(
        TASKBAR_MONITOR_GAP, g_taskbarMonitor.dpi);
    int minimum = TaskbarMonitor_ScaleForDpi(
        TASKBAR_MONITOR_MIN_TASK_LIST, g_taskbarMonitor.dpi);
    if (!IsWindow(g_taskbarMonitor.taskList) ||
        !IsWindow(g_taskbarMonitor.host) ||
        !TaskbarMonitor_GetWindowRectInParent(
            g_taskbarMonitor.taskList, g_taskbarMonitor.host, &taskRect)) {
        return FALSE;
    }
    if (g_taskbarMonitor.taskListReserved &&
        TaskbarMonitor_RectsNearEqual(
            &taskRect, &g_taskbarMonitor.reservedTaskList)) {
        baseRect = g_taskbarMonitor.originalTaskList;
    } else {
        baseRect = taskRect;
        g_taskbarMonitor.originalTaskList = taskRect;
        g_taskbarMonitor.taskListReserved = FALSE;
    }
    reserved = baseRect;
    RECT placement = {0};
    if (!TaskbarMonitor_CalculateClassicPlacement(
            &baseRect, g_taskbarMonitor.horizontal,
            g_taskbarMonitor.width, g_taskbarMonitor.height,
            gap, minimum, &reserved, &placement)) return FALSE;
    if (!g_taskbarMonitor.taskListReserved ||
        !TaskbarMonitor_RectsNearEqual(&taskRect, &reserved)) {
        if (!MoveWindow(g_taskbarMonitor.taskList,
                        reserved.left, reserved.top,
                        reserved.right - reserved.left,
                        reserved.bottom - reserved.top, TRUE)) return FALSE;
    }
    g_taskbarMonitor.reservedTaskList = reserved;
    g_taskbarMonitor.taskListReserved = TRUE;
    if (monitorRect) *monitorRect = placement;
    return TRUE;
}

static BOOL PositionClassicMonitor(const RECT* monitorRect) {
    if (!monitorRect) return FALSE;
    if (WindowRectMatchesParent(
            g_taskbarMonitor.window, g_taskbarMonitor.host, monitorRect)) {
        return TRUE;
    }
    return SetWindowPos(
        g_taskbarMonitor.window, HWND_TOP,
        monitorRect->left, monitorRect->top,
        monitorRect->right - monitorRect->left,
        monitorRect->bottom - monitorRect->top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

static BOOL PositionBesideNotificationArea(void) {
    RECT bounds = {0};
    RECT notifyRect = {0};
    int gap = TaskbarMonitor_ScaleForDpi(
        TASKBAR_MONITOR_GAP, g_taskbarMonitor.dpi);
    if (!GetClientRect(g_taskbarMonitor.taskbar, &bounds)) {
        return FALSE;
    }
    HWND notify = FindDescendantByClass(
        g_taskbarMonitor.taskbar, L"TrayNotifyWnd");
    BOOL hasNotify = notify && GetWindowRect(notify, &notifyRect);
    if (hasNotify) {
        MapWindowPoints(HWND_DESKTOP, g_taskbarMonitor.taskbar,
                        (POINT*)&notifyRect, 2);
    }
    int fallbackInset = TaskbarMonitor_ScaleForDpi(
        g_taskbarMonitor.horizontal ? 100 : 56,
        g_taskbarMonitor.dpi);
    RECT placement = {0};
    if (!TaskbarMonitor_CalculateModernPlacement(
            &bounds, &notifyRect, hasNotify,
            g_taskbarMonitor.horizontal,
            g_taskbarMonitor.width, g_taskbarMonitor.height,
            gap, fallbackInset, &placement)) return FALSE;
    if (WindowRectMatchesParent(
            g_taskbarMonitor.window, g_taskbarMonitor.taskbar, &placement)) {
        return TRUE;
    }
    return SetWindowPos(
        g_taskbarMonitor.window, HWND_TOP,
        placement.left, placement.top,
        placement.right - placement.left,
        placement.bottom - placement.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

static BOOL AttachModernMonitor(HWND taskbar) {
    if (!SetMonitorParent(
            taskbar, g_taskbarMonitor.modernTaskbar)) return FALSE;
    g_taskbarMonitor.host = taskbar;
    g_taskbarMonitor.taskList = NULL;
    g_taskbarMonitor.mode = TASKBAR_HOST_MODERN;
    return PositionBesideNotificationArea();
}

static void ConfigureHiddenRetry(void) {
    LONG_PTR style = GetWindowLongPtrW(g_taskbarMonitor.window, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtrW(
        g_taskbarMonitor.window, GWL_EXSTYLE);
    SetParent(g_taskbarMonitor.window, NULL);
    SetWindowLongPtrW(g_taskbarMonitor.window, GWL_STYLE,
                      (style & ~WS_CHILD) | WS_POPUP);
    exStyle = (exStyle & ~(WS_EX_TOPMOST | WS_EX_TRANSPARENT)) |
              WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED;
    if (g_taskbarMonitor.menuPreviewSessionActive) {
        exStyle |= WS_EX_TRANSPARENT;
    }
    SetWindowLongPtrW(g_taskbarMonitor.window, GWL_EXSTYLE,
                      exStyle);
    SetWindowPos(g_taskbarMonitor.window, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED | SWP_HIDEWINDOW);
    g_taskbarMonitor.taskbar = NULL;
    g_taskbarMonitor.host = NULL;
    g_taskbarMonitor.taskList = NULL;
    g_taskbarMonitor.modernTaskbar = FALSE;
    g_taskbarMonitor.mode = TASKBAR_HOST_NONE;
}

BOOL TaskbarMonitor_AttachToTaskbar(void) {
    RECT taskbarRect = {0};
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (!taskbar || !GetWindowRect(taskbar, &taskbarRect)) return FALSE;
    TaskbarMonitor_RestoreClassicTaskList();
    g_taskbarMonitor.taskbar = taskbar;
    g_taskbarMonitor.dpi = TaskbarMonitor_GetWindowDpi(taskbar);
    TaskbarMonitor_UpdateThemeState();
    TaskbarMonitor_RecreateFont();
    TaskbarMonitor_RefreshTextLayout();
    TaskbarMonitor_UpdateDimensions(&taskbarRect);
    BOOL modernTaskbar = TaskbarMonitor_IsModernTaskbar(taskbar);
    g_taskbarMonitor.modernTaskbar = modernTaskbar;
    if (g_taskbarMonitor.menuPreviewSessionActive) {
        return AttachModernMonitor(taskbar);
    }
    HWND host = FindDescendantByClass(taskbar, L"ReBarWindow32");
    if (!host) host = FindDescendantByClass(taskbar, L"WorkerW");
    HWND taskList = host
        ? FindDescendantByClass(host, L"MSTaskSwWClass") : NULL;
    if (!taskList && host) {
        taskList = FindDescendantByClass(host, L"MSTaskListWClass");
    }
    if (TaskbarMonitor_ShouldUseClassicPlacement(
            modernTaskbar, host != NULL, taskList != NULL)) {
        RECT monitorRect = {0};
        g_taskbarMonitor.host = host;
        g_taskbarMonitor.taskList = taskList;
        g_taskbarMonitor.mode = TASKBAR_HOST_CLASSIC;
        if (ReserveClassicSlot(&monitorRect) &&
            SetMonitorParent(host, FALSE) &&
            PositionClassicMonitor(&monitorRect)) return TRUE;
        TaskbarMonitor_RestoreClassicTaskList();
    }
    if (AttachModernMonitor(taskbar)) return TRUE;
    ConfigureHiddenRetry();
    return FALSE;
}

void TaskbarMonitor_RefreshAttachment(void) {
    if (!IsWindow(g_taskbarMonitor.window)) return;
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (!taskbar) {
        ShowWindow(g_taskbarMonitor.window, SW_HIDE);
        return;
    }
    if (taskbar != g_taskbarMonitor.taskbar ||
        !IsWindow(g_taskbarMonitor.taskbar) ||
        TaskbarMonitor_IsModernTaskbar(taskbar) !=
            g_taskbarMonitor.modernTaskbar ||
        (g_taskbarMonitor.mode == TASKBAR_HOST_CLASSIC &&
         (!IsWindow(g_taskbarMonitor.host) ||
          !IsWindow(g_taskbarMonitor.taskList)))) {
        TaskbarMonitor_AttachToTaskbar();
        return;
    }
    RECT rect = {0};
    if (!GetWindowRect(taskbar, &rect)) return;
    UINT dpi = TaskbarMonitor_GetWindowDpi(taskbar);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    BOOL horizontal = width >= height;
    if (dpi != g_taskbarMonitor.dpi ||
        horizontal != g_taskbarMonitor.horizontal ||
        width != g_taskbarMonitor.taskbarWidth ||
        height != g_taskbarMonitor.taskbarHeight) {
        TaskbarMonitor_AttachToTaskbar();
    } else if (g_taskbarMonitor.mode == TASKBAR_HOST_CLASSIC) {
        RECT monitorRect = {0};
        if (!ReserveClassicSlot(&monitorRect) ||
            !PositionClassicMonitor(&monitorRect)) {
            TaskbarMonitor_AttachToTaskbar();
        }
    } else if (!PositionBesideNotificationArea()) {
        TaskbarMonitor_AttachToTaskbar();
    }
}
