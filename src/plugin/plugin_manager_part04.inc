    wchar_t pluginDir[MAX_PATH];
    if (!PluginManager_GetPluginDirW(pluginDir, MAX_PATH)) {
        return FALSE;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(pluginDir, GetFileExInfoStandard, &attrs)) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            LOG_WARNING("Failed to stat plugin directory snapshot (error=%lu)", error);
            return FALSE;
        }
        snapshot->exists = FALSE;
        return TRUE;
    }

    if (!(attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        snapshot->exists = FALSE;
        return TRUE;
    }

    snapshot->exists = TRUE;
    snapshot->contentHash = 1469598103934665603ull;
    return ScanPluginDirSnapshotRecursive(pluginDir, L"", snapshot, 0);
}

static BOOL PluginDirSnapshotsEqual(const PluginDirSnapshot* a,
                                    const PluginDirSnapshot* b) {
    if (!a || !b) return FALSE;
    if (a->exists != b->exists) return FALSE;
    if (!a->exists) return TRUE;
    if (a->entryCount != b->entryCount) return FALSE;
    if (a->contentHash != b->contentHash) return FALSE;
    if (a->truncated != b->truncated) return FALSE;

    return CompareFileTime(&a->lastWriteTime, &b->lastWriteTime) == 0;
}

static BOOL IsAsyncScanFailureRecentlyCachedLocked(BOOL hasSnapshot,
                                                   const PluginDirSnapshot* snapshot,
                                                   DWORD now) {
    DWORD cooldownUntil =
        (DWORD)InterlockedCompareExchange(&g_asyncScanFailureCooldownUntil, 0, 0);
    if (!g_asyncScanHasFailureSnapshot ||
        cooldownUntil == 0 ||
        (LONG)(cooldownUntil - now) <= 0) {
        return FALSE;
    }

    if (g_asyncScanFailureHadSnapshot != hasSnapshot) {
        return FALSE;
    }
    if (!hasSnapshot) {
        return TRUE;
    }

    return PluginDirSnapshotsEqual(snapshot, &g_asyncScanFailureSnapshot);
}

static void MarkAsyncScanFailureLocked(BOOL hasSnapshot,
                                       const PluginDirSnapshot* snapshot) {
    g_asyncScanHasFailureSnapshot = TRUE;
    g_asyncScanFailureHadSnapshot = hasSnapshot;
    if (hasSnapshot && snapshot) {
        g_asyncScanFailureSnapshot = *snapshot;
    } else {
        ZeroMemory(&g_asyncScanFailureSnapshot, sizeof(g_asyncScanFailureSnapshot));
    }
    DWORD now = GetTickCount();
    DWORD cooldownUntil = now + ASYNC_PLUGIN_SCAN_FAILURE_COOLDOWN_MS;
    InterlockedExchange(&g_asyncScanFailureCooldownUntil,
                        (LONG)(cooldownUntil ? cooldownUntil : 1));
}

static void ClearAsyncScanFailureLocked(void) {
    g_asyncScanHasFailureSnapshot = FALSE;
    g_asyncScanFailureHadSnapshot = FALSE;
    ZeroMemory(&g_asyncScanFailureSnapshot, sizeof(g_asyncScanFailureSnapshot));
    InterlockedExchange(&g_asyncScanFailureCooldownUntil, 0);
}

static int PluginManager_ScanPluginsForGeneration(LONG generation) {
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
