/**
 * @file plugin_manager_state.c
 * @brief Shared plugin-manager state and folder watcher helpers.
 */

#include "plugin_manager_internal.h"

PluginInfo g_plugins[MAX_PLUGINS] = {0};
int g_pluginCount = 0;
CRITICAL_SECTION g_pluginCS;
CRITICAL_SECTION g_pluginLifecycleCS;
BOOL g_pluginManagerInitialized = FALSE;

HANDLE g_hHotReloadThread = NULL;
HANDLE g_hHotReloadStopEvent = NULL;
volatile LONG g_hotReloadRunning = FALSE;
volatile int g_lastRunningPluginIndex = -1;
volatile int g_activePluginIndex = -1;
SRWLOCK g_hotReloadLock = SRWLOCK_INIT;
volatile LONG g_hotReloadRequestGeneration = 0;
BOOL g_hotReloadRequestPending = FALSE;
PluginHotReloadRequest g_hotReloadRequest = {0};
DWORD g_hotReloadStartFailureCooldownUntil = 0;

HANDLE g_hAsyncScanThread = NULL;
HANDLE g_hRetiredAsyncScanThread = NULL;
DirectoryWatcher g_pluginFolderWatcher = {0};
volatile LONG g_asyncScanPending = 0;
volatile LONG g_asyncScanShuttingDown = 0;
volatile LONG g_asyncScanGeneration = 0;
SRWLOCK g_asyncScanLock = SRWLOCK_INIT;
BOOL g_asyncScanHasLastSnapshot = FALSE;
PluginDirSnapshot g_asyncScanLastSnapshot = {0};
BOOL g_asyncScanHasFailureSnapshot = FALSE;
BOOL g_asyncScanFailureHadSnapshot = FALSE;
PluginDirSnapshot g_asyncScanFailureSnapshot = {0};
volatile LONG g_asyncScanFailureCooldownUntil = 0;
BOOL g_pluginLocksInitialized = FALSE;
BOOL g_pluginProcessInitialized = FALSE;

BOOL IsAsyncScanShuttingDown(void) {
    return InterlockedCompareExchange(&g_asyncScanShuttingDown, 0, 0) != 0;
}

BOOL IsAsyncScanGenerationCurrent(LONG generation) {
    return InterlockedCompareExchange(&g_asyncScanGeneration, 0, 0) == generation;
}

BOOL IsHotReloadRunning(void) {
    return InterlockedCompareExchange(&g_hotReloadRunning, FALSE, FALSE) != FALSE;
}

void SetHotReloadRunning(BOOL running) {
    InterlockedExchange(&g_hotReloadRunning, running ? TRUE : FALSE);
}

BOOL IsHotReloadStartFailureCoolingDown(DWORD now) {
    return g_hotReloadStartFailureCooldownUntil != 0 &&
           (LONG)(g_hotReloadStartFailureCooldownUntil - now) > 0;
}

void MarkHotReloadStartFailure(DWORD now) {
    DWORD cooldownUntil = now + HOT_RELOAD_START_FAILURE_COOLDOWN_MS;
    g_hotReloadStartFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
}

BOOL EnterCriticalSectionWithTimeout(CRITICAL_SECTION* cs, DWORD timeoutMs) {
    if (!cs) {
        return FALSE;
    }

    ULONGLONG start = GetTickCount64();
    for (;;) {
        if (TryEnterCriticalSection(cs)) {
            return TRUE;
        }
        if (timeoutMs == 0 || (GetTickCount64() - start) >= timeoutMs) {
            return FALSE;
        }
        Sleep(1);
    }
}

LONG QueueHotReloadRequestLocked(int index,
                                        const wchar_t* name,
                                        const wchar_t* path) {
    if (index < 0 || !name || !path) {
        return 0;
    }

    g_hotReloadRequest.index = index;
    wcsncpy(g_hotReloadRequest.name, name, 63);
    g_hotReloadRequest.name[63] = L'\0';
    wcsncpy(g_hotReloadRequest.path, path, MAX_PATH - 1);
    g_hotReloadRequest.path[MAX_PATH - 1] = L'\0';
    g_hotReloadRequestPending = TRUE;
    return InterlockedIncrement(&g_hotReloadRequestGeneration);
}

BOOL WideToUtf8Fixed(const wchar_t* src, char* dest, int destCount) {
    if (!dest || destCount <= 0) return FALSE;
    dest[0] = '\0';
    if (!src) return FALSE;

    if (WideCharToMultiByte(CP_UTF8, 0, src, -1, dest, destCount, NULL, NULL) <= 0) {
        dest[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

BOOL PluginManager_GetPluginDirW(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0 || bufferSize > (size_t)INT_MAX) {
        return FALSE;
    }
    buffer[0] = L'\0';

    char pluginDirUtf8[MAX_PATH] = {0};
    GetPluginsFolderPath(pluginDirUtf8, MAX_PATH);
    if (pluginDirUtf8[0] == '\0' ||
        MultiByteToWideChar(CP_UTF8, 0, pluginDirUtf8, -1,
                            buffer, (int)bufferSize) <= 0) {
        LOG_ERROR("Failed to resolve plugin directory path");
        return FALSE;
    }

    return TRUE;
}

void OnPluginFolderChanged(void* context) {
    (void)context;
    PluginManager_RequestScanAsync();
}

void StartPluginFolderWatcher(void) {
    wchar_t pluginDir[MAX_PATH];
    if (!PluginManager_GetPluginDirW(pluginDir, MAX_PATH)) {
        LOG_WARNING("Plugin folder watcher could not resolve plugins path");
        return;
    }

    DirectoryWatcher_Start(&g_pluginFolderWatcher,
                           pluginDir,
                           TRUE,
                           FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
                           DIRECTORY_WATCHER_DEFAULT_DEBOUNCE_MS,
                           OnPluginFolderChanged,
                           NULL,
                           "PluginFolderWatcher");
}

void StopPluginFolderWatcher(void) {
    DirectoryWatcher_Stop(&g_pluginFolderWatcher, ASYNC_PLUGIN_SCAN_STOP_TIMEOUT_MS);
}

wchar_t ToLowerAsciiW(wchar_t ch) {
    if (ch >= L'A' && ch <= L'Z') {
        return ch + (L'a' - L'A');
    }
    return ch;
}
