    LOG_INFO("[HotReload] Restarting plugin %d from verified snapshot", request.index);
    return RestartPluginInternalWithExpected(request.index, request.name, request.path);
}

void PluginManager_HandleProcessExit(DWORD processId) {
    if (!g_pluginManagerInitialized || processId == 0) return;

    BOOL shouldClearDisplay = FALSE;
    BOOL hasRunningPlugin = FALSE;
    HANDLE hProcessToClose = NULL;
    HANDLE hThreadToClose = NULL;

    EnterCriticalSection(&g_pluginCS);

    for (int i = 0; i < g_pluginCount; i++) {
        PluginInfo* plugin = &g_plugins[i];
        if (plugin->pi.dwProcessId != processId) {
            continue;
        }

        if (InterlockedCompareExchange((volatile LONG*)&plugin->isRunning, FALSE, TRUE) == TRUE) {
            hProcessToClose = InterlockedExchangePointer((PVOID*)&plugin->pi.hProcess, NULL);
            hThreadToClose = InterlockedExchangePointer((PVOID*)&plugin->pi.hThread, NULL);
            memset(&plugin->pi, 0, sizeof(plugin->pi));
            shouldClearDisplay = TRUE;

            if (g_activePluginIndex == i) {
                g_activePluginIndex = -1;
            }
            if (g_lastRunningPluginIndex == i) {
                g_lastRunningPluginIndex = -1;
            }
        }
        break;
    }

    hasRunningPlugin = AnyPluginRunningLocked();
    LeaveCriticalSection(&g_pluginCS);

    if (hProcessToClose) {
        CloseHandle(hProcessToClose);
    }
    if (hThreadToClose) {
        PluginProcess_CloseMonitorThreadHandle(hThreadToClose, TRUE);
    }

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
}
