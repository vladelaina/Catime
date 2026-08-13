/**
 * @file taskbar_monitor_recovery.c
 * @brief Recovery after Explorer unexpectedly destroys the monitor window.
 */

#include "taskbar_monitor.h"
#include "taskbar_monitor_internal.h"

#include "log.h"

BOOL TaskbarMonitor_HasAttachedWindow(void) {
    if (!IsWindow(g_taskbarMonitor.window) ||
        g_taskbarMonitor.mode == TASKBAR_HOST_NONE ||
        !IsWindow(g_taskbarMonitor.taskbar) ||
        !IsWindow(g_taskbarMonitor.host)) {
        return FALSE;
    }
    HWND expectedParent = g_taskbarMonitor.mode == TASKBAR_HOST_CLASSIC
        ? g_taskbarMonitor.host : g_taskbarMonitor.taskbar;
    if (GetAncestor(g_taskbarMonitor.window, GA_PARENT) != expectedParent) {
        return FALSE;
    }
    LONG_PTR style = GetWindowLongPtrW(
        g_taskbarMonitor.window, GWL_STYLE);
    BOOL childExpected = g_taskbarMonitor.mode == TASKBAR_HOST_MODERN &&
                         g_taskbarMonitor.modernTaskbar;
    if (((style & WS_CHILD) != 0) != childExpected ||
        (!childExpected && (style & WS_POPUP) == 0)) {
        return FALSE;
    }
    RECT client = {0};
    if (!GetClientRect(g_taskbarMonitor.window, &client) ||
        client.right <= client.left || client.bottom <= client.top) {
        return FALSE;
    }
    if (g_taskbarMonitor.mode == TASKBAR_HOST_MODERN) {
        return g_taskbarMonitor.host == g_taskbarMonitor.taskbar;
    }
    return IsWindow(g_taskbarMonitor.taskList) &&
           g_taskbarMonitor.taskListReserved;
}

BOOL TaskbarMonitor_HasUsableWindow(void) {
    return TaskbarMonitor_IsWindowShown(g_taskbarMonitor.window) &&
           TaskbarMonitor_HasAttachedWindow();
}

static BOOL CanRecoverMonitorWindow(void) {
    return g_taskbarMonitor.initialized &&
           TaskbarMonitor_IsEnabled() &&
           !g_taskbarMonitor.windowDestroyExpected &&
           !g_taskbarMonitor.menuPreviewSessionActive &&
           IsWindow(g_taskbarMonitor.owner) &&
           !TaskbarMonitor_HasUsableWindow();
}

void TaskbarMonitor_CancelWindowRecovery(void) {
    if (!g_taskbarMonitor.recoveryTimerId) return;
    KillTimer(NULL, g_taskbarMonitor.recoveryTimerId);
    g_taskbarMonitor.recoveryTimerId = 0;
}

static VOID CALLBACK RecoverMonitorWindowTimerProc(
    HWND window, UINT message, UINT_PTR timerId, DWORD tick) {
    (void)window;
    (void)message;
    (void)tick;
    KillTimer(NULL, timerId);
    if (g_taskbarMonitor.recoveryTimerId != timerId) return;
    g_taskbarMonitor.recoveryTimerId = 0;
    if (!CanRecoverMonitorWindow()) return;
    BOOL recovered = IsWindow(g_taskbarMonitor.window)
        ? TaskbarMonitor_AttachToTaskbar()
        : TaskbarMonitor_CreateWindow();
    if (!recovered || !TaskbarMonitor_HasUsableWindow()) {
        TaskbarMonitor_ScheduleWindowRecovery();
    }
}

void TaskbarMonitor_ScheduleWindowRecovery(void) {
    if (g_taskbarMonitor.recoveryTimerId ||
        !CanRecoverMonitorWindow()) return;
    g_taskbarMonitor.recoveryTimerId = SetTimer(
        NULL, 0, TASKBAR_MONITOR_RECOVERY_MS,
        RecoverMonitorWindowTimerProc);
    if (!g_taskbarMonitor.recoveryTimerId) {
        LOG_WARNING("Failed to schedule taskbar monitor recovery (error=%lu)",
                    GetLastError());
    }
}

void TaskbarMonitor_OnMonitorWindowDestroyed(HWND window) {
    if (g_taskbarMonitor.window != window) return;
    TaskbarMonitor_RestoreClassicTaskList();
    g_taskbarMonitor.window = NULL;
    g_taskbarMonitor.taskbar = NULL;
    g_taskbarMonitor.host = NULL;
    g_taskbarMonitor.taskList = NULL;
    g_taskbarMonitor.modernTaskbar = FALSE;
    g_taskbarMonitor.presentTimerActive = FALSE;
    g_taskbarMonitor.mode = TASKBAR_HOST_NONE;
    g_taskbarMonitor.compositionMode = TASKBAR_COMPOSITION_UNKNOWN;
    g_taskbarMonitor.themeRecheckDueTick = 0;
    TaskbarMonitor_ScheduleWindowRecovery();
}
