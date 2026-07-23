    InterlockedExchange(&g_pluginDataRedrawQueued, 0);
    InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0);
    if (!watcherStopped) {
        g_pluginDataResourcesRetained = TRUE;
        LOG_WARNING("PluginData: Watcher resources retained because the watcher did not stop during shutdown");
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }
    EnterCriticalSection(&g_watchCS);
    if (g_hWatchStopEvent) {
        CloseHandle(g_hWatchStopEvent);
        g_hWatchStopEvent = NULL;
    }
    if (g_hWatchWakeEvent) {
        CloseHandle(g_hWatchWakeEvent);
        g_hWatchWakeEvent = NULL;
    }
    LeaveCriticalSection(&g_watchCS);

    /* Shutdown exit subsystem */
    if (!PluginExit_Shutdown()) {
        g_pluginDataResourcesRetained = TRUE;
        LOG_WARNING("PluginData: Exit countdown resources retained because countdown did not stop during shutdown");
        ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
        return;
    }

    /* Free memory */
    EnterCriticalSection(&g_dataCS);
    ResetPluginDataStateLocked();
    LeaveCriticalSection(&g_dataCS);

    DeletePluginDataLocks();
    g_pluginDataResourcesRetained = FALSE;
    ReleaseSRWLockExclusive(&g_pluginDataLifecycleLock);
}

BOOL PluginData_GetText(wchar_t* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0) return FALSE;
    if (!PluginData_BeginUse()) return FALSE;

    BOOL hasData = FALSE;
    EnterCriticalSection(&g_dataCS);

    if (g_pluginModeActive) {
        if (g_hasPluginData && g_pluginDisplayText) {
            /* Has actual data, including intentionally empty custom display text. */
            wcsncpy(buffer, g_pluginDisplayText, maxLen - 1);
            buffer[maxLen - 1] = L'\0';
            hasData = TRUE;
        } else {
            /* Plugin mode active but no data yet - show loading */
            wcsncpy(buffer, L"Loading...", maxLen - 1);
            buffer[maxLen - 1] = L'\0';
            hasData = TRUE;
        }
    }

    LeaveCriticalSection(&g_dataCS);
    PluginData_EndUse();
    return hasData;
}

void PluginData_Clear(void) {
    if (!PluginData_BeginUse()) return;

    /* Cancel any pending exit countdown */
    PluginExit_Cancel();

    /* Reset poll interval to default */
    SetPollIntervalMs(DEFAULT_POLL_INTERVAL_MS);

    EnterCriticalSection(&g_dataCS);
    g_pluginModeActive = FALSE;  // Deactivate plugin mode
    g_hasPluginData = FALSE;
    ClearPluginDisplayTextLocked();
    ClearLastContentCacheLocked();
    InvalidateLastOutputFileStateLocked();
    SetDisplaySourcePathLocked(NULL);
    SetDefaultPluginOutputDirectoryLocked();
    /* Clear any pending notification to prevent stale notifications */
    ResetPendingNotificationLocked();
    LeaveCriticalSection(&g_dataCS);
    if (!StopWatcherThreadIfIdle(PLUGIN_DATA_WATCHER_UI_STOP_WAIT_MS)) {
        LOG_WARNING("PluginData: Watcher stop deferred while clearing plugin data");
    }
    PluginData_EndUse();
}

void PluginData_SetText(const wchar_t* text) {
    if (!text) return;
    if (!PluginData_BeginUse()) return;

    EnterCriticalSection(&g_dataCS);

    size_t rawTextLen = wcslen(text);
    if (rawTextLen > SIZE_MAX / sizeof(wchar_t) - 1) {
        LeaveCriticalSection(&g_dataCS);
        PluginData_EndUse();
        return;
    }
    size_t textLen = rawTextLen + 1;
    if (!EnsurePluginDisplayTextCapacityLocked(textLen)) {
        LeaveCriticalSection(&g_dataCS);
        PluginData_EndUse();
        return;
    }

    wcscpy_s(g_pluginDisplayText, g_pluginDisplayTextLen, text);
    g_hasPluginData = TRUE;
    g_pluginModeActive = TRUE;
    ClearLastContentCacheLocked();
    InvalidateLastOutputFileStateLocked();
    SetDisplaySourcePathLocked(NULL);

    LeaveCriticalSection(&g_dataCS);

    /* Clear the plugin data file to prevent showing stale content from previous plugin.
     * If clearing fails, remember the old file state as the baseline so the watcher
     * does not immediately overwrite the loading text with stale output.
     */
    wchar_t filePath[MAX_PATH];
    BOOL outputStateCaptured = FALSE;
    if (GetPluginOutputPathW(filePath, MAX_PATH)) {
        HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, PLUGIN_OUTPUT_FILE_SHARE,
                                   NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
        } else {
            LOG_WARNING("PluginData: Failed to clear output file before setting text (error=%lu)",
                        GetLastError());
        }

        FILETIME writeTime = {0};
        ULONGLONG fileSize = 0;
        if (GetPluginOutputFileStateW(filePath, &writeTime, &fileSize)) {
            EnterCriticalSection(&g_dataCS);
            UpdateLastOutputFileStateLocked(&writeTime, fileSize);
            LeaveCriticalSection(&g_dataCS);
            outputStateCaptured = TRUE;
        }
    }

    InterlockedExchange(&g_forceNextUpdate, outputStateCaptured ? FALSE : TRUE);
    StartWatcherThreadIfNeeded();
    WakeWatcherThread();
    PluginData_EndUse();
}

void PluginData_SetStatusText(const wchar_t* text) {
    if (!text) return;
    if (!PluginData_BeginUse()) return;

    EnterCriticalSection(&g_dataCS);

    size_t rawTextLen = wcslen(text);
    if (rawTextLen > SIZE_MAX / sizeof(wchar_t) - 1) {
        LeaveCriticalSection(&g_dataCS);
        PluginData_EndUse();
        return;
    }
    size_t textLen = rawTextLen + 1;
    if (!EnsurePluginDisplayTextCapacityLocked(textLen)) {
        LeaveCriticalSection(&g_dataCS);
        PluginData_EndUse();
        return;
    }

    wcscpy_s(g_pluginDisplayText, g_pluginDisplayTextLen, text);
    g_hasPluginData = TRUE;
    g_pluginModeActive = TRUE;
    ClearLastContentCacheLocked();
    InvalidateLastOutputFileStateLocked();
    SetDisplaySourcePathLocked(NULL);

    LeaveCriticalSection(&g_dataCS);
    if (!StopWatcherThreadIfIdle(PLUGIN_DATA_WATCHER_UI_STOP_WAIT_MS)) {
        LOG_WARNING("PluginData: Watcher stop deferred while setting status text");
    }
    PluginData_EndUse();
}

BOOL PluginData_SetPreviewTextWithSource(const wchar_t* text, const wchar_t* sourcePath) {
    if (!text) return FALSE;

    if (text[0] == L'\0') {
        if (!PluginData_BeginUse()) {
            return FALSE;
        }

        EnterCriticalSection(&g_dataCS);
        g_pluginModeActive = TRUE;
        if (EnsurePluginDisplayTextCapacityLocked(1)) {
            g_pluginDisplayText[0] = L'\0';
            g_hasPluginData = TRUE;
        } else {
            g_hasPluginData = FALSE;
            ClearPluginDisplayTextLocked();
        }
        ClearLastContentCacheLocked();
        InvalidateLastOutputFileStateLocked();
        SetDisplaySourcePathLocked(sourcePath);
        BOOL accepted = g_hasPluginData;
        LeaveCriticalSection(&g_dataCS);

        PluginData_EndUse();
        return accepted;
    }

    char* utf8 = WideToUtf8Alloc(text);
    if (!utf8) {
        return FALSE;
    }

    if (!PluginData_BeginUse()) {
        free(utf8);
        return FALSE;
    }

    EnterCriticalSection(&g_dataCS);
    g_pluginModeActive = TRUE;
    SetDisplaySourcePathLocked(sourcePath);
    LeaveCriticalSection(&g_dataCS);

    BOOL displayChanged = FALSE;
    BOOL timerRecheck = FALSE;
    PluginParseResult parseResult =
        ParseContent(utf8, strlen(utf8), TRUE, &displayChanged, &timerRecheck);

    free(utf8);

    if (parseResult == PLUGIN_PARSE_OK) {
        if (timerRecheck) {
            QueuePluginDataTimerRecheck();
        }
        if ((displayChanged || timerRecheck) && g_hNotifyWnd) {
            RequestPluginDataRedraw(g_hNotifyWnd);
        }
    }

    PluginData_EndUse();
    return parseResult != PLUGIN_PARSE_FAILED;
}

BOOL PluginData_SetPreviewText(const wchar_t* text) {
    return PluginData_SetPreviewTextWithSource(text, NULL);
}

void PluginData_SetActive(BOOL active) {
    if (!PluginData_BeginUse()) return;
    EnterCriticalSection(&g_dataCS);
    g_pluginModeActive = active;
    if (active) {
        InterlockedExchange(&g_forceNextUpdate, FALSE);
        ClearLastContentCacheLocked();
        InvalidateLastOutputFileStateLocked();
    } else {
        // When deactivating, also clear any stale data
        g_hasPluginData = FALSE;
        ClearPluginDisplayTextLocked();
