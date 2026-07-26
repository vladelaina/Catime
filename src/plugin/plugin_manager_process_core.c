/**
 * @file plugin_manager_process_core.c
 * @brief Plugin process detachment and prepared launch mechanics.
 */

#include "plugin_manager_internal.h"

int PluginManager_GetPluginCount(void) {
    if (!g_pluginManagerInitialized) return 0;
    int count;
    EnterCriticalSection(&g_pluginCS);
    count = g_pluginCount;
    LeaveCriticalSection(&g_pluginCS);
    return count;
}

BOOL PluginManager_CopyPlugin(int index, PluginInfo* outPlugin) {
    if (!g_pluginManagerInitialized || !outPlugin) {
        return FALSE;
    }

    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    *outPlugin = g_plugins[index];
    memset(&outPlugin->pi, 0, sizeof(outPlugin->pi));
    LeaveCriticalSection(&g_pluginCS);
    return TRUE;
}

BOOL DetachPluginProcessLocked(int index, PluginInfo* detachedPlugin) {
    if (!detachedPlugin || index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    PluginInfo* plugin = &g_plugins[index];
    if (!plugin->isRunning) {
        return FALSE;
    }

    *detachedPlugin = *plugin;
    plugin->isRunning = FALSE;
    memset(&plugin->pi, 0, sizeof(plugin->pi));

    return TRUE;
}

int DetachAllRunningPluginProcessesLocked(PluginInfo* detachedPlugins, int capacity) {
    if (!detachedPlugins || capacity <= 0) {
        return 0;
    }

    int detachedCount = 0;
    for (int i = 0; i < g_pluginCount && detachedCount < capacity; i++) {
        if (DetachPluginProcessLocked(i, &detachedPlugins[detachedCount])) {
            detachedCount++;
        }
    }

    return detachedCount;
}

PluginInfo* AllocatePluginSnapshotArray(void) {
    return (PluginInfo*)calloc(MAX_PLUGINS, sizeof(PluginInfo));
}

int DetachAndTerminateRunningPluginsIndividually(int skipIndex) {
    int detachedCount = 0;

    for (;;) {
        PluginInfo detachedPlugin;
        memset(&detachedPlugin, 0, sizeof(detachedPlugin));
        BOOL detached = FALSE;

        EnterCriticalSection(&g_pluginCS);
        for (int i = 0; i < g_pluginCount; i++) {
            if (i == skipIndex) {
                continue;
            }
            if (DetachPluginProcessLocked(i, &detachedPlugin)) {
                detached = TRUE;
                g_lastRunningPluginIndex = -1;
                g_activePluginIndex = -1;
                break;
            }
        }
        LeaveCriticalSection(&g_pluginCS);

        if (!detached) {
            break;
        }

        PluginProcess_TerminateDetached(&detachedPlugin);
        detachedCount++;
    }

    return detachedCount;
}

BOOL PreparePluginLaunchLocked(int index, const wchar_t* expectedPath,
                                      PluginInfo* launchPlugin,
                                      PluginInfo* detachedPlugins,
                                      int* detachedCount,
                                      BOOL* alreadyRunning) {
    if (!launchPlugin || !detachedPlugins || !detachedCount || !alreadyRunning) {
        return FALSE;
    }

    *detachedCount = 0;
    *alreadyRunning = FALSE;

    if (!g_pluginManagerInitialized || index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    const PluginInfo* plugin = &g_plugins[index];
    if (expectedPath && wcscmp(plugin->path, expectedPath) != 0) {
        LOG_WARNING("Plugin changed before launch; launch cancelled");
        PluginProcess_SetLastError(L"File changed");
        return FALSE;
    }

    if (plugin->isRunning) {
        *alreadyRunning = TRUE;
        return TRUE;
    }

    *launchPlugin = *plugin;
    launchPlugin->isRunning = FALSE;
    memset(&launchPlugin->pi, 0, sizeof(launchPlugin->pi));

    for (int i = 0; i < g_pluginCount && *detachedCount < MAX_PLUGINS; i++) {
        if (i == index || !g_plugins[i].isRunning) {
            continue;
        }

        if (DetachPluginProcessLocked(i, &detachedPlugins[*detachedCount])) {
            (*detachedCount)++;
        }
    }

    if (*detachedCount > 0) {
        g_lastRunningPluginIndex = -1;
        g_activePluginIndex = -1;
    }

    return TRUE;
}

BOOL LaunchPreparedPlugin(int index, const wchar_t* expectedPath) {
    if (!g_pluginManagerInitialized) return FALSE;

    PluginInfo launchPlugin;
    PluginInfo* detachedPlugins = AllocatePluginSnapshotArray();
    if (!detachedPlugins) {
        LOG_ERROR("Failed to allocate detached plugin snapshots");
        PluginProcess_SetLastError(L"Internal error");
        return FALSE;
    }
    int detachedCount = 0;
    BOOL alreadyRunning = FALSE;

    EnterCriticalSection(&g_pluginLifecycleCS);
    EnterCriticalSection(&g_pluginCS);

    BOOL prepared = PreparePluginLaunchLocked(index, expectedPath, &launchPlugin,
                                              detachedPlugins, &detachedCount,
                                              &alreadyRunning);

    LeaveCriticalSection(&g_pluginCS);

    if (!prepared) {
        LeaveCriticalSection(&g_pluginLifecycleCS);
        free(detachedPlugins);
        return FALSE;
    }
    if (alreadyRunning) {
        LeaveCriticalSection(&g_pluginLifecycleCS);
        free(detachedPlugins);
        return TRUE;
    }

    PluginProcess_TerminateAllOrphans();
    for (int i = 0; i < detachedCount; i++) {
        PluginProcess_TerminateDetached(&detachedPlugins[i]);
    }

    PluginData_SetOutputDirectoryFromPluginPath(launchPlugin.path);

    if (!PluginProcess_Launch(&launchPlugin)) {
        LOG_ERROR("Failed to launch plugin: %ls", launchPlugin.displayName);
        LeaveCriticalSection(&g_pluginLifecycleCS);
        free(detachedPlugins);
        return FALSE;
    }

    EnterCriticalSection(&g_pluginCS);

    if (!g_pluginManagerInitialized || index < 0 || index >= g_pluginCount ||
        (expectedPath && wcscmp(g_plugins[index].path, expectedPath) != 0)) {
        LOG_WARNING("Plugin changed while launch was running; launched process will be stopped");
        PluginProcess_SetLastError(L"File changed");
        LeaveCriticalSection(&g_pluginCS);
        PluginProcess_TerminateDetached(&launchPlugin);
        LeaveCriticalSection(&g_pluginLifecycleCS);
        free(detachedPlugins);
        return FALSE;
    }

    g_plugins[index].isRunning = launchPlugin.isRunning;
    g_plugins[index].pi = launchPlugin.pi;

    BOOL stillRunning = PluginProcess_IsAlive(&g_plugins[index]);
    if (stillRunning) {
        g_activePluginIndex = index;
        g_lastRunningPluginIndex = -1;
    } else {
        PluginProcess_SetLastError(L"Exited");
        LOG_WARNING("Plugin exited before startup completed: %ls", launchPlugin.displayName);
    }
    LeaveCriticalSection(&g_pluginCS);

    LeaveCriticalSection(&g_pluginLifecycleCS);
    free(detachedPlugins);
    return stillRunning;
}

void UpdatePluginLastModTimeIfCurrent(int index, const wchar_t* pluginPath) {
    if (!pluginPath) return;

    FILETIME modTime = {0};
    if (!GetFileModTime(pluginPath, &modTime)) {
        return;
    }

    if (!g_pluginManagerInitialized) return;
    EnterCriticalSection(&g_pluginCS);

    if (index >= 0 && index < g_pluginCount &&
        wcscmp(g_plugins[index].path, pluginPath) == 0 &&
        g_plugins[index].isRunning) {
        g_plugins[index].lastModTime = modTime;
    }

    LeaveCriticalSection(&g_pluginCS);
}
