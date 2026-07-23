/**
 * @file plugin_process.c
 * @brief Public plugin launch and process-state API.
 */

#include "plugin_process_internal.h"

static BOOL IsLaunchStartFailureCoolingDown(DWORD now) {
    return g_pluginLaunchFailureCooldownUntil != 0 &&
           (LONG)(g_pluginLaunchFailureCooldownUntil - now) > 0;
}

static void MarkLaunchStartFailure(DWORD now) {
    DWORD until = now + PLUGIN_LAUNCH_START_FAILURE_COOLDOWN_MS;
    g_pluginLaunchFailureCooldownUntil = until ? until : 1;
}

BOOL PluginProcess_Launch(PluginInfo* plugin) {
    if (!plugin) return FALSE;
    PluginProcess_SetLastError(NULL);
    DWORD now = GetTickCount();
    if (IsLaunchStartFailureCoolingDown(now)) {
        wcscpy_s(g_pluginLastLaunchError, _countof(g_pluginLastLaunchError),
                 L"Internal error");
        return FALSE;
    }

    PluginLauncherArgs* args = (PluginLauncherArgs*)calloc(1, sizeof(*args));
    if (!args) {
        LOG_ERROR("[Process] Failed to allocate launcher args");
        MarkLaunchStartFailure(now);
        wcscpy_s(g_pluginLastLaunchError, _countof(g_pluginLastLaunchError),
                 L"Internal error");
        return FALSE;
    }
    args->pluginSnapshot = *plugin;
    args->readyState = PLUGIN_LAUNCH_READY_PENDING;
    args->refCount = 1;

    HANDLE readyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!readyEvent) {
        free(args);
        MarkLaunchStartFailure(now);
        wcscpy_s(g_pluginLastLaunchError, _countof(g_pluginLastLaunchError),
                 L"Internal error");
        return FALSE;
    }
    if (g_pluginJob && !DuplicateHandle(
            GetCurrentProcess(), g_pluginJob, GetCurrentProcess(),
            &args->hJob, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        LOG_ERROR("[Process] Failed to duplicate Job Object handle: %lu",
                  GetLastError());
        CloseHandle(readyEvent);
        free(args);
        MarkLaunchStartFailure(now);
        wcscpy_s(g_pluginLastLaunchError, _countof(g_pluginLastLaunchError),
                 L"Internal error");
        return FALSE;
    }
    if (!DuplicateHandle(GetCurrentProcess(), readyEvent,
                         GetCurrentProcess(), &args->hReadySignalEvent,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
        LOG_ERROR("[Process] Failed to duplicate sync event handle: %lu",
                  GetLastError());
        ClosePluginLaunchJobHandle(args);
        CloseHandle(readyEvent);
        free(args);
        MarkLaunchStartFailure(now);
        wcscpy_s(g_pluginLastLaunchError, _countof(g_pluginLastLaunchError),
                 L"Internal error");
        return FALSE;
    }

    InterlockedIncrement(&args->refCount);
    HANDLE launcherThread = CreateThread(NULL, 0, PluginLauncherThread,
                                         args, 0, NULL);
    if (!launcherThread) {
        ClosePluginLaunchJobHandle(args);
        CloseHandle(readyEvent);
        ReleaseAbandonedPluginLaunchArgs(args);
        ReleaseAbandonedPluginLaunchArgs(args);
        MarkLaunchStartFailure(now);
        wcscpy_s(g_pluginLastLaunchError, _countof(g_pluginLastLaunchError),
                 L"Internal error");
        return FALSE;
    }
    g_pluginLaunchFailureCooldownUntil = 0;

    DWORD waitResult = WaitForSingleObject(readyEvent,
                                           PLUGIN_LAUNCH_READY_TIMEOUT_MS);
    CloseHandle(readyEvent);
    readyEvent = NULL;
    if (waitResult != WAIT_OBJECT_0) {
        LONG previous = InterlockedCompareExchange(
            &args->readyState, PLUGIN_LAUNCH_READY_ABANDONED,
            PLUGIN_LAUNCH_READY_PENDING);
        if (previous == PLUGIN_LAUNCH_READY_PENDING) {
            if (waitResult == WAIT_TIMEOUT) {
                LOG_ERROR("[Process] Plugin launch timed out: %ls",
                          plugin->displayName);
                wcscpy_s(g_pluginLastLaunchError,
                         _countof(g_pluginLastLaunchError), L"Launch timed out");
            } else {
                LOG_ERROR("[Process] Plugin launch wait failed (wait=%lu, error=%lu)",
                          waitResult, GetLastError());
                wcscpy_s(g_pluginLastLaunchError,
                         _countof(g_pluginLastLaunchError), L"Launch failed");
            }
            CloseHandle(launcherThread);
            ReleaseAbandonedPluginLaunchArgs(args);
            return FALSE;
        }
        if (previous != PLUGIN_LAUNCH_READY_SIGNALED) {
            CloseHandle(launcherThread);
            ReleaseAbandonedPluginLaunchArgs(args);
            return FALSE;
        }
    }

    if (args->success && args->pluginSnapshot.isRunning &&
        args->pluginSnapshot.pi.dwProcessId != 0) {
        *plugin = args->pluginSnapshot;
        plugin->pi.hThread = launcherThread;
    } else {
        DWORD threadWait = WaitForSingleObject(
            launcherThread, PLUGIN_LAUNCH_FAILURE_THREAD_WAIT_MS);
        if (threadWait == WAIT_TIMEOUT) {
            LOG_WARNING("[Process] Launcher thread did not exit after failed launch within %lu ms",
                        (DWORD)PLUGIN_LAUNCH_FAILURE_THREAD_WAIT_MS);
        } else if (threadWait == WAIT_FAILED) {
            LOG_WARNING("[Process] Failed waiting for launcher thread after failed launch: %lu",
                        GetLastError());
        }
        CloseHandle(launcherThread);
    }
    if (!args->success && args->errorMsg[0]) {
        wcscpy_s(g_pluginLastLaunchError,
                 _countof(g_pluginLastLaunchError), args->errorMsg);
    }
    BOOL success = args->success;
    ReleaseAbandonedPluginLaunchArgs(args);
    return success;
}

BOOL PluginProcess_TerminateDetached(PluginInfo* plugin) {
    if (!plugin || !plugin->isRunning) return FALSE;
    HANDLE process = plugin->pi.hProcess;
    HANDLE thread = plugin->pi.hThread;
    DWORD pid = plugin->pi.dwProcessId;
    plugin->isRunning = FALSE;
    memset(&plugin->pi, 0, sizeof(plugin->pi));
    if (pid) PluginProcess_TerminateTree(pid, 0);
    if (process) {
        TerminateProcess(process, 0);
        WaitForSingleObject(process, 2000);
        CloseHandle(process);
    }
    if (thread) PluginProcess_CloseMonitorThreadHandle(thread, TRUE);
    return TRUE;
}

void PluginProcess_TerminateAllOrphans(void) {
    PluginProcess_TerminateAllJobProcesses();
}

BOOL PluginProcess_IsAlive(PluginInfo* plugin) {
    if (!plugin || !plugin->isRunning) return FALSE;
    HANDLE process = plugin->pi.hProcess;
    if (!process) return plugin->isRunning;
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(process, &exitCode)) {
        LOG_WARNING("[Process] GetExitCodeProcess failed for plugin pid %lu: %lu",
                    plugin->pi.dwProcessId, GetLastError());
        if (InterlockedCompareExchange(
                (volatile LONG*)&plugin->isRunning, FALSE, TRUE) == TRUE) {
            PluginProcess_ClearHandles(plugin, FALSE);
        }
        return FALSE;
    }
    if (exitCode != STILL_ACTIVE) {
        if (InterlockedCompareExchange(
                (volatile LONG*)&plugin->isRunning, FALSE, TRUE) == TRUE) {
            PluginProcess_ClearHandles(plugin, FALSE);
        }
        return FALSE;
    }
    return TRUE;
}
