    size_t pathLen = wcslen(path);
    if (pathLen == 0 || pathLen >= directorySize) return FALSE;

    wcsncpy(directory, path, directorySize - 1);
    directory[directorySize - 1] = L'\0';

    wchar_t* lastSlash = wcsrchr(directory, L'\\');
    wchar_t* lastForwardSlash = wcsrchr(directory, L'/');
    if (!lastSlash || (lastForwardSlash && lastForwardSlash > lastSlash)) {
        lastSlash = lastForwardSlash;
    }
    if (!lastSlash || lastSlash == directory) {
        directory[0] = L'\0';
        return FALSE;
    }

    *lastSlash = L'\0';
    return directory[0] != L'\0';
}

/**
 * @brief Ensure plugin output directory exists
 * @note Only creates directory, does NOT clear or modify output.txt
 *       User may have pre-defined content in output.txt
 */
static void EnsureOutputDirExistsW(const wchar_t* filePath) {
    /* First ensure the directory exists */
    wchar_t dirPath[MAX_PATH];
    if (!filePath || wcslen(filePath) >= MAX_PATH) return;
    wcsncpy(dirPath, filePath, MAX_PATH - 1);
    dirPath[MAX_PATH - 1] = L'\0';

    /* Find last backslash to get directory path */
    wchar_t* lastSlash = wcsrchr(dirPath, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';
        /* Create directory (and parent directories if needed) */
        /* SHCreateDirectoryExW creates all intermediate directories */
        SHCreateDirectoryExW(NULL, dirPath, NULL);
    }
}

static size_t ChooseLastContentCacheCapacity(size_t requiredSize) {
    size_t capacity = PLUGIN_OUTPUT_STACK_BUFFER_BYTES + 1;
    while (capacity < requiredSize && capacity < PLUGIN_LAST_CONTENT_RETAIN_BYTES) {
        capacity *= 2;
    }
    return capacity < requiredSize ? requiredSize : capacity;
}

static BOOL UpdateLastContentCache(const char* content, DWORD contentSize) {
    size_t requiredSize = (size_t)contentSize + 1;
    if (requiredSize > g_lastContentCapacity ||
        (g_lastContentCapacity > PLUGIN_LAST_CONTENT_RETAIN_BYTES &&
         requiredSize <= PLUGIN_LAST_CONTENT_RETAIN_BYTES)) {
        size_t newCapacity = ChooseLastContentCacheCapacity(requiredSize);
        char* newBuf = (char*)realloc(g_lastContent, newCapacity);
        if (!newBuf) {
            LOG_ERROR("PluginData: Failed to resize last-content cache to %zu bytes", newCapacity);
            if (g_lastContent && g_lastContentCapacity > 0) {
                g_lastContent[0] = '\0';
            }
            g_lastContentSize = 0;
            return FALSE;
        }
        g_lastContent = newBuf;
        g_lastContentCapacity = newCapacity;
    }

    g_lastContentSize = requiredSize;
    memcpy(g_lastContent, content, contentSize);
    g_lastContent[contentSize] = '\0';
    return TRUE;
}

static void ClearLastContentCacheLocked(void) {
    if (g_lastContentCapacity > PLUGIN_LAST_CONTENT_RETAIN_BYTES) {
        free(g_lastContent);
        g_lastContent = NULL;
        g_lastContentCapacity = 0;
        g_lastContentSize = 0;
        return;
    }
    if (g_lastContent && g_lastContentSize > 0) {
        g_lastContent[0] = '\0';
    }
    g_lastContentSize = 0;
}

static BOOL ClearPluginDisplayDataLocked(void) {
    BOOL hadDisplayData = g_hasPluginData ||
                          (g_pluginDisplayText && g_pluginDisplayText[0] != L'\0');

    g_hasPluginData = FALSE;
    ClearPluginDisplayTextLocked();
    SetDisplaySourcePathLocked(NULL);
    SetPollIntervalMs(DEFAULT_POLL_INTERVAL_MS);

    return hadDisplayData;
}

static void InvalidateLastOutputFileStateLocked(void) {
    ZeroMemory(&g_lastOutputWriteTime, sizeof(g_lastOutputWriteTime));
    g_lastOutputFileSize = 0;
    g_hasLastOutputFileState = FALSE;
}

static void UpdateLastOutputFileStateLocked(const FILETIME* writeTime, ULONGLONG fileSize) {
    if (writeTime) {
        g_lastOutputWriteTime = *writeTime;
    } else {
        ZeroMemory(&g_lastOutputWriteTime, sizeof(g_lastOutputWriteTime));
    }
    g_lastOutputFileSize = fileSize;
    g_hasLastOutputFileState = TRUE;
}

static void FreePluginDataBuffersLocked(void) {
    if (g_pluginDisplayText) {
        free(g_pluginDisplayText);
        g_pluginDisplayText = NULL;
    }
    g_pluginDisplayTextLen = 0;

    if (g_lastContent) {
        free(g_lastContent);
        g_lastContent = NULL;
    }
    g_lastContentSize = 0;
    g_lastContentCapacity = 0;
}

static void ResetPluginDataStateLocked(void) {
    g_pluginModeActive = FALSE;
    g_hasPluginData = FALSE;
    FreePluginDataBuffersLocked();
    InvalidateLastOutputFileStateLocked();
    SetDisplaySourcePathLocked(NULL);
    ResetPendingNotificationLocked();
    SetDefaultPluginOutputDirectoryLocked();
}

static BOOL CopyLastOutputFileStateLocked(FILETIME* writeTime, ULONGLONG* fileSize) {
    if (!g_hasLastOutputFileState || !writeTime || !fileSize) {
        return FALSE;
    }

    *writeTime = g_lastOutputWriteTime;
    *fileSize = g_lastOutputFileSize;
    return TRUE;
}

static BOOL GetPluginOutputFileStateW(const wchar_t* filePath,
                                      FILETIME* writeTime,
                                      ULONGLONG* fileSize) {
    if (!filePath || !writeTime || !fileSize) return FALSE;

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(filePath, GetFileExInfoStandard, &data)) {
        return FALSE;
    }

    *writeTime = data.ftLastWriteTime;
    *fileSize = ((ULONGLONG)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    return TRUE;
}

static BOOL ProcessPluginOutputFile(const wchar_t* filePath, BOOL forceRefresh,
                                    FILETIME* lastWriteTime, ULONGLONG* lastFileSize) {
    if (!filePath || !lastWriteTime || !lastFileSize) {
        return FALSE;
    }

    HANDLE hFile = CreateFileW(
        filePath,
        GENERIC_READ,
        PLUGIN_OUTPUT_FILE_SHARE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            ZeroMemory(lastWriteTime, sizeof(*lastWriteTime));
            *lastFileSize = 0;

            EnterCriticalSection(&g_dataCS);
            BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();
            BOOL displayChanged = ClearPluginDisplayDataLocked();
            BOOL displayTimerRecheck = hadCatimeTag != PluginDisplayHasCatimeTagLocked();
            ClearLastContentCacheLocked();
            InvalidateLastOutputFileStateLocked();
            LeaveCriticalSection(&g_dataCS);
            if (displayTimerRecheck) {
                QueuePluginDataTimerRecheck();
            }
            if ((displayChanged || displayTimerRecheck) && g_hNotifyWnd) {
                RequestPluginDataRedraw(g_hNotifyWnd);
            }
            return displayChanged;
        }
        return FALSE;
    }

    FILETIME currentWriteTime = {0};
    GetFileTime(hFile, NULL, NULL, &currentWriteTime);

    LARGE_INTEGER sizeValue;
    if (!GetFileSizeEx(hFile, &sizeValue) || sizeValue.QuadPart <= 0) {
        *lastWriteTime = currentWriteTime;
        *lastFileSize = 0;
        EnterCriticalSection(&g_dataCS);
        BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();
        BOOL displayChanged = ClearPluginDisplayDataLocked();
        BOOL timerRecheck = hadCatimeTag != PluginDisplayHasCatimeTagLocked();
        ClearLastContentCacheLocked();
        UpdateLastOutputFileStateLocked(&currentWriteTime, 0);
        LeaveCriticalSection(&g_dataCS);
        CloseHandle(hFile);
        if (timerRecheck) {
            QueuePluginDataTimerRecheck();
        }
        if ((displayChanged || timerRecheck) && g_hNotifyWnd) {
            RequestPluginDataRedraw(g_hNotifyWnd);
        }
        return displayChanged;
    }

    ULONGLONG fileSize64 = (ULONGLONG)sizeValue.QuadPart;
    if (!forceRefresh &&
        CompareFileTime(&currentWriteTime, lastWriteTime) == 0 &&
        fileSize64 == *lastFileSize) {
        CloseHandle(hFile);
        return FALSE;
    }

    if (fileSize64 > MAX_PLUGIN_OUTPUT_BYTES) {
        LOG_WARNING("PluginData: Skipping output.txt larger than %llu bytes",
                    (ULONGLONG)MAX_PLUGIN_OUTPUT_BYTES);
        *lastWriteTime = currentWriteTime;
        *lastFileSize = fileSize64;
        EnterCriticalSection(&g_dataCS);
        BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();
        BOOL displayChanged = ClearPluginDisplayDataLocked();
        BOOL timerRecheck = hadCatimeTag != PluginDisplayHasCatimeTagLocked();
        ClearLastContentCacheLocked();
        UpdateLastOutputFileStateLocked(&currentWriteTime, *lastFileSize);
        LeaveCriticalSection(&g_dataCS);
        CloseHandle(hFile);
        if (timerRecheck) {
            QueuePluginDataTimerRecheck();
        }
        if ((displayChanged || timerRecheck) && g_hNotifyWnd) {
            RequestPluginDataRedraw(g_hNotifyWnd);
        }
        return displayChanged;
    }
