/**
 * @file config_watcher_thread.c
 * @brief Directory notification thread and debounced reload delivery.
 */

#include "config_watcher_internal.h"

BOOL ConfigWatcher_IsValidTargetWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }
    wchar_t className[64] = {
        0
    };
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }
    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}
static void ExtractDirectoryPath(const char* filePath, char* dirPath, size_t dirPathSize) {
    strncpy(dirPath, filePath, dirPathSize - 1);
    dirPath[dirPathSize - 1] = '\0';
    char* lastSlash = strrchr(dirPath, '\\');
    if (!lastSlash) lastSlash = strrchr(dirPath, '/');
    if (lastSlash) {
        *lastSlash = '\0';
    } else {
        strncpy(dirPath, ".\\", dirPathSize - 1);
        dirPath[dirPathSize - 1] = '\0';
    }
}
static void NotifyConfigChanges(HWND hwnd) {
    if (InterlockedCompareExchange(&g_acceptingChanges, 0, 0) == 0) return;
    if (!ConfigWatcher_IsValidTargetWindow(hwnd)) return;
    InterlockedExchange(&g_configReloadDirty, 1);
    if (InterlockedCompareExchange(&g_configReloadPending, 1, 0) != 0) {
        return;
    }
    if (!PostMessage(hwnd, WM_APP_CONFIG_CHANGED, 0, 0)) {
        InterlockedExchange(&g_configReloadPending, 0);
        InterlockedExchange(&g_configReloadDirty, 0);
    }
}
void ConfigWatcher_BeginConfigReloadHandling(void) {
    InterlockedExchange(&g_configReloadDirty, 0);
}
void ConfigWatcher_EndConfigReloadHandling(HWND hwnd) {
    InterlockedExchange(&g_configReloadPending, 0);
    if (InterlockedExchange(&g_configReloadDirty, 0) != 0) {
        NotifyConfigChanges(hwnd);
    }
}
static BOOL ConfigWatcher_ShouldProcessChange(HANDLE stopEvent) {
    if (InterlockedCompareExchange(&g_acceptingChanges, 0, 0) == 0) {
        return FALSE;
    }
    return !stopEvent || WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0;
}
static BOOL BuildConfigWatchPaths(const char* iniPath, wchar_t* outDir, size_t outDirSize, wchar_t* outIni, size_t outIniSize) {
    if (!iniPath || !outDir || outDirSize == 0 || outDirSize > INT_MAX || !outIni || outIniSize == 0 || outIniSize > INT_MAX) {
        return FALSE;
    }
    outDir[0] = L'\0';
    outIni[0] = L'\0';
    char dirPath[MAX_PATH];
    ExtractDirectoryPath(iniPath, dirPath, sizeof(dirPath));
    if (MultiByteToWideChar(CP_UTF8, 0, dirPath, -1, outDir, (int)outDirSize) == 0) {
        outDir[0] = L'\0';
        return FALSE;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, iniPath, -1, outIni, (int)outIniSize) == 0) {
        outIni[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}
static BOOL ReadConfigFileSnapshot(const wchar_t* iniPath, ConfigFileSnapshot* snapshot) {
    if (!iniPath || !snapshot) return FALSE;
    ZeroMemory(snapshot, sizeof(*snapshot));
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(iniPath, GetFileExInfoStandard, &attrs)) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            snapshot->exists = FALSE;
            return TRUE;
        }
        LOG_WARNING("ConfigWatcher: failed to stat config file (error=%lu)", error);
        return FALSE;
    }
    snapshot->exists = TRUE;
    snapshot->lastWriteTime = attrs.ftLastWriteTime;
    snapshot->fileSize = ((ULONGLONG)attrs.nFileSizeHigh << 32) | (ULONGLONG)attrs.nFileSizeLow;
    return TRUE;
}
static BOOL ConfigFileSnapshotChanged(const ConfigFileSnapshot* oldSnapshot, const ConfigFileSnapshot* newSnapshot) {
    if (!oldSnapshot || !newSnapshot) return TRUE;
    return oldSnapshot->exists != newSnapshot->exists || oldSnapshot->fileSize != newSnapshot->fileSize || CompareFileTime(&oldSnapshot->lastWriteTime, &newSnapshot->lastWriteTime) != 0;
}
DWORD WINAPI ConfigWatcher_ThreadProc(LPVOID lpParam) {
    ConfigWatcherThreadContext* context = (ConfigWatcherThreadContext*)lpParam;
    HANDLE stopEvent = context ? context->stopEvent : NULL;
    HWND targetHwnd = context ? context->targetHwnd : NULL;
    if (context) {
        free(context);
    }
    if (!stopEvent) {
        return 0;
    }
    char iniPath[MAX_PATH] = {
        0
    };
    GetConfigPath(iniPath, sizeof(iniPath));
    wchar_t wDir[MAX_PATH] = {
        0
    };
    wchar_t wIni[MAX_PATH] = {
        0
    };
    if (!BuildConfigWatchPaths(iniPath, wDir, MAX_PATH, wIni, MAX_PATH)) {
        CloseHandle(stopEvent);
        return 0;
    }
    HANDLE changeEvent = FindFirstChangeNotificationW(wDir, FALSE, WATCH_CHANGE_FILTER);
    if (changeEvent == INVALID_HANDLE_VALUE) {
        LOG_WARNING("ConfigWatcher: failed to watch config directory (error=%lu)", GetLastError());
        CloseHandle(stopEvent);
        return 0;
    }
    ConfigFileSnapshot lastSnapshot = {
        0
    };
    if (!ReadConfigFileSnapshot(wIni, &lastSnapshot)) {
        lastSnapshot.exists = FALSE;
    }
    HANDLE hEvents[WATCH_EVENT_COUNT] = {
        stopEvent, changeEvent
    };
    for (;;) {
        DWORD wait = WaitForMultipleObjects(WATCH_EVENT_COUNT, hEvents, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) {
            break;
        }
        if (wait == WAIT_FAILED) {
            LOG_WARNING("ConfigWatcher: wait failed (error=%lu)", GetLastError());
            break;
        }
        if (wait == WAIT_OBJECT_0 + 1) {
            if (WaitForSingleObject(stopEvent, DEBOUNCE_DELAY_MS) == WAIT_OBJECT_0) {
                break;
            }
            if (!FindNextChangeNotification(changeEvent)) {
                LOG_WARNING("ConfigWatcher: failed to re-arm directory watch (error=%lu)", GetLastError());
                break;
            }
            ConfigFileSnapshot currentSnapshot = {
                0
            };
            if (!ConfigWatcher_ShouldProcessChange(stopEvent)) {
                break;
            }
            BOOL snapshotOk = ReadConfigFileSnapshot(wIni, &currentSnapshot);
            if (!snapshotOk || ConfigFileSnapshotChanged(&lastSnapshot, &currentSnapshot)) {
                if (snapshotOk) {
                    lastSnapshot = currentSnapshot;
                } else {
                    ZeroMemory(&lastSnapshot, sizeof(lastSnapshot));
                }
                if (ConfigWatcher_ShouldProcessChange(stopEvent)) {
                    InvalidateIniCache();
                    NotifyConfigChanges(targetHwnd);
                }
            }
        }
    }
    FindCloseChangeNotification(changeEvent);
    CloseHandle(stopEvent);
    return 0;
}
