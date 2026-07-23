        ClearLastContentCacheLocked();
        InvalidateLastOutputFileStateLocked();
        SetDisplaySourcePathLocked(NULL);
        SetDefaultPluginOutputDirectoryLocked();
        // Clear any pending notification
        ResetPendingNotificationLocked();
    }
    LeaveCriticalSection(&g_dataCS);

    if (active) {
        /* If activating, immediately read the file content when there is no
         * current baseline.  Otherwise let the normal change check decide,
         * which preserves freshly-set loading text when output.txt is stale.
         */
        wchar_t filePath[MAX_PATH];
        if (GetPluginOutputPathW(filePath, MAX_PATH)) {
            FILETIME currentWriteTime = {0};
            ULONGLONG currentFileSize = 0;
            BOOL hasBaseline = FALSE;
            EnterCriticalSection(&g_dataCS);
            hasBaseline = CopyLastOutputFileStateLocked(&currentWriteTime, &currentFileSize);
            LeaveCriticalSection(&g_dataCS);
            ProcessPluginOutputFile(filePath, !hasBaseline, &currentWriteTime, &currentFileSize);
        }
        StartWatcherThreadIfNeeded();
    } else {
        PluginExit_Cancel();
        if (!StopWatcherThreadIfIdle(PLUGIN_DATA_WATCHER_UI_STOP_WAIT_MS)) {
            LOG_WARNING("PluginData: Watcher stop deferred while deactivating plugin data");
        }
    }
    PluginData_EndUse();
}

BOOL PluginData_IsActive(void) {
    if (!PluginData_BeginUse()) return FALSE;
    BOOL active;
    EnterCriticalSection(&g_dataCS);
    active = g_pluginModeActive;
    LeaveCriticalSection(&g_dataCS);
    PluginData_EndUse();
    return active;
}

BOOL PluginData_HasCatimeTag(void) {
    if (!PluginData_BeginUse()) return FALSE;
    BOOL hasTag = FALSE;
    EnterCriticalSection(&g_dataCS);
    if (g_pluginModeActive && g_hasPluginData && g_pluginDisplayText) {
        // Check for <catime> and </catime> tags
        const wchar_t* start = wcsstr(g_pluginDisplayText, L"<catime>");
        const wchar_t* end = wcsstr(g_pluginDisplayText, L"</catime>");
        if (start && end && end > start) {
            hasTag = TRUE;
        }
    }
    LeaveCriticalSection(&g_dataCS);
    PluginData_EndUse();
    return hasTag;
}

void PluginData_ProcessPendingNotification(HWND hwnd) {
    if (!PluginData_BeginUse()) return;

    /* Copy pending notification data under lock, then process outside lock */
    PendingNotification localNotify;

    EnterCriticalSection(&g_dataCS);
    if (!g_pendingNotify.pending) {
        LeaveCriticalSection(&g_dataCS);
        PluginData_EndUse();
        return;
    }

    /* Copy to local and clear pending flag */
    memcpy(&localNotify, &g_pendingNotify, sizeof(PendingNotification));
    g_pendingNotify.pending = FALSE;
    LeaveCriticalSection(&g_dataCS);
    PluginData_EndUse();

    int notifyType = localNotify.type;
    int customTimeout = localNotify.timeout;

    /* Show notification based on type */
    if (notifyType == -1) {
        /* Use default configured type */
        if (customTimeout > 0 &&
            g_AppConfig.notification.display.type == NOTIFICATION_TYPE_CATIME) {
            ShowToastNotificationWithTimeout(hwnd, localNotify.message, customTimeout);
        } else {
            ShowNotification(hwnd, localNotify.message);
        }
    } else if (notifyType == NOTIFICATION_TYPE_CATIME) {
        if (customTimeout > 0) {
            ShowToastNotificationWithTimeout(hwnd, localNotify.message, customTimeout);
        } else {
            ShowToastNotification(hwnd, localNotify.message);
        }
    } else if (notifyType == NOTIFICATION_TYPE_SYSTEM_MODAL) {
        ShowModalNotification(hwnd, localNotify.message);
    } else if (notifyType == NOTIFICATION_TYPE_OS) {
        /* Convert to UTF-8 for tray notification */
        char msgUtf8[2048] = {0};
        if (WideCharToMultiByte(CP_UTF8, 0, localNotify.message, -1, msgUtf8, sizeof(msgUtf8), NULL, NULL) <= 0) {
            LOG_WARNING("PluginData: Failed to convert OS notification message to UTF-8");
            return;
        }
        extern void ShowTrayNotification(HWND hwnd, const char* message);
        ShowTrayNotification(hwnd, msgUtf8);
    }
}
