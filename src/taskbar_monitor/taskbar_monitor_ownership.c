/**
 * @file taskbar_monitor_ownership.c
 * @brief Cross-process ownership for the shared taskbar monitor surface.
 */

#include "taskbar_monitor_internal.h"

#include "log.h"

#define TASKBAR_MONITOR_OWNER_MUTEX_NAME \
    L"Local\\Vladelaina.Catime.TaskbarMonitorOwner"

static HANDLE g_taskbarMonitorOwnerMutex = NULL;
static BOOL g_taskbarMonitorOwnsDisplay = FALSE;

static BOOL IsOtherProcessWindow(HWND window) {
    if (!window || !IsWindow(window)) return FALSE;
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    return processId != 0 && processId != GetCurrentProcessId();
}

static BOOL HasLegacyTaskbarMonitorWindow(void) {
    HWND window = NULL;
    while ((window = FindWindowExW(
                NULL, window, TASKBAR_MONITOR_CLASS, NULL)) != NULL) {
        if (IsOtherProcessWindow(window)) return TRUE;
    }

    HWND taskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    window = TaskbarMonitor_FindDescendantByClass(
        taskbar, TASKBAR_MONITOR_CLASS);
    return IsOtherProcessWindow(window);
}

BOOL TaskbarMonitor_TryAcquireOwnership(void) {
    if (g_taskbarMonitorOwnsDisplay && g_taskbarMonitorOwnerMutex) {
        return TRUE;
    }

    HANDLE mutex = CreateMutexW(
        NULL, FALSE, TASKBAR_MONITOR_OWNER_MUTEX_NAME);
    if (!mutex) {
        LOG_WARNING("Failed to create taskbar monitor ownership mutex (error=%lu)",
                    GetLastError());
        return FALSE;
    }

    DWORD waitResult = WaitForSingleObject(mutex, 0);
    if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED) {
        if (waitResult != WAIT_TIMEOUT) {
            LOG_WARNING("Failed to acquire taskbar monitor ownership (result=%lu, error=%lu)",
                        waitResult, GetLastError());
        }
        CloseHandle(mutex);
        return FALSE;
    }

    g_taskbarMonitorOwnerMutex = mutex;
    g_taskbarMonitorOwnsDisplay = TRUE;
    if (HasLegacyTaskbarMonitorWindow()) {
        TaskbarMonitor_ReleaseOwnership();
        return FALSE;
    }
    return TRUE;
}

void TaskbarMonitor_ReleaseOwnership(void) {
    HANDLE mutex = g_taskbarMonitorOwnerMutex;
    BOOL ownsDisplay = g_taskbarMonitorOwnsDisplay;
    g_taskbarMonitorOwnerMutex = NULL;
    g_taskbarMonitorOwnsDisplay = FALSE;
    if (!mutex) return;
    if (ownsDisplay && !ReleaseMutex(mutex)) {
        LOG_WARNING("Failed to release taskbar monitor ownership (error=%lu)",
                    GetLastError());
    }
    CloseHandle(mutex);
}
