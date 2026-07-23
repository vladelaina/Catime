        ReleaseSRWLockExclusive(&g_hotReloadLock);
        return;
    }

    EnterCriticalSection(&g_pluginCS);
    hasRunningPlugin = AnyPluginRunningLocked();
    LeaveCriticalSection(&g_pluginCS);

    if (!hasRunningPlugin) {
        StopHotReloadThreadLocked();
    }
    ReleaseSRWLockExclusive(&g_hotReloadLock);
}

/**
 * @brief Extract display name from plugin filename (keep extension)
 */
static void ExtractDisplayName(const wchar_t* filename, wchar_t* displayName, size_t bufferSize) {
    wcsncpy(displayName, filename, bufferSize - 1);
    displayName[bufferSize - 1] = L'\0';
}

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

static BOOL ScanPluginDirSnapshotRecursive(const wchar_t* folderPath,
                                           const wchar_t* relativePath,
                                           PluginDirSnapshot* snapshot,
                                           int depth) {
    if (!folderPath || !snapshot) return FALSE;

    if (depth >= MAX_PLUGIN_RECURSION_DEPTH) {
        snapshot->truncated = TRUE;
        return TRUE;
    }

    wchar_t searchPath[MAX_PATH];
    int written = _snwprintf_s(searchPath, MAX_PATH, _TRUNCATE,
                               L"%s\\*", folderPath);
    if (written < 0 || written >= MAX_PATH) {
        snapshot->truncated = TRUE;
        return TRUE;
    }

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND &&
            error != ERROR_PATH_NOT_FOUND &&
            error != ERROR_NO_MORE_FILES) {
            LOG_WARNING("Failed to snapshot plugin folder: %ls (error=%lu)",
                        folderPath, error);
            return FALSE;
        }
        return TRUE;
    }

    BOOL ok = TRUE;
    do {
        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        if (++snapshot->scannedEntries > MAX_PLUGIN_SCAN_ENTRIES) {
            snapshot->truncated = TRUE;
            break;
        }

        wchar_t childRelativePath[MAX_PATH];
        if (!BuildRelativePathW(childRelativePath, MAX_PATH,
                                relativePath, findData.cFileName)) {
            snapshot->truncated = TRUE;
            continue;
        }

        BOOL isDirectory =
            (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDirectory &&
            !(findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            wchar_t childPath[MAX_PATH];
            int pathWritten = _snwprintf_s(childPath, MAX_PATH, _TRUNCATE,
                                           L"%s\\%s", folderPath, findData.cFileName);
            if (pathWritten < 0 || pathWritten >= MAX_PATH) {
                snapshot->truncated = TRUE;
                continue;
            }
            if (!ScanPluginDirSnapshotRecursive(childPath, childRelativePath,
                                                snapshot, depth + 1)) {
                ok = FALSE;
                break;
            }
            if (snapshot->truncated) {
                break;
            }
        } else if (!isDirectory && IsSupportedPluginFileW(findData.cFileName)) {
            snapshot->entryCount++;
            UpdateLatestWriteTime(&snapshot->lastWriteTime, &findData.ftLastWriteTime);
            MixPluginSnapshotPath(snapshot, childRelativePath);
            MixPluginSnapshotHash(snapshot,
                                  ((ULONGLONG)findData.ftLastWriteTime.dwHighDateTime << 32) |
                                  findData.ftLastWriteTime.dwLowDateTime);
            MixPluginSnapshotHash(snapshot,
                                  ((ULONGLONG)findData.nFileSizeHigh << 32) |
                                  findData.nFileSizeLow);
        }
    } while (FindNextFileW(hFind, &findData));

    DWORD findError = snapshot->truncated ? ERROR_SUCCESS : GetLastError();
    FindClose(hFind);
    if (ok && findError != ERROR_NO_MORE_FILES) {
        LOG_WARNING("Plugin snapshot enumeration failed: %ls (error=%lu)",
                    folderPath, findError);
        ok = FALSE;
    }
    return ok;
}

static BOOL GetPluginDirSnapshot(PluginDirSnapshot* snapshot) {
    if (!snapshot) return FALSE;
    ZeroMemory(snapshot, sizeof(*snapshot));
