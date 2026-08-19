/**
 * @file plugin_process_state.c
 * @brief Job handle, monitor-window, and launch-error state.
 */

#include "plugin_process_internal.h"

HANDLE g_pluginJob = NULL;
HWND g_pluginNotifyWindow = NULL;
wchar_t g_pluginLastLaunchError[128] = {0};
DWORD g_pluginLaunchFailureCooldownUntil = 0;

void PluginProcess_CloseMonitorThreadHandle(HANDLE hThread, BOOL waitForExit) {
    if (!hThread) return;
    if (waitForExit) {
        DWORD threadId = GetThreadId(hThread);
        if (threadId && threadId != GetCurrentThreadId()) {
            DWORD result = WaitForSingleObject(hThread, 250);
            if (result == WAIT_TIMEOUT) {
                LOG_WARNING("[Process] Monitor thread did not exit before handle close");
            } else if (result == WAIT_FAILED) {
                LOG_WARNING("[Process] Failed waiting for monitor thread: %lu",
                            GetLastError());
            }
        }
    } else if (WaitForSingleObject(hThread, 0) == WAIT_TIMEOUT) {
        LOG_DEBUG("[Process] Closing monitor thread handle before thread has fully returned");
    }
    CloseHandle(hThread);
}

void PluginProcess_ClearHandles(PluginInfo* plugin,
                                BOOL waitForMonitorThread) {
    if (!plugin) return;
    HANDLE process = InterlockedExchangePointer(
        (PVOID*)&plugin->pi.hProcess, NULL);
    HANDLE thread = InterlockedExchangePointer(
        (PVOID*)&plugin->pi.hThread, NULL);
    if (process) CloseHandle(process);
    if (thread) PluginProcess_CloseMonitorThreadHandle(
        thread, waitForMonitorThread);
    memset(&plugin->pi, 0, sizeof(plugin->pi));
}

BOOL IsValidPluginNotifyWindow(HWND window) {
    if (!window || !IsWindow(window)) return FALSE;
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId != GetCurrentProcessId()) return FALSE;
    wchar_t className[64] = {0};
    if (!GetClassNameW(window, className, _countof(className))) return FALSE;
    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

const wchar_t* PluginProcess_GetLastError(void) {
    return g_pluginLastLaunchError;
}

void PluginProcess_SetLastError(const wchar_t* errorMsg) {
    if (!errorMsg) {
        g_pluginLastLaunchError[0] = L'\0';
        return;
    }
    wcsncpy(g_pluginLastLaunchError, errorMsg,
            _countof(g_pluginLastLaunchError) - 1);
    g_pluginLastLaunchError[_countof(g_pluginLastLaunchError) - 1] = L'\0';
}

void PluginProcess_SetNotifyWindow(HWND hwnd) {
    g_pluginNotifyWindow = IsValidPluginNotifyWindow(hwnd) ? hwnd : NULL;
}

HWND PluginProcess_GetNotifyWindow(void) {
    if (!IsValidPluginNotifyWindow(g_pluginNotifyWindow)) {
        g_pluginNotifyWindow = NULL;
    }
    return g_pluginNotifyWindow;
}

BOOL PluginProcess_Init(void) {
    g_pluginNotifyWindow = NULL;
    g_pluginJob = CreateJobObject(NULL, NULL);
    if (!g_pluginJob) {
        LOG_ERROR("[Process] Failed to create Job Object, error: %lu",
                  GetLastError());
        return FALSE;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(g_pluginJob,
                                 JobObjectExtendedLimitInformation,
                                 &limits, sizeof(limits))) {
        LOG_ERROR("[Process] Failed to configure Job Object, error: %lu",
                  GetLastError());
        CloseHandle(g_pluginJob);
        g_pluginJob = NULL;
        return FALSE;
    }
    return TRUE;
}

void PluginProcess_Shutdown(void) {
    g_pluginNotifyWindow = NULL;
    if (g_pluginJob) {
        CloseHandle(g_pluginJob);
        g_pluginJob = NULL;
    }
}
