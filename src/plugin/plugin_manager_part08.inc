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

static BOOL StartPluginIfPathMatches(int index, const wchar_t* expectedPath) {
    return StartPluginWithExpectedPath(index, expectedPath);
}

BOOL PluginManager_IsPluginRunning(int index) {
    if (!g_pluginManagerInitialized) return FALSE;

    BOOL isRunning;
    BOOL shouldClearDisplay = FALSE;
    BOOL hasRunningPlugin = FALSE;
    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    PluginInfo* plugin = &g_plugins[index];

    /* Check via process module */
    if (plugin->isRunning && !PluginProcess_IsAlive(plugin)) {
        if (g_activePluginIndex == index) {
            g_activePluginIndex = -1;
            shouldClearDisplay = TRUE;
        }
        if (g_lastRunningPluginIndex == index) {
            g_lastRunningPluginIndex = -1;
        }
    } else if (!plugin->isRunning && g_activePluginIndex == index) {
        g_activePluginIndex = -1;
        shouldClearDisplay = TRUE;
        if (g_lastRunningPluginIndex == index) {
            g_lastRunningPluginIndex = -1;
        }
    }

    isRunning = plugin->isRunning;
    hasRunningPlugin = AnyPluginRunningLocked();
    LeaveCriticalSection(&g_pluginCS);

    if (shouldClearDisplay) {
        if (!hasRunningPlugin) {
            PluginProcess_TerminateAllOrphans();
        }
        PluginData_Clear();
        HWND hwnd = PluginProcess_GetNotifyWindow();
        if (hwnd) {
            InvalidateRect(hwnd, NULL, TRUE);
        }
        StopHotReloadIfIdle();
    }

    return isRunning;
}

BOOL PluginManager_NeedsSecurityCheck(int index) {
    if (!g_pluginManagerInitialized) return FALSE;

    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    /* Get plugin path */
    wchar_t pluginPath[MAX_PATH];
    wcsncpy(pluginPath, g_plugins[index].path, MAX_PATH - 1);
    pluginPath[MAX_PATH - 1] = L'\0';

    LeaveCriticalSection(&g_pluginCS);

    /* Convert to UTF-8 for security check */
    char pluginPathUtf8[MAX_PATH];
    if (!WideToUtf8Fixed(pluginPath, pluginPathUtf8, MAX_PATH)) {
        LOG_ERROR("Failed to convert plugin path to UTF-8: %ls", pluginPath);
        return TRUE;
    }

    /* Return TRUE if NOT trusted (needs security check) */
    return !IsPluginTrusted(pluginPathUtf8);
}

void PluginManager_StopAllPlugins(void) {
    if (!g_pluginManagerInitialized) return;

    PluginInfo* detachedPlugins = AllocatePluginSnapshotArray();

    EnterCriticalSection(&g_pluginLifecycleCS);

    if (detachedPlugins) {
        EnterCriticalSection(&g_pluginCS);
        int detachedCount = DetachAllRunningPluginProcessesLocked(detachedPlugins, MAX_PLUGINS);
        // Always clear indices (even if no plugin was running)
        // This handles cases where plugin already exited (e.g., after <exit> tag)
        g_activePluginIndex = -1;
        g_lastRunningPluginIndex = -1;
        LeaveCriticalSection(&g_pluginCS);

        for (int i = 0; i < detachedCount; i++) {
            PluginProcess_TerminateDetached(&detachedPlugins[i]);
        }
        PluginProcess_TerminateAllOrphans();
        free(detachedPlugins);
    } else {
        LOG_WARNING("Plugin stop-all using one-by-one cleanup after snapshot allocation failed");
        DetachAndTerminateRunningPluginsIndividually(-1);
        PluginProcess_TerminateAllOrphans();

        EnterCriticalSection(&g_pluginCS);
        g_activePluginIndex = -1;
        g_lastRunningPluginIndex = -1;
        LeaveCriticalSection(&g_pluginCS);
    }

    PluginData_Clear();
    StopHotReloadThread();
    LeaveCriticalSection(&g_pluginLifecycleCS);
}

BOOL PluginManager_OpenPluginFolder(void) {
    wchar_t pluginDir[MAX_PATH];
    if (!PluginManager_GetPluginDirW(pluginDir, MAX_PATH)) {
        return FALSE;
    }

    // Ensure directory exists
    CreateDirectoryW(pluginDir, NULL);

    // Open in explorer
    ShellExecuteW(NULL, L"open", pluginDir, NULL, NULL, SW_SHOW);
    return TRUE;
}

void PluginManager_SetNotifyWindow(HWND hwnd) {
    PluginProcess_SetNotifyWindow(hwnd);
}

int PluginManager_GetActivePluginIndex(void) {
    if (!g_pluginManagerInitialized) return -1;

    EnterCriticalSection(&g_pluginCS);

    int activeIndex = g_activePluginIndex;
    if (activeIndex < 0 || activeIndex >= g_pluginCount) {
        activeIndex = -1;
    }

    LeaveCriticalSection(&g_pluginCS);
    return activeIndex;
}

BOOL PluginManager_RestartPlugin(int index) {
    return RestartPluginInternal(index);
}

BOOL PluginManager_RestartPendingHotReload(LONG requestGeneration) {
    if (requestGeneration == 0 || !g_pluginManagerInitialized) {
        return FALSE;
    }

    PluginHotReloadRequest request = {0};
    BOOL hasRequest = FALSE;

    EnterCriticalSection(&g_pluginCS);
    if (g_hotReloadRequestPending &&
        InterlockedCompareExchange(&g_hotReloadRequestGeneration, 0, 0) == requestGeneration) {
        request = g_hotReloadRequest;
        g_hotReloadRequestPending = FALSE;
        hasRequest = TRUE;
    }
    LeaveCriticalSection(&g_pluginCS);

    if (!hasRequest) {
        LOG_WARNING("[HotReload] Ignoring stale hot-reload request generation %ld",
                    requestGeneration);
        return FALSE;
    }
