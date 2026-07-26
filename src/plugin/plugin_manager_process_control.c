/**
 * @file plugin_manager_process_control.c
 * @brief Restart, stop, and toggle operations.
 */

#include "plugin_manager_internal.h"

BOOL RestartPluginInternal(int index) {
    return RestartPluginInternalWithExpected(index, NULL, NULL);
}

BOOL RestartPluginInternalWithExpected(int index,
                                             const wchar_t* expectedName,
                                             const wchar_t* expectedPath) {
    PluginInfo pluginSnapshot;
    if (!PluginManager_CopyPlugin(index, &pluginSnapshot)) {
        return FALSE;
    }
    if ((expectedName && wcscmp(pluginSnapshot.name, expectedName) != 0) ||
        (expectedPath && wcscmp(pluginSnapshot.path, expectedPath) != 0)) {
        LOG_WARNING("Plugin changed before hot-reload restart; restart cancelled");
        return FALSE;
    }

    BOOL pathMatched = FALSE;
    StopPluginIfPathMatches(index, pluginSnapshot.path, &pathMatched);
    if (!pathMatched) {
        LOG_WARNING("Plugin changed before hot-reload restart; restart cancelled");
        return FALSE;
    }

    PluginInfo currentPlugin;
    if (!PluginManager_CopyPlugin(index, &currentPlugin) ||
        wcscmp(currentPlugin.name, pluginSnapshot.name) != 0 ||
        wcscmp(currentPlugin.path, pluginSnapshot.path) != 0) {
        LOG_WARNING("Plugin changed after hot-reload stop; restart cancelled");
        return FALSE;
    }

    /* Show "Loading..." message */
    wchar_t loadingText[256];
    PluginData_SetOutputDirectoryFromPluginPath(currentPlugin.path);
    _snwprintf_s(loadingText, 256, _TRUNCATE, L"Loading %ls...", currentPlugin.displayName);
    PluginData_SetText(loadingText);
    PluginData_SetActive(TRUE);

    /* Force redraw */
    HWND hwnd = PluginProcess_GetNotifyWindow();
    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }

    return StartPluginIfPathMatches(index, pluginSnapshot.path);
}

BOOL StopPluginIfPathMatches(int index, const wchar_t* expectedPath, BOOL* pathMatched) {
    if (pathMatched) {
        *pathMatched = FALSE;
    }
    if (!g_pluginManagerInitialized || !expectedPath) return FALSE;

    EnterCriticalSection(&g_pluginLifecycleCS);
    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount ||
        wcscmp(g_plugins[index].path, expectedPath) != 0) {
        LeaveCriticalSection(&g_pluginCS);
        LeaveCriticalSection(&g_pluginLifecycleCS);
        return FALSE;
    }

    if (pathMatched) {
        *pathMatched = TRUE;
    }

    PluginInfo detachedPlugin;
    memset(&detachedPlugin, 0, sizeof(detachedPlugin));
    BOOL wasActive = (g_activePluginIndex == index);
    BOOL wasLastRunning = (g_lastRunningPluginIndex == index);
    if (!DetachPluginProcessLocked(index, &detachedPlugin)) {
        if (!wasActive && !wasLastRunning) {
            LeaveCriticalSection(&g_pluginCS);
            LeaveCriticalSection(&g_pluginLifecycleCS);
            return FALSE;
        }
        if (wasActive) {
            g_activePluginIndex = -1;
        }
        if (wasLastRunning) {
            g_lastRunningPluginIndex = -1;
        }
        BOOL hasRunningPlugin = AnyPluginRunningLocked();
        LeaveCriticalSection(&g_pluginCS);
        if (!hasRunningPlugin) {
            PluginProcess_TerminateAllOrphans();
        }
        PluginData_Clear();
        StopHotReloadIfIdle();
        LeaveCriticalSection(&g_pluginLifecycleCS);
        return TRUE;
    }

    g_lastRunningPluginIndex = -1;
    g_activePluginIndex = -1;
    BOOL hasRunningPlugin = AnyPluginRunningLocked();

    LeaveCriticalSection(&g_pluginCS);
    PluginProcess_TerminateDetached(&detachedPlugin);
    if (!hasRunningPlugin) {
        PluginProcess_TerminateAllOrphans();
    }
    PluginData_Clear();
    StopHotReloadIfIdle();
    LeaveCriticalSection(&g_pluginLifecycleCS);
    return TRUE;
}

BOOL PluginManager_StopPlugin(int index) {
    if (!g_pluginManagerInitialized) return FALSE;
    EnterCriticalSection(&g_pluginLifecycleCS);
    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LeaveCriticalSection(&g_pluginCS);
        LeaveCriticalSection(&g_pluginLifecycleCS);
        return FALSE;
    }

    PluginInfo detachedPlugin;
    memset(&detachedPlugin, 0, sizeof(detachedPlugin));
    wchar_t pluginDisplayName[64];
    wcsncpy(pluginDisplayName, g_plugins[index].displayName, 63);
    pluginDisplayName[63] = L'\0';
    BOOL wasActive = (g_activePluginIndex == index);
    BOOL wasLastRunning = (g_lastRunningPluginIndex == index);

    if (!DetachPluginProcessLocked(index, &detachedPlugin)) {
        if (!wasActive && !wasLastRunning) {
            LOG_WARNING("Plugin %ls is not running", pluginDisplayName);
            LeaveCriticalSection(&g_pluginCS);
            LeaveCriticalSection(&g_pluginLifecycleCS);
            return FALSE;
        }
        if (wasActive) {
            g_activePluginIndex = -1;
        }
        if (wasLastRunning) {
            g_lastRunningPluginIndex = -1;
        }
        BOOL hasRunningPlugin = AnyPluginRunningLocked();
        LeaveCriticalSection(&g_pluginCS);
        if (!hasRunningPlugin) {
            PluginProcess_TerminateAllOrphans();
        }
        PluginData_Clear();
        StopHotReloadIfIdle();
        LeaveCriticalSection(&g_pluginLifecycleCS);
        return TRUE;
    }

    g_lastRunningPluginIndex = -1;
    g_activePluginIndex = -1;
    BOOL hasRunningPlugin = AnyPluginRunningLocked();

    LeaveCriticalSection(&g_pluginCS);
    PluginProcess_TerminateDetached(&detachedPlugin);
    if (!hasRunningPlugin) {
        PluginProcess_TerminateAllOrphans();
    }
    PluginData_Clear();
    StopHotReloadIfIdle();
    LeaveCriticalSection(&g_pluginLifecycleCS);
    return TRUE;
}

BOOL PluginManager_TogglePlugin(int index) {
    if (!g_pluginManagerInitialized) return FALSE;

    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    BOOL isRunning = g_plugins[index].isRunning;
    BOOL isActive = (g_activePluginIndex == index);
    wchar_t pluginPath[MAX_PATH];
    wcsncpy(pluginPath, g_plugins[index].path, MAX_PATH - 1);
    pluginPath[MAX_PATH - 1] = L'\0';
    LeaveCriticalSection(&g_pluginCS);

    if (isRunning || isActive) {
        BOOL pathMatched = FALSE;
        return StopPluginIfPathMatches(index, pluginPath, &pathMatched);
    } else {
        return StartPluginIfPathMatches(index, pluginPath);
    }
}
