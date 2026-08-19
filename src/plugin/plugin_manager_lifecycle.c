/**
 * @file plugin_manager_lifecycle.c
 * @brief Public manager initialization and shutdown.
 */

#include "plugin_manager_internal.h"

void PluginManager_Init(void) {
    BOOL retiredScanStopped = CleanupRetiredAsyncScanThread(ASYNC_PLUGIN_SCAN_STOP_TIMEOUT_MS);
    BOOL hasRetiredScan = HasRetiredAsyncScanThread();

    if (!retiredScanStopped || hasRetiredScan) {
        InterlockedExchange(&g_asyncScanShuttingDown, 1);
        InterlockedIncrement(&g_asyncScanGeneration);
        LOG_WARNING("Plugin manager initialization skipped because previous async scan is still retiring");
        return;
    }

    if (!g_pluginLocksInitialized) {
        InitializeCriticalSection(&g_pluginCS);
        InitializeCriticalSection(&g_pluginLifecycleCS);
        g_pluginLocksInitialized = TRUE;
    }

    g_pluginManagerInitialized = TRUE;
    PluginManager_InitAsync();
    memset(g_plugins, 0, sizeof(g_plugins));
    g_pluginCount = 0;
    InterlockedExchange(&g_asyncScanPending, 0);
    InterlockedExchange(&g_asyncScanShuttingDown, 0);
    InterlockedIncrement(&g_asyncScanGeneration);
    g_hotReloadStartFailureCooldownUntil = 0;
    g_hAsyncScanThread = NULL;
    g_asyncScanHasLastSnapshot = FALSE;
    ZeroMemory(&g_asyncScanLastSnapshot, sizeof(g_asyncScanLastSnapshot));
    g_asyncScanHasFailureSnapshot = FALSE;
    g_asyncScanFailureHadSnapshot = FALSE;
    ZeroMemory(&g_asyncScanFailureSnapshot, sizeof(g_asyncScanFailureSnapshot));
    InterlockedExchange(&g_asyncScanFailureCooldownUntil, 0);

    /* Initialize process management */
    if (!g_pluginProcessInitialized) {
        g_pluginProcessInitialized = PluginProcess_Init();
    }

    StartPluginFolderWatcher();

    LOG_INFO("Plugin manager initialized");
}

void PluginManager_Shutdown(void) {
    if (!g_pluginManagerInitialized && !g_pluginLocksInitialized) return;

    /* Drain queued start/stop work before tearing down process locks. */
    PluginManager_ShutdownAsync();

    StopPluginFolderWatcher();

    InterlockedIncrement(&g_asyncScanGeneration);
    BOOL asyncScanStopped = StopAsyncScanThread() &&
                            CleanupRetiredAsyncScanThread(0);
    BOOL lifecycleLockEntered = FALSE;

    if (asyncScanStopped) {
        EnterCriticalSection(&g_pluginLifecycleCS);
        lifecycleLockEntered = TRUE;
    } else {
        lifecycleLockEntered = EnterCriticalSectionWithTimeout(&g_pluginLifecycleCS,
            PLUGIN_MANAGER_SHUTDOWN_LOCK_WAIT_MS);
    }

    if (!lifecycleLockEntered) {
        StopHotReloadThread();
        if (g_pluginProcessInitialized) {
            PluginProcess_TerminateAllOrphans();
            PluginProcess_Shutdown();
            g_pluginProcessInitialized = FALSE;
        }
        g_pluginManagerInitialized = FALSE;
        LOG_WARNING("Plugin manager shutdown deferred because async scan still owns lifecycle lock; plugin process job was closed via fallback");
        LOG_INFO("Plugin manager shutdown");
        return;
    }

    BOOL hotReloadStopped = StopHotReloadThread();

    PluginInfo* detachedPlugins = AllocatePluginSnapshotArray();

    if (detachedPlugins) {
        EnterCriticalSection(&g_pluginCS);

        int detachedCount =
            DetachAllRunningPluginProcessesLocked(detachedPlugins, MAX_PLUGINS);
        g_activePluginIndex = -1;
        g_lastRunningPluginIndex = -1;
        g_pluginCount = 0;
        g_pluginManagerInitialized = FALSE;

        LeaveCriticalSection(&g_pluginCS);

        for (int i = 0; i < detachedCount; i++) {
            PluginProcess_TerminateDetached(&detachedPlugins[i]);
        }
        free(detachedPlugins);
    } else {
        LOG_WARNING("Plugin manager shutdown using one-by-one plugin cleanup after snapshot allocation failed");
        DetachAndTerminateRunningPluginsIndividually(-1);

        EnterCriticalSection(&g_pluginCS);
        g_activePluginIndex = -1;
        g_lastRunningPluginIndex = -1;
        g_pluginCount = 0;
        g_pluginManagerInitialized = FALSE;
        LeaveCriticalSection(&g_pluginCS);
    }

    PluginData_Clear();

    /* Shutdown process management once plugin state has been detached.  The
     * async scanner may still be retiring, but it no longer needs the job.
     */
    if (g_pluginProcessInitialized) {
        PluginProcess_Shutdown();
        g_pluginProcessInitialized = FALSE;
    }

    LeaveCriticalSection(&g_pluginLifecycleCS);
    if (asyncScanStopped && hotReloadStopped) {
        DeleteCriticalSection(&g_pluginCS);
        DeleteCriticalSection(&g_pluginLifecycleCS);
        g_pluginLocksInitialized = FALSE;
    } else {
        LOG_WARNING("Plugin manager locks retained because background plugin threads did not stop before shutdown");
    }
    LOG_INFO("Plugin manager shutdown");
}

BOOL PluginManager_GetPluginDir(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0 || bufferSize > (size_t)INT_MAX) {
        return FALSE;
    }

    wchar_t pluginDir[MAX_PATH];
    if (!PluginManager_GetPluginDirW(pluginDir, MAX_PATH) ||
        !WideToUtf8Fixed(pluginDir, buffer, (int)bufferSize)) {
        return FALSE;
    }

    return TRUE;
}
