/**
 * @file plugin_manager_scan.c
 * @brief Synchronous scan execution and plugin-list replacement.
 */

#include "plugin_manager_internal.h"

int PluginManager_ScanPluginsForGeneration(LONG generation) {
    wchar_t pluginDir[MAX_PATH];
    if (!PluginManager_GetPluginDirW(pluginDir, MAX_PATH)) {
        return PLUGIN_SCAN_FAILED;
    }

    if (!g_pluginManagerInitialized ||
        !IsAsyncScanGenerationCurrent(generation)) {
        return PLUGIN_SCAN_FAILED;
    }

    PluginInfo* newPlugins = (PluginInfo*)calloc(MAX_PLUGINS, sizeof(*newPlugins));
    PluginInfo* removedPlugins = (PluginInfo*)calloc(MAX_PLUGINS, sizeof(*removedPlugins));
    int newPluginCount = 0;
    int removedPluginCount = 0;
    BOOL shouldClearDisplay = FALSE;
    BOOL hasRunningPluginAfterScan = FALSE;
    BOOL scanCancelled = FALSE;
    int scanResult = PLUGIN_SCAN_FAILED;

    if (!newPlugins || !removedPlugins) {
        LOG_WARNING("Failed to allocate plugin scan buffers");
        goto cleanup;
    }

    DWORD pluginDirAttrs = GetFileAttributesW(pluginDir);
    if (pluginDirAttrs == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            scanResult = 0;
        } else {
            LOG_WARNING("Failed to stat plugin directory (error=%lu)", error);
        }
        goto cleanup;
    }
    if (!(pluginDirAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
        LOG_WARNING("Plugin path is not a directory: %ls", pluginDir);
        goto cleanup;
    }

    PluginScanContext scanCtx = {0};
    scanCtx.plugins = newPlugins;
    ScanPluginFolderRecursive(pluginDir, pluginDir, L"", &scanCtx, 0, generation);
    if (IsAsyncScanShuttingDown() || !IsAsyncScanGenerationCurrent(generation)) {
        scanCancelled = TRUE;
        goto cleanup;
    }
    if (scanCtx.failed) {
        goto cleanup;
    }
    newPluginCount = scanCtx.count;

    if (scanCancelled || IsAsyncScanShuttingDown() ||
        !IsAsyncScanGenerationCurrent(generation)) {
        goto cleanup;
    }

    // Sort plugins by display name (natural order) for consistent menu ordering
    if (newPluginCount > 1) {
        qsort(newPlugins, newPluginCount, sizeof(PluginInfo), ComparePluginInfo);
    }

    if (IsAsyncScanShuttingDown() ||
        !IsAsyncScanGenerationCurrent(generation)) {
        goto cleanup;
    }

    EnterCriticalSection(&g_pluginLifecycleCS);

    if (!g_pluginManagerInitialized ||
        IsAsyncScanShuttingDown() ||
        !IsAsyncScanGenerationCurrent(generation)) {
        LeaveCriticalSection(&g_pluginLifecycleCS);
        goto cleanup;
    }
    EnterCriticalSection(&g_pluginCS);

    // Preserve state from existing list after the lock is acquired.
    for (int j = 0; j < newPluginCount; j++) {
        for (int i = 0; i < g_pluginCount; i++) {
            if (wcscmp(g_plugins[i].path, newPlugins[j].path) == 0) {
                if (g_plugins[i].isRunning && !PluginProcess_IsAlive(&g_plugins[i])) {
                    if (g_activePluginIndex == i) {
                        g_activePluginIndex = -1;
                        shouldClearDisplay = TRUE;
                    }
                    if (g_lastRunningPluginIndex == i) {
                        g_lastRunningPluginIndex = -1;
                    }
                }
                newPlugins[j].isRunning = g_plugins[i].isRunning;
                newPlugins[j].pi = g_plugins[i].pi;
                newPlugins[j].lastModTime = g_plugins[i].lastModTime;
                break;
            }
        }
    }

    /* Clean up orphaned plugins (running but file removed) */
    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            BOOL found = FALSE;
            for (int j = 0; j < newPluginCount; j++) {
                if (wcscmp(g_plugins[i].path, newPlugins[j].path) == 0) {
                    found = TRUE;
                    break;
                }
            }
            if (!found) {
                removedPlugins[removedPluginCount++] = g_plugins[i];
                g_plugins[i].isRunning = FALSE;
                memset(&g_plugins[i].pi, 0, sizeof(g_plugins[i].pi));
                if (g_activePluginIndex == i) {
                    g_activePluginIndex = -1;
                    shouldClearDisplay = TRUE;
                }
                if (g_lastRunningPluginIndex == i) {
                    g_lastRunningPluginIndex = -1;
                }
            }
        }
    }

    // Remember old plugin paths for re-mapping indices
    wchar_t lastRunningPath[MAX_PATH] = {0};
    wchar_t activePluginPath[MAX_PATH] = {0};
    if (g_lastRunningPluginIndex >= 0 && g_lastRunningPluginIndex < g_pluginCount) {
        wcsncpy(lastRunningPath, g_plugins[g_lastRunningPluginIndex].path, MAX_PATH - 1);
        lastRunningPath[MAX_PATH - 1] = L'\0';
    }
    if (g_activePluginIndex >= 0 && g_activePluginIndex < g_pluginCount) {
        wcsncpy(activePluginPath, g_plugins[g_activePluginIndex].path, MAX_PATH - 1);
        activePluginPath[MAX_PATH - 1] = L'\0';
    }

    // Update global list
    if (newPluginCount > 0) {
        memcpy(g_plugins, newPlugins, (size_t)newPluginCount * sizeof(PluginInfo));
    }
    if (newPluginCount < MAX_PLUGINS) {
        memset(&g_plugins[newPluginCount], 0,
               (size_t)(MAX_PLUGINS - newPluginCount) * sizeof(PluginInfo));
    }
    g_pluginCount = newPluginCount;

    // Re-map g_lastRunningPluginIndex to new list
    if (lastRunningPath[0]) {
        g_lastRunningPluginIndex = -1;  // Reset first
        for (int i = 0; i < g_pluginCount; i++) {
            if (wcscmp(g_plugins[i].path, lastRunningPath) == 0) {
                g_lastRunningPluginIndex = i;
                break;
            }
        }
    }

    // Re-map g_activePluginIndex to new list
    if (activePluginPath[0]) {
        BOOL activePluginRemapped = FALSE;
        g_activePluginIndex = -1;  // Reset first
        for (int i = 0; i < g_pluginCount; i++) {
            if (wcscmp(g_plugins[i].path, activePluginPath) == 0) {
                g_activePluginIndex = i;
                activePluginRemapped = TRUE;
                break;
            }
        }
        if (!activePluginRemapped) {
            shouldClearDisplay = TRUE;
        }
    }

    hasRunningPluginAfterScan = AnyPluginRunningLocked();

    LeaveCriticalSection(&g_pluginCS);

    for (int i = 0; i < removedPluginCount; i++) {
        PluginProcess_TerminateDetached(&removedPlugins[i]);
    }
    if ((removedPluginCount > 0 || shouldClearDisplay) &&
        !hasRunningPluginAfterScan) {
        PluginProcess_TerminateAllOrphans();
    }
    if (shouldClearDisplay) {
        PluginData_Clear();
    }
    if (removedPluginCount > 0 || shouldClearDisplay) {
        StopHotReloadIfIdle();
    }
    LeaveCriticalSection(&g_pluginLifecycleCS);

    scanResult = newPluginCount;

cleanup:
    free(removedPlugins);
    free(newPlugins);
    return scanResult;
}

int PluginManager_ScanPlugins(void) {
    LONG generation = InterlockedCompareExchange(&g_asyncScanGeneration, 0, 0);
    return PluginManager_ScanPluginsForGeneration(generation);
}

DWORD WINAPI AsyncScanThread(LPVOID lpParam) {
    PluginDirSnapshot requestedSnapshot = {0};
    BOOL hasRequestedSnapshot = FALSE;
    LONG generation = 0;

    if (lpParam) {
        const AsyncScanThreadParams* params = (const AsyncScanThreadParams*)lpParam;
        requestedSnapshot = params->snapshot;
        hasRequestedSnapshot = params->hasSnapshot;
        generation = params->generation;
        free(lpParam);
    }

    if (generation == 0) {
        generation = InterlockedCompareExchange(&g_asyncScanGeneration, 0, 0);
    }

    int scanResult = PluginManager_ScanPluginsForGeneration(generation);

    AcquireSRWLockExclusive(&g_asyncScanLock);
    if (scanResult >= 0 &&
        !IsAsyncScanShuttingDown() &&
        IsAsyncScanGenerationCurrent(generation)) {
        if (hasRequestedSnapshot) {
            g_asyncScanLastSnapshot = requestedSnapshot;
            g_asyncScanHasLastSnapshot = TRUE;
        }
        ClearAsyncScanFailureLocked();
    } else if (scanResult < 0 &&
               !IsAsyncScanShuttingDown() &&
               IsAsyncScanGenerationCurrent(generation)) {
        MarkAsyncScanFailureLocked(hasRequestedSnapshot, &requestedSnapshot);
    }
    InterlockedExchange(&g_asyncScanPending, 0);
    ReleaseSRWLockExclusive(&g_asyncScanLock);

    return 0;
}
