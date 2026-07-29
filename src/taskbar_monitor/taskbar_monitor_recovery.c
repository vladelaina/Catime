/**
 * @file taskbar_monitor_recovery.c
 * @brief Recovery after Explorer unexpectedly destroys the monitor window.
 */

#include "taskbar_monitor.h"
#include "taskbar_monitor_internal.h"

#include "log.h"

static BOOL CanRecoverMonitorWindow(void) {
    return g_taskbarMonitor.initialized &&
           TaskbarMonitor_IsEnabled() &&
           !g_taskbarMonitor.windowDestroyExpected &&
           !g_taskbarMonitor.menuPreviewSessionActive &&
           IsWindow(g_taskbarMonitor.owner) &&
           !IsWindow(g_taskbarMonitor.window);
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
    if (!TaskbarMonitor_CreateWindow() ||
        !IsWindow(g_taskbarMonitor.window)) {
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
    g_taskbarMonitor.window = NULL;
    TaskbarMonitor_ScheduleWindowRecovery();
}
