/**
 * @file taskbar_monitor_lifecycle.c
 * @brief Public taskbar-monitor configuration and window lifecycle.
 */

#include "taskbar_monitor.h"
#include "taskbar_monitor_internal.h"

#include "config.h"
#include "log.h"
#include "system_monitor.h"
#include "tray/tray_theme_state.h"

static BOOL RegisterMonitorClass(void) {
    if (g_taskbarMonitor.classRegistered) return TRUE;
    WNDCLASSW windowClass = {0};
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    windowClass.lpfnWndProc = TaskbarMonitorWindowProc;
    windowClass.hInstance = g_taskbarMonitor.instance;
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.lpszClassName = TASKBAR_MONITOR_CLASS;
    if (!RegisterClassW(&windowClass)) {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            LOG_WARNING("Taskbar monitor class registration failed (error=%lu)",
                        error);
            return FALSE;
        }
    }
    g_taskbarMonitor.classRegistered = TRUE;
    return TRUE;
}
BOOL TaskbarMonitor_CreateWindow(void) {
    TaskbarMonitor_CancelWindowRecovery();
    if (IsWindow(g_taskbarMonitor.window)) return TRUE;
    if (!RegisterMonitorClass()) {
        TaskbarMonitor_ScheduleWindowRecovery();
        return FALSE;
    }
    if (TaskbarMonitor_IsEnabled()) SystemMonitor_Init();
    g_taskbarMonitor.window = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        TASKBAR_MONITOR_CLASS, L"Catime Taskbar Monitor", WS_POPUP,
        0, 0, 1, 1, NULL, NULL, g_taskbarMonitor.instance, NULL);
    if (!g_taskbarMonitor.window) {
        LOG_WARNING("Taskbar monitor window creation failed (error=%lu)",
                    GetLastError());
        TaskbarMonitor_ScheduleWindowRecovery();
        return FALSE;
    }
    if (!TaskbarMonitor_AttachToTaskbar()) {
        ShowWindow(g_taskbarMonitor.window, SW_HIDE);
    }
    if (!TaskbarMonitor_HasUsableWindow()) {
        TaskbarMonitor_ScheduleWindowRecovery();
    }
    return TRUE;
}
static void DestroyMonitorWindow(void) {
    TaskbarMonitor_CancelWindowRecovery();
    g_taskbarMonitor.windowDestroyExpected = TRUE;
    TaskbarMonitor_RestoreClassicTaskList();
    if (IsWindow(g_taskbarMonitor.window)) {
        DestroyWindow(g_taskbarMonitor.window);
    }
    g_taskbarMonitor.windowDestroyExpected = FALSE;
    g_taskbarMonitor.window = NULL;
    g_taskbarMonitor.taskbar = NULL;
    g_taskbarMonitor.host = NULL;
    g_taskbarMonitor.taskList = NULL;
    g_taskbarMonitor.modernTaskbar = FALSE;
    g_taskbarMonitor.presentTimerActive = FALSE;
    g_taskbarMonitor.mode = TASKBAR_HOST_NONE;
    g_taskbarMonitor.compositionMode = TASKBAR_COMPOSITION_UNKNOWN;
    g_taskbarMonitor.themeRecheckDueTick = 0;
    TaskbarMonitor_ResetMenuPreviewWindowGeometry();
    TaskbarMonitor_DeleteFont();
}

static void SyncMonitorWindow(BOOL reattachExisting) {
    if (TaskbarMonitor_IsEnabled()) {
        if (!IsWindow(g_taskbarMonitor.window)) {
            TaskbarMonitor_CreateWindow();
        } else if (reattachExisting ||
                   !TaskbarMonitor_HasUsableWindow()) {
            TaskbarMonitor_AttachToTaskbar();
            InvalidateRect(g_taskbarMonitor.window, NULL, FALSE);
        }
    } else if (IsWindow(g_taskbarMonitor.window) ||
               g_taskbarMonitor.taskListReserved) {
        DestroyMonitorWindow();
    }
}

BOOL TaskbarMonitor_BeginMenuPreviewSession(void) {
    if (!g_taskbarMonitor.initialized) return FALSE;
    if (g_taskbarMonitor.menuPreviewSessionActive) {
        return IsWindow(g_taskbarMonitor.window);
    }

    g_taskbarMonitor.menuPreviewOriginalCpuMemoryEnabled =
        g_taskbarMonitor.cpuMemoryEnabled;
    g_taskbarMonitor.menuPreviewOriginalNetworkEnabled =
        g_taskbarMonitor.networkEnabled;
    g_taskbarMonitor.menuPreviewWindowCreated =
        !IsWindow(g_taskbarMonitor.window);
    TaskbarMonitor_ResetMenuPreviewWindowGeometry();
    g_taskbarMonitor.menuPreviewSessionActive = TRUE;
    TaskbarMonitor_PrefetchSnapshot(TaskbarMonitor_GetRequiredSnapshotFields(), TRUE);
    if (g_taskbarMonitor.menuPreviewWindowCreated) {
        g_taskbarMonitor.window = NULL;
        (void)TaskbarMonitor_CreateWindow();
    } else if (!TaskbarMonitor_HasUsableWindow()) {
        (void)TaskbarMonitor_AttachToTaskbar();
    }
    if (IsWindow(g_taskbarMonitor.window)) {
        (void)TaskbarMonitor_SetMenuPreviewPassThrough(TRUE);
    }
    (void)TaskbarMonitor_CaptureMenuPreviewWindowGeometry();
    return IsWindow(g_taskbarMonitor.window);
}

void TaskbarMonitor_EndMenuPreviewSession(void) {
    if (!g_taskbarMonitor.menuPreviewSessionActive) return;
    BOOL configurationChanged =
        g_taskbarMonitor.cpuMemoryEnabled !=
            g_taskbarMonitor.menuPreviewOriginalCpuMemoryEnabled ||
        g_taskbarMonitor.networkEnabled !=
            g_taskbarMonitor.menuPreviewOriginalNetworkEnabled;
    BOOL hostNeedsSync =
        g_taskbarMonitor.menuPreviewWindowCreated || configurationChanged ||
        (TaskbarMonitor_IsEnabled() &&
         !TaskbarMonitor_HasUsableWindow()) ||
        (TaskbarMonitor_IsEnabled() !=
         IsWindow(g_taskbarMonitor.window));
    if (!hostNeedsSync && g_taskbarMonitor.menuPreviewWindowResized &&
        !TaskbarMonitor_RestoreMenuPreviewWindowGeometry()) {
        hostNeedsSync = TRUE;
    }
    g_taskbarMonitor.menuPreviewSessionActive = FALSE;
    g_taskbarMonitor.menuPreviewWindowCreated = FALSE;
    if (hostNeedsSync) {
        SyncMonitorWindow(TRUE);
    } else {
        TaskbarMonitor_ClearMenuPreviewWindowInteraction();
    }
    TaskbarMonitor_ResetMenuPreviewWindowGeometry();
}

void TaskbarMonitor_ApplyConfig(BOOL enabled, BOOL cpuMemoryEnabled,
                                BOOL networkEnabled) {
    BOOL nextCpuMemoryEnabled =
        enabled && cpuMemoryEnabled ? TRUE : FALSE;
    BOOL nextNetworkEnabled =
        enabled && networkEnabled ? TRUE : FALSE;
    BOOL configurationChanged =
        g_taskbarMonitor.cpuMemoryEnabled != nextCpuMemoryEnabled ||
        g_taskbarMonitor.networkEnabled != nextNetworkEnabled;
    g_taskbarMonitor.cpuMemoryEnabled = nextCpuMemoryEnabled;
    g_taskbarMonitor.networkEnabled = nextNetworkEnabled;
    if (!g_taskbarMonitor.initialized) return;
    if (g_taskbarMonitor.menuPreviewSessionActive) {
        /* Resize only Catime's prepared window. Explorer task-list reservation
         * stays unchanged while TrackPopupMenu owns the UI thread. */
        TaskbarMonitor_PrefetchSnapshot(TaskbarMonitor_GetRequiredSnapshotFields(), FALSE);
        if (IsWindow(g_taskbarMonitor.window)) {
            (void)TaskbarMonitor_UpdateMenuPreviewWindowGeometry();
        }
        if (configurationChanged && IsWindow(g_taskbarMonitor.window)) {
            InvalidateRect(g_taskbarMonitor.window, NULL, FALSE);
        }
        return;
    }
    SyncMonitorWindow(configurationChanged);
}

BOOL TaskbarMonitor_Initialize(HINSTANCE instance, HWND owner) {
    if (!instance || !IsWindow(owner)) return FALSE;
    g_taskbarMonitor.instance = instance;
    g_taskbarMonitor.owner = owner;
    g_taskbarMonitor.initialized = TRUE;
    return !TaskbarMonitor_IsEnabled() || TaskbarMonitor_CreateWindow();
}

void TaskbarMonitor_Shutdown(void) {
    g_taskbarMonitor.initialized = FALSE;
    DestroyMonitorWindow();
    if (g_taskbarMonitor.classRegistered && g_taskbarMonitor.instance) {
        UnregisterClassW(TASKBAR_MONITOR_CLASS, g_taskbarMonitor.instance);
    }
    g_taskbarMonitor.classRegistered = FALSE;
    g_taskbarMonitor.menuPreviewSessionActive = FALSE;
    g_taskbarMonitor.menuPreviewWindowCreated = FALSE;
    TaskbarMonitor_ResetMenuPreviewWindowGeometry();
    g_taskbarMonitor.owner = NULL;
    g_taskbarMonitor.instance = NULL;
}

BOOL TaskbarMonitor_IsEnabled(void) {
    return g_taskbarMonitor.cpuMemoryEnabled ||
           g_taskbarMonitor.networkEnabled;
}

BOOL TaskbarMonitor_IsOptionEnabled(TaskbarMonitorOption option) {
    if (option == TASKBAR_MONITOR_OPTION_CPU_MEMORY) {
        return g_taskbarMonitor.cpuMemoryEnabled;
    }
    if (option == TASKBAR_MONITOR_OPTION_NETWORK) {
        return g_taskbarMonitor.networkEnabled;
    }
    return FALSE;
}

static BOOL PersistTaskbarMonitorOptions(BOOL cpuMemoryEnabled,
                                         BOOL networkEnabled) {
    const IniKeyValue updates[] = {
        {"Animation", "TASKBAR_MONITOR_ENABLED",
         cpuMemoryEnabled || networkEnabled ? "TRUE" : "FALSE"},
        {"Animation", "TASKBAR_MONITOR_CPU_MEMORY",
         cpuMemoryEnabled ? "TRUE" : "FALSE"},
        {"Animation", "TASKBAR_MONITOR_NETWORK",
         networkEnabled ? "TRUE" : "FALSE"}
    };
    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, _countof(configPath));
    if (!configPath[0] || !WriteIniMultipleAtomic(
            configPath, updates, _countof(updates))) {
        LOG_WARNING("Failed to save the taskbar monitor preferences");
        return FALSE;
    }
    TaskbarMonitor_ApplyConfig(TRUE, cpuMemoryEnabled, networkEnabled);
    return TRUE;
}

BOOL TaskbarMonitor_SetOptionEnabled(TaskbarMonitorOption option,
                                     BOOL enabled) {
    BOOL normalized = enabled ? TRUE : FALSE;
    BOOL cpuMemoryEnabled = g_taskbarMonitor.cpuMemoryEnabled;
    BOOL networkEnabled = g_taskbarMonitor.networkEnabled;
    if (option == TASKBAR_MONITOR_OPTION_CPU_MEMORY) {
        cpuMemoryEnabled = normalized;
    } else if (option == TASKBAR_MONITOR_OPTION_NETWORK) {
        networkEnabled = normalized;
    } else {
        return FALSE;
    }
    return PersistTaskbarMonitorOptions(cpuMemoryEnabled, networkEnabled);
}

void TaskbarMonitor_OnTaskbarCreated(void) {
    if (!g_taskbarMonitor.initialized ||
        !TaskbarMonitor_IsEnabled()) return;
    if (g_taskbarMonitor.menuPreviewSessionActive) return;
    if (!IsWindow(g_taskbarMonitor.window)) {
        g_taskbarMonitor.window = NULL;
        TaskbarMonitor_CreateWindow();
    } else {
        TaskbarMonitor_AttachToTaskbar();
    }
}

void TaskbarMonitor_Refresh(void) {
    if (!g_taskbarMonitor.initialized ||
        !TaskbarMonitor_IsEnabled()) return;
    if (g_taskbarMonitor.menuPreviewSessionActive) {
        if (IsWindow(g_taskbarMonitor.window)) {
            InvalidateRect(g_taskbarMonitor.window, NULL, FALSE);
        }
        return;
    }
    if (!IsWindow(g_taskbarMonitor.window)) {
        TaskbarMonitor_CreateWindow();
        return;
    }
    KillTimer(g_taskbarMonitor.window,
              TASKBAR_MONITOR_THEME_RECHECK_TIMER_ID);
    g_taskbarMonitor.themeRecheckDueTick = 0;
    TaskbarMonitor_AttachToTaskbar();
    InvalidateRect(g_taskbarMonitor.window, NULL, FALSE);
}

void TaskbarMonitor_RefreshAppearance(void) {
    if (!g_taskbarMonitor.initialized ||
        !TaskbarMonitor_IsEnabled()) return;
    if (!IsWindow(g_taskbarMonitor.window)) {
        TaskbarMonitor_CreateWindow();
        return;
    }
    TaskbarMonitor_UpdateThemeState();
    InvalidateRect(g_taskbarMonitor.window, NULL, FALSE);
    UINT delay = GetSystemThemeRecheckDelay();
    g_taskbarMonitor.themeRecheckDueTick = GetTickCount() + delay;
    if (!SetTimer(g_taskbarMonitor.window,
                  TASKBAR_MONITOR_THEME_RECHECK_TIMER_ID,
                  delay, NULL)) {
        g_taskbarMonitor.themeRecheckDueTick = 0;
        TaskbarMonitor_UpdateThemeState();
        InvalidateRect(g_taskbarMonitor.window, NULL, FALSE);
    }
}
