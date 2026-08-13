/**
 * @file taskbar_monitor_host.c
 * @brief Explorer taskbar discovery, attachment, and slot management.
 */

#include "taskbar_monitor_internal.h"

static BOOL CompleteTaskbarAttachment(void) {
    HWND window = g_taskbarMonitor.window;
    if (!TaskbarMonitor_HasAttachedWindow()) {
        return FALSE;
    }

    HRGN hiddenRegion = CreateRectRgn(0, 0, 0, 0);
    if (!hiddenRegion) return FALSE;
    if (!SetWindowRgn(window, hiddenRegion, FALSE)) {
        DeleteObject(hiddenRegion);
        return FALSE;
    }

    ShowWindow(window, SW_SHOWNOACTIVATE);
    BOOL presented = TaskbarMonitor_PresentCurrentFrame(window);
    BOOL revealed = SetWindowRgn(window, NULL, TRUE) != 0;
    if (!presented || !revealed) {
        ShowWindow(window, SW_HIDE);
        (void)SetWindowRgn(window, NULL, FALSE);
        return FALSE;
    }
    if (!TaskbarMonitor_HasUsableWindow()) return FALSE;
    TaskbarMonitor_CancelWindowRecovery();
    return TRUE;
}

static BOOL AttachModernMonitor(HWND taskbar) {
    if (!TaskbarMonitor_SetWindowParent(
            taskbar, g_taskbarMonitor.modernTaskbar)) return FALSE;
    g_taskbarMonitor.host = taskbar;
    g_taskbarMonitor.taskList = NULL;
    g_taskbarMonitor.mode = TASKBAR_HOST_MODERN;
    return TaskbarMonitor_PositionModern();
}

static void ConfigureHiddenRetry(void) {
    LONG_PTR style = GetWindowLongPtrW(g_taskbarMonitor.window, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtrW(
        g_taskbarMonitor.window, GWL_EXSTYLE);
    SetParent(g_taskbarMonitor.window, NULL);
    SetWindowLongPtrW(g_taskbarMonitor.window, GWL_STYLE,
                      (style & ~WS_CHILD) | WS_POPUP);
    exStyle = (exStyle & ~WS_EX_TOPMOST) |
              WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED |
              WS_EX_TRANSPARENT;
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
    if (!IsWindow(g_taskbarMonitor.window)) {
        TaskbarMonitor_ScheduleWindowRecovery();
        return FALSE;
    }
    if (TaskbarMonitor_IsWindowShown(g_taskbarMonitor.window)) {
        ShowWindow(g_taskbarMonitor.window, SW_HIDE);
    }
    RECT taskbarRect = {0};
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (!taskbar || !GetWindowRect(taskbar, &taskbarRect)) {
        TaskbarMonitor_RestoreClassicTaskList();
        ConfigureHiddenRetry();
        TaskbarMonitor_ScheduleWindowRecovery();
        return FALSE;
    }
    HWND previousTaskbar = g_taskbarMonitor.taskbar;
    HWND previousHost = g_taskbarMonitor.host;
    HWND previousTaskList = g_taskbarMonitor.taskList;
    TaskbarHostMode previousMode = g_taskbarMonitor.mode;
    g_taskbarMonitor.taskbar = taskbar;
    g_taskbarMonitor.dpi = TaskbarMonitor_GetWindowDpi(taskbar);
    TaskbarMonitor_UpdateThemeState();
    /* Resolve the available row height before creating the font. Text width
     * is measured in a second pass after the fitted font is available. */
    TaskbarMonitor_UpdateDimensions(&taskbarRect);
    TaskbarMonitor_RecreateFont();
    TaskbarMonitor_RefreshTextLayout();
    TaskbarMonitor_UpdateDimensions(&taskbarRect);
    BOOL modernTaskbar = TaskbarMonitor_IsModernTaskbar(taskbar);
    g_taskbarMonitor.modernTaskbar = modernTaskbar;
    if (g_taskbarMonitor.menuPreviewSessionActive) {
        /* A recovered preview window must not strand an older classic slot. */
        TaskbarMonitor_RestoreClassicTaskList();
        if (AttachModernMonitor(taskbar) &&
            CompleteTaskbarAttachment()) return TRUE;
        ConfigureHiddenRetry();
        TaskbarMonitor_ScheduleWindowRecovery();
        return FALSE;
    }
    HWND host = TaskbarMonitor_FindDescendantByClass(
        taskbar, L"ReBarWindow32");
    if (!host) {
        host = TaskbarMonitor_FindDescendantByClass(taskbar, L"WorkerW");
    }
    HWND taskList = host
        ? TaskbarMonitor_FindDescendantByClass(
            host, L"MSTaskSwWClass") : NULL;
    if (!taskList && host) {
        taskList = TaskbarMonitor_FindDescendantByClass(
            host, L"MSTaskListWClass");
    }
    if (TaskbarMonitor_ShouldUseClassicPlacement(
            modernTaskbar, host != NULL, taskList != NULL)) {
        RECT monitorRect = {0};
        BOOL sameClassicHost =
            previousMode == TASKBAR_HOST_CLASSIC &&
            previousTaskbar == taskbar && previousHost == host &&
            previousTaskList == taskList;
        if (!sameClassicHost) TaskbarMonitor_RestoreClassicTaskList();
        g_taskbarMonitor.host = host;
        g_taskbarMonitor.taskList = taskList;
        g_taskbarMonitor.mode = TASKBAR_HOST_CLASSIC;
        if (TaskbarMonitor_ReserveClassicSlot(&monitorRect) &&
            TaskbarMonitor_SetWindowParent(host, FALSE) &&
            TaskbarMonitor_PositionClassic(&monitorRect) &&
            CompleteTaskbarAttachment()) return TRUE;
        TaskbarMonitor_RestoreClassicTaskList();
    }
    TaskbarMonitor_RestoreClassicTaskList();
    if (AttachModernMonitor(taskbar) &&
        CompleteTaskbarAttachment()) return TRUE;
    ConfigureHiddenRetry();
    TaskbarMonitor_ScheduleWindowRecovery();
    return FALSE;
}

void TaskbarMonitor_RefreshAttachment(void) {
    if (!IsWindow(g_taskbarMonitor.window)) return;
    /* Attachment may reparent the window or resize Explorer's task list.
     * Defer all such corrections until TrackPopupMenu releases the UI. */
    if (g_taskbarMonitor.menuPreviewSessionActive) return;
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (!taskbar) {
        ShowWindow(g_taskbarMonitor.window, SW_HIDE);
        TaskbarMonitor_ScheduleWindowRecovery();
        return;
    }
    if (taskbar != g_taskbarMonitor.taskbar ||
        !TaskbarMonitor_HasUsableWindow() ||
        TaskbarMonitor_IsModernTaskbar(taskbar) !=
            g_taskbarMonitor.modernTaskbar) {
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
        if (!TaskbarMonitor_ReserveClassicSlot(&monitorRect) ||
            !TaskbarMonitor_PositionClassic(&monitorRect)) {
            TaskbarMonitor_AttachToTaskbar();
        }
    } else if (!TaskbarMonitor_PositionModern()) {
        TaskbarMonitor_AttachToTaskbar();
    }
}
