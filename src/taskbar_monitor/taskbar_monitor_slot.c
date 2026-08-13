/**
 * @file taskbar_monitor_slot.c
 * @brief Explorer taskbar slot reservation and monitor positioning.
 */

#include "taskbar_monitor_internal.h"

static BOOL WindowRectMatchesParent(
    HWND window, HWND parent, const RECT* expected) {
    RECT current = {0};
    return TaskbarMonitor_IsWindowShown(window) &&
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

BOOL TaskbarMonitor_ReserveClassicSlot(RECT* monitorRect) {
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

BOOL TaskbarMonitor_PositionClassic(const RECT* monitorRect) {
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
        SWP_NOACTIVATE);
}

BOOL TaskbarMonitor_PositionModern(void) {
    RECT bounds = {0};
    RECT notifyRect = {0};
    int gap = TaskbarMonitor_ScaleForDpi(
        TASKBAR_MONITOR_GAP, g_taskbarMonitor.dpi);
    if (!GetClientRect(g_taskbarMonitor.taskbar, &bounds)) {
        return FALSE;
    }
    HWND notify = TaskbarMonitor_FindDescendantByClass(
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
        SWP_NOACTIVATE);
}
