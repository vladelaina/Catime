/**
 * @file taskbar_monitor_lifecycle.c
 * @brief Public taskbar-monitor configuration and window lifecycle.
 */

#include "taskbar_monitor.h"
#include "taskbar_monitor_internal.h"

#include "config.h"
#include "log.h"
#include "system_monitor.h"

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

static BOOL CreateMonitorWindow(void) {
    if (IsWindow(g_taskbarMonitor.window)) return TRUE;
    if (!RegisterMonitorClass()) return FALSE;
    SystemMonitor_Init();
    g_taskbarMonitor.window = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        TASKBAR_MONITOR_CLASS, L"Catime Taskbar Monitor", WS_POPUP,
        0, 0, 1, 1, NULL, NULL, g_taskbarMonitor.instance, NULL);
    if (!g_taskbarMonitor.window) {
        LOG_WARNING("Taskbar monitor window creation failed (error=%lu)",
                    GetLastError());
        return FALSE;
    }
    if (!TaskbarMonitor_AttachToTaskbar()) {
        ShowWindow(g_taskbarMonitor.window, SW_HIDE);
    }
    return TRUE;
}

static void DestroyMonitorWindow(void) {
    TaskbarMonitor_RestoreClassicTaskList();
    if (IsWindow(g_taskbarMonitor.window)) {
        DestroyWindow(g_taskbarMonitor.window);
    }
    g_taskbarMonitor.window = NULL;
    g_taskbarMonitor.taskbar = NULL;
    g_taskbarMonitor.host = NULL;
    g_taskbarMonitor.taskList = NULL;
    g_taskbarMonitor.mode = TASKBAR_HOST_NONE;
    g_taskbarMonitor.compositionMode = TASKBAR_COMPOSITION_UNKNOWN;
    TaskbarMonitor_DeleteFont();
}

void TaskbarMonitor_ApplyConfig(BOOL enabled, BOOL cpuMemoryEnabled,
                                BOOL networkEnabled) {
    BOOL wasEnabled = TaskbarMonitor_IsEnabled();
    g_taskbarMonitor.cpuMemoryEnabled =
        enabled && cpuMemoryEnabled ? TRUE : FALSE;
    g_taskbarMonitor.networkEnabled =
        enabled && networkEnabled ? TRUE : FALSE;
    if (!g_taskbarMonitor.initialized) return;
    if (TaskbarMonitor_IsEnabled()) {
        if (!wasEnabled || !IsWindow(g_taskbarMonitor.window)) {
            CreateMonitorWindow();
        } else {
            TaskbarMonitor_AttachToTaskbar();
            InvalidateRect(g_taskbarMonitor.window, NULL, FALSE);
        }
    } else {
        DestroyMonitorWindow();
    }
}

BOOL TaskbarMonitor_Initialize(HINSTANCE instance, HWND owner) {
    if (!instance || !IsWindow(owner)) return FALSE;
    g_taskbarMonitor.instance = instance;
    g_taskbarMonitor.owner = owner;
    g_taskbarMonitor.initialized = TRUE;
    return !TaskbarMonitor_IsEnabled() || CreateMonitorWindow();
}

void TaskbarMonitor_Shutdown(void) {
    DestroyMonitorWindow();
    if (g_taskbarMonitor.classRegistered && g_taskbarMonitor.instance) {
        UnregisterClassW(TASKBAR_MONITOR_CLASS, g_taskbarMonitor.instance);
    }
    g_taskbarMonitor.classRegistered = FALSE;
    g_taskbarMonitor.initialized = FALSE;
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

BOOL TaskbarMonitor_SetOptionEnabled(TaskbarMonitorOption option,
                                     BOOL enabled) {
    BOOL normalized = enabled ? TRUE : FALSE;
    BOOL cpuMemoryEnabled = g_taskbarMonitor.cpuMemoryEnabled;
    BOOL networkEnabled = g_taskbarMonitor.networkEnabled;
    if (option == TASKBAR_MONITOR_OPTION_CPU_MEMORY) {
        if (normalized == cpuMemoryEnabled) return TRUE;
        cpuMemoryEnabled = normalized;
    } else if (option == TASKBAR_MONITOR_OPTION_NETWORK) {
        if (normalized == networkEnabled) return TRUE;
        networkEnabled = normalized;
    } else {
        return FALSE;
    }
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

void TaskbarMonitor_OnTaskbarCreated(void) {
    if (!g_taskbarMonitor.initialized ||
        !TaskbarMonitor_IsEnabled()) return;
    if (!IsWindow(g_taskbarMonitor.window)) {
        g_taskbarMonitor.window = NULL;
        CreateMonitorWindow();
    } else {
        TaskbarMonitor_AttachToTaskbar();
    }
}

void TaskbarMonitor_Refresh(void) {
    if (!g_taskbarMonitor.initialized ||
        !TaskbarMonitor_IsEnabled()) return;
    if (!IsWindow(g_taskbarMonitor.window)) {
        CreateMonitorWindow();
        return;
    }
    KillTimer(g_taskbarMonitor.window, TASKBAR_MONITOR_THEME_TIMER_ID);
    TaskbarMonitor_AttachToTaskbar();
    InvalidateRect(g_taskbarMonitor.window, NULL, FALSE);
}

static UINT GetThemeSettleDelay(void) {
    BOOL animationsEnabled = TRUE;
    if (SystemParametersInfoW(
            SPI_GETCLIENTAREAANIMATION, 0, &animationsEnabled, 0) &&
        !animationsEnabled) {
        return TASKBAR_MONITOR_THEME_NO_ANIMATION_SETTLE_MS;
    }
    return TASKBAR_MONITOR_THEME_SETTLE_MS;
}

void TaskbarMonitor_RefreshAppearance(void) {
    if (!g_taskbarMonitor.initialized ||
        !TaskbarMonitor_IsEnabled()) return;
    if (!IsWindow(g_taskbarMonitor.window)) {
        CreateMonitorWindow();
        return;
    }
    if (!SetTimer(g_taskbarMonitor.window,
                  TASKBAR_MONITOR_THEME_TIMER_ID,
                  GetThemeSettleDelay(), NULL)) {
        TaskbarMonitor_UpdateThemeState();
        InvalidateRect(g_taskbarMonitor.window, NULL, FALSE);
    }
}
