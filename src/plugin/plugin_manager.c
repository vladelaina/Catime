/**
 * @file plugin_manager.c
 * @brief Plugin manager - core plugin lifecycle and state management
 */

#include "plugin/plugin_manager.h"
#include "plugin/plugin_process.h"
#include "plugin/plugin_extensions.h"
#include "plugin/plugin_data.h"
#include "config/config_plugin_security.h"
#include "dialog/dialog_plugin_security.h"
#include "utils/natural_sort.h"
#include "log.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shellapi.h>
#include <limits.h>

#define MAX_PLUGIN_SCAN_ENTRIES 4096
#define ASYNC_PLUGIN_SCAN_STOP_TIMEOUT_MS 2000
#define ASYNC_PLUGIN_SCAN_FAILURE_COOLDOWN_MS 2000
#define HOT_RELOAD_STOP_TIMEOUT_MS 2000
#define HOT_RELOAD_START_FAILURE_COOLDOWN_MS 2000
#define PLUGIN_MANAGER_SHUTDOWN_LOCK_WAIT_MS 2000
#define PLUGIN_SCAN_FAILED (-1)

typedef struct {
    BOOL exists;
    FILETIME lastWriteTime;
} PluginDirSnapshot;

typedef struct {
    PluginDirSnapshot snapshot;
    BOOL hasSnapshot;
    LONG generation;
} AsyncScanThreadParams;

typedef struct {
    int index;
    wchar_t name[64];
    wchar_t path[MAX_PATH];
} PluginHotReloadRequest;

/* Plugin state */
static PluginInfo g_plugins[MAX_PLUGINS];
static int g_pluginCount = 0;
static CRITICAL_SECTION g_pluginCS;
static CRITICAL_SECTION g_pluginLifecycleCS;
static BOOL g_pluginManagerInitialized = FALSE;

/* Hot-reload monitoring */
static HANDLE g_hHotReloadThread = NULL;
static HANDLE g_hHotReloadStopEvent = NULL;
static volatile LONG g_hotReloadRunning = FALSE;
static volatile int g_lastRunningPluginIndex = -1;
static volatile int g_activePluginIndex = -1;
static SRWLOCK g_hotReloadLock = SRWLOCK_INIT;
static volatile LONG g_hotReloadRequestGeneration = 0;
static BOOL g_hotReloadRequestPending = FALSE;
static PluginHotReloadRequest g_hotReloadRequest;
static DWORD g_hotReloadLastStartFailureTick = 0;

/* Async plugin scan state */
static HANDLE g_hAsyncScanThread = NULL;
static HANDLE g_hRetiredAsyncScanThread = NULL;
static volatile LONG g_asyncScanPending = 0;
static volatile LONG g_asyncScanShuttingDown = 0;
static volatile LONG g_asyncScanGeneration = 0;
static SRWLOCK g_asyncScanLock = SRWLOCK_INIT;
static BOOL g_asyncScanHasLastSnapshot = FALSE;
static PluginDirSnapshot g_asyncScanLastSnapshot;
static BOOL g_asyncScanHasFailureSnapshot = FALSE;
static BOOL g_asyncScanFailureHadSnapshot = FALSE;
static PluginDirSnapshot g_asyncScanFailureSnapshot;
static volatile LONG g_asyncScanLastFailureTick = 0;
static BOOL g_pluginLocksInitialized = FALSE;
static BOOL g_pluginProcessInitialized = FALSE;

/* Forward declarations */
static BOOL RestartPluginInternal(int index);
static BOOL RestartPluginInternalWithExpected(int index,
                                             const wchar_t* expectedName,
                                             const wchar_t* expectedPath);
static BOOL StartPluginWithExpectedPath(int index, const wchar_t* expectedPath);
static BOOL StartPluginIfPathMatches(int index, const wchar_t* expectedPath);
static BOOL GetFileModTime(const wchar_t* path, FILETIME* modTime);
static void UpdatePluginLastModTimeIfCurrent(int index, const wchar_t* pluginPath);
static BOOL DetachPluginProcessLocked(int index, PluginInfo* detachedPlugin);
static int DetachAllRunningPluginProcessesLocked(PluginInfo* detachedPlugins, int capacity);
static PluginInfo* AllocatePluginSnapshotArray(void);
static int DetachAndTerminateRunningPluginsIndividually(int skipIndex);
static BOOL PreparePluginLaunchLocked(int index, const wchar_t* expectedPath,
                                      PluginInfo* launchPlugin,
                                      PluginInfo* detachedPlugins,
                                      int* detachedCount,
                                      BOOL* alreadyRunning);
static BOOL LaunchPreparedPlugin(int index, const wchar_t* expectedPath);
static BOOL StopPluginIfPathMatches(int index, const wchar_t* expectedPath, BOOL* pathMatched);
static void StartHotReloadIfNeeded(void);
static void CleanupCompletedHotReloadThreadLocked(void);
static void StopHotReloadThread(BOOL waitIndefinitely);
static void StopHotReloadThreadLocked(BOOL waitIndefinitely);
static void StopHotReloadIfIdle(void);
static BOOL StopAsyncScanThread(void);
static BOOL CleanupRetiredAsyncScanThread(DWORD waitMs);
static BOOL HasRetiredAsyncScanThread(void);
static BOOL GetPluginDirSnapshot(PluginDirSnapshot* snapshot);
static BOOL PluginDirSnapshotsEqual(const PluginDirSnapshot* a,
                                    const PluginDirSnapshot* b);
static BOOL IsAsyncScanFailureRecentlyCachedLocked(BOOL hasSnapshot,
                                                   const PluginDirSnapshot* snapshot,
                                                   DWORD now);
static void MarkAsyncScanFailureLocked(BOOL hasSnapshot,
                                       const PluginDirSnapshot* snapshot);
static void ClearAsyncScanFailureLocked(void);
static LONG QueueHotReloadRequestLocked(int index,
                                        const wchar_t* name,
                                        const wchar_t* path);

static BOOL IsAsyncScanShuttingDown(void) {
    return InterlockedCompareExchange(&g_asyncScanShuttingDown, 0, 0) != 0;
}

static BOOL IsAsyncScanGenerationCurrent(LONG generation) {
    return InterlockedCompareExchange(&g_asyncScanGeneration, 0, 0) == generation;
}

static BOOL IsHotReloadRunning(void) {
    return InterlockedCompareExchange(&g_hotReloadRunning, FALSE, FALSE) != FALSE;
}

static void SetHotReloadRunning(BOOL running) {
    InterlockedExchange(&g_hotReloadRunning, running ? TRUE : FALSE);
}

static BOOL IsHotReloadStartFailureCoolingDown(DWORD now) {
    return g_hotReloadLastStartFailureTick != 0 &&
           (DWORD)(now - g_hotReloadLastStartFailureTick) <
               HOT_RELOAD_START_FAILURE_COOLDOWN_MS;
}

static void MarkHotReloadStartFailure(DWORD now) {
    g_hotReloadLastStartFailureTick = now;
}

static BOOL EnterCriticalSectionWithTimeout(CRITICAL_SECTION* cs, DWORD timeoutMs) {
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

static LONG QueueHotReloadRequestLocked(int index,
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

static BOOL WideToUtf8Fixed(const wchar_t* src, char* dest, int destCount) {
    if (!dest || destCount <= 0) return FALSE;
    dest[0] = '\0';
    if (!src) return FALSE;

    if (WideCharToMultiByte(CP_UTF8, 0, src, -1, dest, destCount, NULL, NULL) <= 0) {
        dest[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL PluginManager_GetPluginDirW(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0 || bufferSize > (size_t)MAXDWORD) {
        return FALSE;
    }
    buffer[0] = L'\0';

    DWORD result = ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Catime\\resources\\plugins",
        buffer,
        (DWORD)bufferSize);
    if (result == 0 || result >= bufferSize) {
        LOG_ERROR("Failed to expand plugin directory path");
        buffer[0] = L'\0';
        return FALSE;
    }

    return TRUE;
}

static wchar_t ToLowerAsciiW(wchar_t ch) {
    if (ch >= L'A' && ch <= L'Z') {
        return ch + (L'a' - L'A');
    }
    return ch;
}

static BOOL MatchesPluginPatternExtension(const wchar_t* ext, const char* pattern) {
    if (!ext || !pattern) return FALSE;
    if (pattern[0] == '*') {
        pattern++;
    }

    while (*ext && *pattern) {
        if (ToLowerAsciiW(*ext) != ToLowerAsciiW((wchar_t)(unsigned char)*pattern)) {
            return FALSE;
        }
        ext++;
        pattern++;
    }

    return *ext == L'\0' && *pattern == '\0';
}

static BOOL IsSupportedPluginFileW(const wchar_t* fileName) {
    const wchar_t* ext = wcsrchr(fileName, L'.');
    if (!ext) return FALSE;

    for (size_t i = 0; i < PLUGIN_EXTENSION_COUNT; i++) {
        if (MatchesPluginPatternExtension(ext, PLUGIN_EXTENSIONS[i])) {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief Comparator for plugin sorting (by display name, natural order)
 */
static int ComparePluginInfo(const void* a, const void* b) {
    const PluginInfo* pa = (const PluginInfo*)a;
    const PluginInfo* pb = (const PluginInfo*)b;
    return NaturalCompareW(pa->displayName, pb->displayName);
}

/**
 * @brief Get file modification time
 */
static BOOL GetFileModTime(const wchar_t* path, FILETIME* modTime) {
    HANDLE hFile = CreateFileW(path, GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    BOOL result = GetFileTime(hFile, NULL, NULL, modTime);
    CloseHandle(hFile);
    return result;
}

/**
 * @brief Hot-reload monitoring thread
 */
static DWORD WINAPI HotReloadThread(LPVOID lpParam) {
    (void)lpParam;

    while (IsHotReloadRunning()) {
        if (g_hHotReloadStopEvent &&
            WaitForSingleObject(g_hHotReloadStopEvent, 1000) == WAIT_OBJECT_0) {
            break;
        }
        if (!IsHotReloadRunning()) break;

        int indexToMonitor = -1;
        wchar_t pathToCheck[MAX_PATH] = {0};
        wchar_t nameToCheck[64] = {0};
        FILETIME lastModTime = {0};

        EnterCriticalSection(&g_pluginCS);

        /* Find running plugin or last running */
        for (int i = 0; i < g_pluginCount; i++) {
            if (g_plugins[i].isRunning) {
                indexToMonitor = i;
                g_lastRunningPluginIndex = i;
                break;
            }
        }
        if (indexToMonitor < 0 && g_lastRunningPluginIndex >= 0 &&
            g_lastRunningPluginIndex < g_pluginCount) {
            indexToMonitor = g_lastRunningPluginIndex;
        }

        if (indexToMonitor >= 0) {
            wcsncpy(pathToCheck, g_plugins[indexToMonitor].path, MAX_PATH - 1);
            pathToCheck[MAX_PATH - 1] = L'\0';
            wcsncpy(nameToCheck, g_plugins[indexToMonitor].name, 63);
            nameToCheck[63] = L'\0';
            lastModTime = g_plugins[indexToMonitor].lastModTime;
        }

        LeaveCriticalSection(&g_pluginCS);

        if (indexToMonitor >= 0) {
            FILETIME currentModTime;
            if (GetFileModTime(pathToCheck, &currentModTime) &&
                CompareFileTime(&currentModTime, &lastModTime) != 0) {
                BOOL shouldPostReload = FALSE;

                EnterCriticalSection(&g_pluginCS);
                if (indexToMonitor < g_pluginCount &&
                    wcscmp(g_plugins[indexToMonitor].name, nameToCheck) == 0 &&
                    wcscmp(g_plugins[indexToMonitor].path, pathToCheck) == 0 &&
                    CompareFileTime(&currentModTime, &g_plugins[indexToMonitor].lastModTime) != 0) {
                    g_plugins[indexToMonitor].lastModTime = currentModTime;
                    shouldPostReload = TRUE;
                }
                LeaveCriticalSection(&g_pluginCS);

                if (shouldPostReload) {
                    /* Post message to main thread instead of calling directly */
                    /* This avoids deadlock when security dialog needs to be shown */
                    HWND hwnd = PluginProcess_GetNotifyWindow();
                    if (hwnd) {
                        LONG requestGeneration = 0;
                        EnterCriticalSection(&g_pluginCS);
                        requestGeneration = QueueHotReloadRequestLocked(indexToMonitor,
                                                                        nameToCheck,
                                                                        pathToCheck);
                        LeaveCriticalSection(&g_pluginCS);

                        if (requestGeneration == 0 ||
                            !PostMessage(hwnd, WM_PLUGIN_HOT_RELOAD,
                                         (WPARAM)requestGeneration, 0)) {
                            EnterCriticalSection(&g_pluginCS);
                            if (InterlockedCompareExchange(&g_hotReloadRequestGeneration,
                                                           0, 0) == requestGeneration) {
                                g_hotReloadRequestPending = FALSE;
                            }
                            LeaveCriticalSection(&g_pluginCS);
                        }
                    } else {
                        /* No window available - skip hot-reload this cycle */
                        /* Window should be set during initialization */
                        LOG_WARNING("[HotReload] No notify window, skipping reload");
                    }
                }
            }
        }
    }

    return 0;
}

static BOOL AnyPluginRunningLocked(void) {
    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            return TRUE;
        }
    }
    return FALSE;
}

static void StartHotReloadIfNeeded(void) {
    AcquireSRWLockExclusive(&g_hotReloadLock);

    CleanupCompletedHotReloadThreadLocked();
    if (g_hHotReloadThread) {
        ReleaseSRWLockExclusive(&g_hotReloadLock);
        return;
    }

    DWORD now = GetTickCount();
    if (IsHotReloadStartFailureCoolingDown(now)) {
        ReleaseSRWLockExclusive(&g_hotReloadLock);
        return;
    }

    SetHotReloadRunning(TRUE);
    g_hHotReloadStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_hHotReloadStopEvent) {
        SetHotReloadRunning(FALSE);
        LOG_WARNING("Failed to create hot-reload stop event");
        MarkHotReloadStartFailure(now);
        ReleaseSRWLockExclusive(&g_hotReloadLock);
        return;
    }

    g_hHotReloadThread = CreateThread(NULL, 0, HotReloadThread, NULL, 0, NULL);
    if (!g_hHotReloadThread) {
        LOG_WARNING("Failed to start hot-reload thread");
        CloseHandle(g_hHotReloadStopEvent);
        g_hHotReloadStopEvent = NULL;
        SetHotReloadRunning(FALSE);
        MarkHotReloadStartFailure(now);
    } else {
        g_hotReloadLastStartFailureTick = 0;
    }

    ReleaseSRWLockExclusive(&g_hotReloadLock);
}

static void CleanupCompletedHotReloadThreadLocked(void) {
    if (!g_hHotReloadThread) return;

    DWORD waitResult = WaitForSingleObject(g_hHotReloadThread, 0);
    if (waitResult == WAIT_TIMEOUT) {
        return;
    }
    if (waitResult == WAIT_FAILED) {
        LOG_WARNING("Hot-reload thread status check failed: %lu", GetLastError());
        return;
    }

    CloseHandle(g_hHotReloadThread);
    g_hHotReloadThread = NULL;

    if (g_hHotReloadStopEvent) {
        CloseHandle(g_hHotReloadStopEvent);
        g_hHotReloadStopEvent = NULL;
    }
    SetHotReloadRunning(FALSE);
}

static void StopHotReloadThreadLocked(BOOL waitIndefinitely) {
    CleanupCompletedHotReloadThreadLocked();

    if (g_hHotReloadThread) {
        SetHotReloadRunning(FALSE);
        if (g_hHotReloadStopEvent) {
            SetEvent(g_hHotReloadStopEvent);
        }
        DWORD waitMs = waitIndefinitely ? INFINITE : HOT_RELOAD_STOP_TIMEOUT_MS;
        DWORD waitResult = WaitForSingleObject(g_hHotReloadThread, waitMs);
        if (waitResult == WAIT_TIMEOUT) {
            LOG_WARNING("Hot-reload thread stop timed out after %lu ms",
                        (DWORD)HOT_RELOAD_STOP_TIMEOUT_MS);
            return;
        }
        if (waitResult == WAIT_FAILED) {
            LOG_WARNING("Hot-reload thread stop wait failed: %lu", GetLastError());
            return;
        }
        CloseHandle(g_hHotReloadThread);
        g_hHotReloadThread = NULL;
    } else {
        SetHotReloadRunning(FALSE);
    }
    if (g_hHotReloadStopEvent) {
        CloseHandle(g_hHotReloadStopEvent);
        g_hHotReloadStopEvent = NULL;
    }
}

static void StopHotReloadThread(BOOL waitIndefinitely) {
    AcquireSRWLockExclusive(&g_hotReloadLock);
    StopHotReloadThreadLocked(waitIndefinitely);
    ReleaseSRWLockExclusive(&g_hotReloadLock);
}

static void StopHotReloadIfIdle(void) {
    BOOL hasRunningPlugin = FALSE;

    if (!g_pluginManagerInitialized) return;

    AcquireSRWLockExclusive(&g_hotReloadLock);
    if (!g_hHotReloadThread) {
        ReleaseSRWLockExclusive(&g_hotReloadLock);
        return;
    }

    EnterCriticalSection(&g_pluginCS);
    hasRunningPlugin = AnyPluginRunningLocked();
    LeaveCriticalSection(&g_pluginCS);

    if (!hasRunningPlugin) {
        StopHotReloadThreadLocked(FALSE);
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
    g_hotReloadLastStartFailureTick = 0;
    g_hAsyncScanThread = NULL;
    g_asyncScanHasLastSnapshot = FALSE;
    ZeroMemory(&g_asyncScanLastSnapshot, sizeof(g_asyncScanLastSnapshot));
    g_asyncScanHasFailureSnapshot = FALSE;
    g_asyncScanFailureHadSnapshot = FALSE;
    ZeroMemory(&g_asyncScanFailureSnapshot, sizeof(g_asyncScanFailureSnapshot));
    InterlockedExchange(&g_asyncScanLastFailureTick, 0);

    /* Initialize process management */
    if (!g_pluginProcessInitialized) {
        g_pluginProcessInitialized = PluginProcess_Init();
    }

    LOG_INFO("Plugin manager initialized");
}

void PluginManager_Shutdown(void) {
    if (!g_pluginManagerInitialized && !g_pluginLocksInitialized) return;

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
        StopHotReloadThread(FALSE);
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

    StopHotReloadThread(TRUE);

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
    if (asyncScanStopped) {
        DeleteCriticalSection(&g_pluginCS);
        DeleteCriticalSection(&g_pluginLifecycleCS);
        g_pluginLocksInitialized = FALSE;
    } else {
        LOG_WARNING("Plugin manager locks retained because async scan did not stop before shutdown");
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

static BOOL GetPluginDirSnapshot(PluginDirSnapshot* snapshot) {
    if (!snapshot) return FALSE;
    ZeroMemory(snapshot, sizeof(*snapshot));

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
    snapshot->lastWriteTime = attrs.ftLastWriteTime;
    return TRUE;
}

static BOOL PluginDirSnapshotsEqual(const PluginDirSnapshot* a,
                                    const PluginDirSnapshot* b) {
    if (!a || !b) return FALSE;
    if (a->exists != b->exists) return FALSE;
    if (!a->exists) return TRUE;

    return CompareFileTime(&a->lastWriteTime, &b->lastWriteTime) == 0;
}

static BOOL IsAsyncScanFailureRecentlyCachedLocked(BOOL hasSnapshot,
                                                   const PluginDirSnapshot* snapshot,
                                                   DWORD now) {
    DWORD lastFailureTick =
        (DWORD)InterlockedCompareExchange(&g_asyncScanLastFailureTick, 0, 0);
    if (!g_asyncScanHasFailureSnapshot ||
        lastFailureTick == 0 ||
        (DWORD)(now - lastFailureTick) >= ASYNC_PLUGIN_SCAN_FAILURE_COOLDOWN_MS) {
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
    InterlockedExchange(&g_asyncScanLastFailureTick, (LONG)GetTickCount());
}

static void ClearAsyncScanFailureLocked(void) {
    g_asyncScanHasFailureSnapshot = FALSE;
    g_asyncScanFailureHadSnapshot = FALSE;
    ZeroMemory(&g_asyncScanFailureSnapshot, sizeof(g_asyncScanFailureSnapshot));
    InterlockedExchange(&g_asyncScanLastFailureTick, 0);
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
    BOOL scanCancelled = FALSE;
    int scanResult = PLUGIN_SCAN_FAILED;

    if (!newPlugins || !removedPlugins) {
        LOG_WARNING("Failed to allocate plugin scan buffers");
        goto cleanup;
    }

    wchar_t searchPath[MAX_PATH];
    int searchWritten = _snwprintf_s(searchPath, MAX_PATH, _TRUNCATE, L"%s\\*", pluginDir);
    if (searchWritten < 0) {
        LOG_WARNING("Plugin search path is too long");
        goto cleanup;
    }

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);

    if (hFind != INVALID_HANDLE_VALUE) {
        int scannedEntries = 0;
        BOOL stoppedEarly = FALSE;
        do {
            if (IsAsyncScanShuttingDown() ||
                !IsAsyncScanGenerationCurrent(generation)) {
                scanCancelled = TRUE;
                stoppedEarly = TRUE;
                break;
            }
            if (++scannedEntries > MAX_PLUGIN_SCAN_ENTRIES) {
                LOG_WARNING("Plugin directory scan limit reached (%d entries)",
                            MAX_PLUGIN_SCAN_ENTRIES);
                stoppedEarly = TRUE;
                break;
            }
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            if (!IsSupportedPluginFileW(findData.cFileName)) {
                continue;
            }
            if (newPluginCount >= MAX_PLUGINS) {
                LOG_WARNING("Maximum plugin count reached (%d)", MAX_PLUGINS);
                stoppedEarly = TRUE;
                break;
            }

            PluginInfo* plugin = &newPlugins[newPluginCount];
            memset(plugin, 0, sizeof(*plugin));

            wcsncpy(plugin->name, findData.cFileName, 63);
            plugin->name[63] = L'\0';
            ExtractDisplayName(findData.cFileName, plugin->displayName, 64);
            int pathWritten = _snwprintf_s(plugin->path, MAX_PATH, _TRUNCATE,
                                           L"%s\\%s", pluginDir, findData.cFileName);
            if (pathWritten < 0) {
                LOG_WARNING("Plugin path is too long: %ls", findData.cFileName);
                continue;
            }

            plugin->isRunning = FALSE;
            memset(&plugin->pi, 0, sizeof(plugin->pi));

            newPluginCount++;
        } while (FindNextFileW(hFind, &findData));

        DWORD findError = stoppedEarly ? ERROR_SUCCESS : GetLastError();
        FindClose(hFind);
        if (scanCancelled || IsAsyncScanShuttingDown() ||
            !IsAsyncScanGenerationCurrent(generation)) {
            goto cleanup;
        }
        if (!stoppedEarly && findError != ERROR_NO_MORE_FILES) {
            LOG_WARNING("Plugin directory enumeration failed (error=%lu)", findError);
            goto cleanup;
        }
    } else {
        DWORD findError = GetLastError();
        if (IsAsyncScanShuttingDown() ||
            !IsAsyncScanGenerationCurrent(generation)) {
            goto cleanup;
        }
        if (findError != ERROR_FILE_NOT_FOUND &&
            findError != ERROR_PATH_NOT_FOUND &&
            findError != ERROR_NO_MORE_FILES &&
            findError != ERROR_DIRECTORY) {
            LOG_WARNING("Failed to scan plugin directory (error=%lu)", findError);
            goto cleanup;
        }
    }

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
            if (wcscmp(g_plugins[i].name, newPlugins[j].name) == 0) {
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
                if (wcscmp(g_plugins[i].name, newPlugins[j].name) == 0) {
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

    // Remember old plugin names for re-mapping indices
    wchar_t lastRunningName[64] = {0};
    wchar_t activePluginName[64] = {0};
    if (g_lastRunningPluginIndex >= 0 && g_lastRunningPluginIndex < g_pluginCount) {
        wcsncpy(lastRunningName, g_plugins[g_lastRunningPluginIndex].name, 63);
        lastRunningName[63] = L'\0';
    }
    if (g_activePluginIndex >= 0 && g_activePluginIndex < g_pluginCount) {
        wcsncpy(activePluginName, g_plugins[g_activePluginIndex].name, 63);
        activePluginName[63] = L'\0';
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
    if (lastRunningName[0]) {
        g_lastRunningPluginIndex = -1;  // Reset first
        for (int i = 0; i < g_pluginCount; i++) {
            if (wcscmp(g_plugins[i].name, lastRunningName) == 0) {
                g_lastRunningPluginIndex = i;
                break;
            }
        }
    }

    // Re-map g_activePluginIndex to new list
    if (activePluginName[0]) {
        g_activePluginIndex = -1;  // Reset first
        for (int i = 0; i < g_pluginCount; i++) {
            if (wcscmp(g_plugins[i].name, activePluginName) == 0) {
                g_activePluginIndex = i;
                break;
            }
        }
    }

    LeaveCriticalSection(&g_pluginCS);

    for (int i = 0; i < removedPluginCount; i++) {
        PluginProcess_TerminateDetached(&removedPlugins[i]);
    }
    if (shouldClearDisplay) {
        PluginData_Clear();
    }
    if (removedPluginCount > 0) {
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

static DWORD WINAPI AsyncScanThread(LPVOID lpParam) {
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

static BOOL CleanupRetiredAsyncScanThread(DWORD waitMs) {
    HANDLE hThread = NULL;
    HANDLE hThreadToClose = NULL;

    AcquireSRWLockExclusive(&g_asyncScanLock);
    hThread = g_hRetiredAsyncScanThread;
    ReleaseSRWLockExclusive(&g_asyncScanLock);

    if (!hThread) {
        return TRUE;
    }

    DWORD waitResult = WaitForSingleObject(hThread, waitMs);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_FAILED) {
            LOG_WARNING("Retired async plugin scan wait failed: %lu", GetLastError());
        }
        return FALSE;
    }

    AcquireSRWLockExclusive(&g_asyncScanLock);
    if (g_hRetiredAsyncScanThread == hThread) {
        g_hRetiredAsyncScanThread = NULL;
        hThreadToClose = hThread;
    }
    ReleaseSRWLockExclusive(&g_asyncScanLock);

    if (hThreadToClose) {
        CloseHandle(hThreadToClose);
    }

    return TRUE;
}

static BOOL HasRetiredAsyncScanThread(void) {
    BOOL hasThread = FALSE;

    AcquireSRWLockShared(&g_asyncScanLock);
    hasThread = (g_hRetiredAsyncScanThread != NULL);
    ReleaseSRWLockShared(&g_asyncScanLock);

    return hasThread;
}

static BOOL StopAsyncScanThread(void) {
    HANDLE hThread = NULL;

    AcquireSRWLockExclusive(&g_asyncScanLock);
    InterlockedExchange(&g_asyncScanShuttingDown, 1);

    if (g_hAsyncScanThread) {
        hThread = g_hAsyncScanThread;
        g_hAsyncScanThread = NULL;
    }

    InterlockedExchange(&g_asyncScanPending, 0);
    ReleaseSRWLockExclusive(&g_asyncScanLock);

    if (hThread) {
        DWORD waitResult = WaitForSingleObject(hThread, ASYNC_PLUGIN_SCAN_STOP_TIMEOUT_MS);
        if (waitResult != WAIT_OBJECT_0) {
            LOG_WARNING("Async plugin scan stop timed out after %lu ms (wait=%lu, error=%lu)",
                        (DWORD)ASYNC_PLUGIN_SCAN_STOP_TIMEOUT_MS,
                        waitResult,
                        GetLastError());
            AcquireSRWLockExclusive(&g_asyncScanLock);
            if (!g_hRetiredAsyncScanThread) {
                g_hRetiredAsyncScanThread = hThread;
                hThread = NULL;
            }
            ReleaseSRWLockExclusive(&g_asyncScanLock);
            if (hThread) {
                CloseHandle(hThread);
            }
            return FALSE;
        }
        CloseHandle(hThread);
    }

    return TRUE;
}

void PluginManager_RequestScanAsync(void) {
    PluginDirSnapshot currentSnapshot = {0};
    BOOL hasCurrentSnapshot = FALSE;
    AsyncScanThreadParams* threadParams = NULL;

    if (!CleanupRetiredAsyncScanThread(0)) {
        return;
    }

    AcquireSRWLockExclusive(&g_asyncScanLock);

    if (!g_hRetiredAsyncScanThread &&
        InterlockedCompareExchange(&g_asyncScanShuttingDown, 0, 0) != 0) {
        InterlockedExchange(&g_asyncScanShuttingDown, 0);
    }

    if (InterlockedCompareExchange(&g_asyncScanShuttingDown, 0, 0) != 0) {
        ReleaseSRWLockExclusive(&g_asyncScanLock);
        return;
    }

    /* Avoid multiple concurrent scans */
    if (InterlockedCompareExchange(&g_asyncScanPending, 1, 0) != 0) {
        ReleaseSRWLockExclusive(&g_asyncScanLock);
        return;
    }

    if (g_hAsyncScanThread) {
        DWORD wait = WaitForSingleObject(g_hAsyncScanThread, 0);
        if (wait == WAIT_OBJECT_0) {
            CloseHandle(g_hAsyncScanThread);
            g_hAsyncScanThread = NULL;
        } else {
            InterlockedExchange(&g_asyncScanPending, 0);
            ReleaseSRWLockExclusive(&g_asyncScanLock);
            return;
        }
    }

    ReleaseSRWLockExclusive(&g_asyncScanLock);

    hasCurrentSnapshot = GetPluginDirSnapshot(&currentSnapshot);

    threadParams = (AsyncScanThreadParams*)malloc(sizeof(*threadParams));
    if (threadParams) {
        ZeroMemory(threadParams, sizeof(*threadParams));
        threadParams->snapshot = currentSnapshot;
        threadParams->hasSnapshot = hasCurrentSnapshot;
        threadParams->generation =
            InterlockedCompareExchange(&g_asyncScanGeneration, 0, 0);
    }

    AcquireSRWLockExclusive(&g_asyncScanLock);

    if (InterlockedCompareExchange(&g_asyncScanShuttingDown, 0, 0) != 0) {
        free(threadParams);
        InterlockedExchange(&g_asyncScanPending, 0);
        ReleaseSRWLockExclusive(&g_asyncScanLock);
        return;
    }

    if (hasCurrentSnapshot && g_asyncScanHasLastSnapshot &&
        PluginDirSnapshotsEqual(&currentSnapshot, &g_asyncScanLastSnapshot)) {
        free(threadParams);
        InterlockedExchange(&g_asyncScanPending, 0);
        ReleaseSRWLockExclusive(&g_asyncScanLock);
        return;
    }

    if (IsAsyncScanFailureRecentlyCachedLocked(hasCurrentSnapshot,
                                               &currentSnapshot,
                                               GetTickCount())) {
        free(threadParams);
        InterlockedExchange(&g_asyncScanPending, 0);
        ReleaseSRWLockExclusive(&g_asyncScanLock);
        return;
    }

    if (!threadParams) {
        MarkAsyncScanFailureLocked(hasCurrentSnapshot, &currentSnapshot);
        InterlockedExchange(&g_asyncScanPending, 0);
        ReleaseSRWLockExclusive(&g_asyncScanLock);
        return;
    }

    HANDLE hThread = CreateThread(NULL, 0, AsyncScanThread, threadParams, 0, NULL);
    if (hThread) {
        g_hAsyncScanThread = hThread;
    } else {
        free(threadParams);
        MarkAsyncScanFailureLocked(hasCurrentSnapshot, &currentSnapshot);
        InterlockedExchange(&g_asyncScanPending, 0);
    }

    ReleaseSRWLockExclusive(&g_asyncScanLock);
}

int PluginManager_GetPluginCount(void) {
    if (!g_pluginManagerInitialized) return 0;
    int count;
    EnterCriticalSection(&g_pluginCS);
    count = g_pluginCount;
    LeaveCriticalSection(&g_pluginCS);
    return count;
}

BOOL PluginManager_CopyPlugin(int index, PluginInfo* outPlugin) {
    if (!g_pluginManagerInitialized || !outPlugin) {
        return FALSE;
    }

    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    *outPlugin = g_plugins[index];
    memset(&outPlugin->pi, 0, sizeof(outPlugin->pi));
    LeaveCriticalSection(&g_pluginCS);
    return TRUE;
}

static BOOL DetachPluginProcessLocked(int index, PluginInfo* detachedPlugin) {
    if (!detachedPlugin || index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    PluginInfo* plugin = &g_plugins[index];
    if (!plugin->isRunning) {
        return FALSE;
    }

    *detachedPlugin = *plugin;
    plugin->isRunning = FALSE;
    memset(&plugin->pi, 0, sizeof(plugin->pi));

    return TRUE;
}

static int DetachAllRunningPluginProcessesLocked(PluginInfo* detachedPlugins, int capacity) {
    if (!detachedPlugins || capacity <= 0) {
        return 0;
    }

    int detachedCount = 0;
    for (int i = 0; i < g_pluginCount && detachedCount < capacity; i++) {
        if (DetachPluginProcessLocked(i, &detachedPlugins[detachedCount])) {
            detachedCount++;
        }
    }

    return detachedCount;
}

static PluginInfo* AllocatePluginSnapshotArray(void) {
    return (PluginInfo*)calloc(MAX_PLUGINS, sizeof(PluginInfo));
}

static int DetachAndTerminateRunningPluginsIndividually(int skipIndex) {
    int detachedCount = 0;

    for (;;) {
        PluginInfo detachedPlugin;
        memset(&detachedPlugin, 0, sizeof(detachedPlugin));
        BOOL detached = FALSE;

        EnterCriticalSection(&g_pluginCS);
        for (int i = 0; i < g_pluginCount; i++) {
            if (i == skipIndex) {
                continue;
            }
            if (DetachPluginProcessLocked(i, &detachedPlugin)) {
                detached = TRUE;
                g_lastRunningPluginIndex = -1;
                g_activePluginIndex = -1;
                break;
            }
        }
        LeaveCriticalSection(&g_pluginCS);

        if (!detached) {
            break;
        }

        PluginProcess_TerminateDetached(&detachedPlugin);
        detachedCount++;
    }

    return detachedCount;
}

static BOOL PreparePluginLaunchLocked(int index, const wchar_t* expectedPath,
                                      PluginInfo* launchPlugin,
                                      PluginInfo* detachedPlugins,
                                      int* detachedCount,
                                      BOOL* alreadyRunning) {
    if (!launchPlugin || !detachedPlugins || !detachedCount || !alreadyRunning) {
        return FALSE;
    }

    *detachedCount = 0;
    *alreadyRunning = FALSE;

    if (!g_pluginManagerInitialized || index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    const PluginInfo* plugin = &g_plugins[index];
    if (expectedPath && wcscmp(plugin->path, expectedPath) != 0) {
        LOG_WARNING("Plugin changed before launch; launch cancelled");
        PluginProcess_SetLastError(L"File changed");
        return FALSE;
    }

    if (plugin->isRunning) {
        *alreadyRunning = TRUE;
        return TRUE;
    }

    *launchPlugin = *plugin;
    launchPlugin->isRunning = FALSE;
    memset(&launchPlugin->pi, 0, sizeof(launchPlugin->pi));

    for (int i = 0; i < g_pluginCount && *detachedCount < MAX_PLUGINS; i++) {
        if (i == index || !g_plugins[i].isRunning) {
            continue;
        }

        if (DetachPluginProcessLocked(i, &detachedPlugins[*detachedCount])) {
            (*detachedCount)++;
        }
    }

    if (*detachedCount > 0) {
        g_lastRunningPluginIndex = -1;
        g_activePluginIndex = -1;
    }

    return TRUE;
}

static BOOL LaunchPreparedPlugin(int index, const wchar_t* expectedPath) {
    if (!g_pluginManagerInitialized) return FALSE;

    PluginInfo launchPlugin;
    PluginInfo* detachedPlugins = AllocatePluginSnapshotArray();
    if (!detachedPlugins) {
        LOG_ERROR("Failed to allocate detached plugin snapshots");
        PluginProcess_SetLastError(L"Internal error");
        return FALSE;
    }
    int detachedCount = 0;
    BOOL alreadyRunning = FALSE;

    EnterCriticalSection(&g_pluginLifecycleCS);
    EnterCriticalSection(&g_pluginCS);

    BOOL prepared = PreparePluginLaunchLocked(index, expectedPath, &launchPlugin,
                                              detachedPlugins, &detachedCount,
                                              &alreadyRunning);

    LeaveCriticalSection(&g_pluginCS);

    if (!prepared) {
        LeaveCriticalSection(&g_pluginLifecycleCS);
        free(detachedPlugins);
        return FALSE;
    }
    if (alreadyRunning) {
        LeaveCriticalSection(&g_pluginLifecycleCS);
        free(detachedPlugins);
        return TRUE;
    }

    PluginProcess_TerminateAllOrphans();
    for (int i = 0; i < detachedCount; i++) {
        PluginProcess_TerminateDetached(&detachedPlugins[i]);
    }

    if (!PluginProcess_Launch(&launchPlugin)) {
        LOG_ERROR("Failed to launch plugin: %ls", launchPlugin.displayName);
        LeaveCriticalSection(&g_pluginLifecycleCS);
        free(detachedPlugins);
        return FALSE;
    }

    EnterCriticalSection(&g_pluginCS);

    if (!g_pluginManagerInitialized || index < 0 || index >= g_pluginCount ||
        (expectedPath && wcscmp(g_plugins[index].path, expectedPath) != 0)) {
        LOG_WARNING("Plugin changed while launch was running; launched process will be stopped");
        PluginProcess_SetLastError(L"File changed");
        LeaveCriticalSection(&g_pluginCS);
        PluginProcess_TerminateDetached(&launchPlugin);
        LeaveCriticalSection(&g_pluginLifecycleCS);
        free(detachedPlugins);
        return FALSE;
    }

    g_plugins[index].isRunning = launchPlugin.isRunning;
    g_plugins[index].pi = launchPlugin.pi;

    BOOL stillRunning = PluginProcess_IsAlive(&g_plugins[index]);
    if (stillRunning) {
        g_activePluginIndex = index;
        g_lastRunningPluginIndex = -1;
    } else {
        PluginProcess_SetLastError(L"Exited");
        LOG_WARNING("Plugin exited before startup completed: %ls", launchPlugin.displayName);
    }
    LeaveCriticalSection(&g_pluginCS);

    LeaveCriticalSection(&g_pluginLifecycleCS);
    free(detachedPlugins);
    return stillRunning;
}

static void UpdatePluginLastModTimeIfCurrent(int index, const wchar_t* pluginPath) {
    if (!pluginPath) return;

    FILETIME modTime = {0};
    if (!GetFileModTime(pluginPath, &modTime)) {
        return;
    }

    if (!g_pluginManagerInitialized) return;
    EnterCriticalSection(&g_pluginCS);

    if (index >= 0 && index < g_pluginCount &&
        wcscmp(g_plugins[index].path, pluginPath) == 0 &&
        g_plugins[index].isRunning) {
        g_plugins[index].lastModTime = modTime;
    }

    LeaveCriticalSection(&g_pluginCS);
}

static BOOL StartPluginWithExpectedPath(int index, const wchar_t* expectedPath) {
    if (!g_pluginManagerInitialized) return FALSE;

    wchar_t pluginPath[MAX_PATH];
    wchar_t pluginDisplayName[64];
    BOOL alreadyRunning = FALSE;

    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    if (expectedPath && wcscmp(g_plugins[index].path, expectedPath) != 0) {
        LOG_WARNING("Plugin changed before start; start cancelled");
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    /* Copy plugin info before doing file hashing/trust checks outside the lock. */
    alreadyRunning = g_plugins[index].isRunning;
    wcsncpy(pluginPath, g_plugins[index].path, MAX_PATH - 1);
    pluginPath[MAX_PATH - 1] = L'\0';
    wcsncpy(pluginDisplayName, g_plugins[index].displayName, 63);
    pluginDisplayName[63] = L'\0';

    LeaveCriticalSection(&g_pluginCS);

    if (alreadyRunning) {
        return TRUE;
    }

    /* Convert to UTF-8 for security check functions */
    char pluginPathUtf8[MAX_PATH];
    if (!WideToUtf8Fixed(pluginPath, pluginPathUtf8, MAX_PATH)) {
        LOG_ERROR("Failed to convert plugin path to UTF-8: %ls", pluginPath);
        return FALSE;
    }

    /* Security check: verify plugin trust before launching */
    if (!IsPluginTrusted(pluginPathUtf8)) {
        LOG_INFO("Plugin not trusted, showing security dialog: %ls", pluginDisplayName);

        /* Show modeless security confirmation dialog */
        HWND hwnd = PluginProcess_GetNotifyWindow();
        if (IsPluginSecurityDialogOpen()) {
            ShowPluginSecurityDialog(hwnd, pluginPathUtf8, "", index, "");
            return FALSE;
        }

        /* Calculate and save hash at dialog show time for later verification */
        char pluginHash[65] = {0};
        if (!CalculatePluginHash(pluginPathUtf8, pluginHash)) {
            LOG_ERROR("Failed to calculate plugin hash for security dialog");
        }

        char displayNameUtf8[128];
        if (!WideToUtf8Fixed(pluginDisplayName, displayNameUtf8, 128)) {
            LOG_ERROR("Failed to convert plugin display name to UTF-8: %ls", pluginDisplayName);
            return FALSE;
        }
        ShowPluginSecurityDialog(hwnd, pluginPathUtf8, displayNameUtf8, index, pluginHash);

        /* Return FALSE - plugin will be started via WM_DIALOG_PLUGIN_SECURITY message handler */
        return FALSE;
    }

    /* Plugin is trusted, launch directly */
    BOOL result = LaunchPreparedPlugin(index, pluginPath);
    if (result && PluginManager_IsPluginRunning(index)) {
        UpdatePluginLastModTimeIfCurrent(index, pluginPath);
        StartHotReloadIfNeeded();
    }
    return result;
}

BOOL PluginManager_StartPlugin(int index) {
    return StartPluginWithExpectedPath(index, NULL);
}

/**
 * @brief Start plugin after security dialog confirmation
 * @param index Plugin index
 * @param trustPlugin TRUE if user chose "Trust & Run", FALSE for "Run Once"
 * @return TRUE if plugin started successfully
 */
BOOL PluginManager_StartPluginAfterSecurityCheck(int index, BOOL trustPlugin) {
    if (!g_pluginManagerInitialized) return FALSE;

    char expectedPluginPathUtf8[MAX_PATH] = {0};
    const char* pendingPluginPath = GetPendingPluginPath();
    if (pendingPluginPath) {
        strncpy(expectedPluginPathUtf8, pendingPluginPath, sizeof(expectedPluginPathUtf8) - 1);
        expectedPluginPathUtf8[sizeof(expectedPluginPathUtf8) - 1] = '\0';
    }

    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LOG_ERROR("Plugin index invalid after security dialog");
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    wchar_t pluginPath[MAX_PATH];
    wchar_t pluginDisplayName[64];
    wcsncpy(pluginPath, g_plugins[index].path, MAX_PATH - 1);
    pluginPath[MAX_PATH - 1] = L'\0';
    wcsncpy(pluginDisplayName, g_plugins[index].displayName, 63);
    pluginDisplayName[63] = L'\0';

    char pluginPathUtf8[MAX_PATH];
    if (!WideToUtf8Fixed(pluginPath, pluginPathUtf8, MAX_PATH)) {
        LOG_ERROR("Failed to convert plugin path to UTF-8: %ls", pluginPath);
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    if (expectedPluginPathUtf8[0] != '\0' &&
        _stricmp(expectedPluginPathUtf8, pluginPathUtf8) != 0) {
        LOG_WARNING("Plugin index changed during security dialog; launch cancelled");
        PluginProcess_SetLastError(L"File changed");
        ClearPendingPluginInfo();
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    LeaveCriticalSection(&g_pluginCS);

    /* Security: Verify plugin file hasn't changed since dialog was shown */
    char savedHash[65] = {0};
    const char* pendingHash = GetPendingPluginHash();
    if (pendingHash) {
        strncpy(savedHash, pendingHash, sizeof(savedHash) - 1);
        savedHash[sizeof(savedHash) - 1] = '\0';
    }

    char verifiedHash[65] = {0};
    if (savedHash[0] != '\0') {
        if (CalculatePluginHash(pluginPathUtf8, verifiedHash)) {
            if (strcmp(savedHash, verifiedHash) != 0) {
                LOG_ERROR("Plugin file changed during security dialog! Aborting launch for security.");
                LOG_ERROR("  Saved hash: %s", savedHash);
                LOG_ERROR("  Current hash: %s", verifiedHash);
                PluginProcess_SetLastError(L"File changed");
                ClearPendingPluginInfo();
                return FALSE;
            }
        } else {
            LOG_ERROR("Failed to calculate current plugin hash, aborting launch for security");
            PluginProcess_SetLastError(L"Hash error");
            ClearPendingPluginInfo();
            return FALSE;
        }
    } else {
        LOG_WARNING("No saved hash available for verification (proceeding anyway)");
    }

    if (trustPlugin) {
        /* User chose "Trust & Run" - reuse the security verification hash when available. */
        BOOL trustResult = verifiedHash[0] != '\0'
            ? TrustPluginWithVerifiedHash(pluginPathUtf8, verifiedHash)
            : TrustPlugin(pluginPathUtf8);
        if (!trustResult) {
            LOG_ERROR("Failed to add plugin to trust list: %ls (will still run once)", pluginDisplayName);
        }
    }

    BOOL result = LaunchPreparedPlugin(index, pluginPath);
    if (result && PluginManager_IsPluginRunning(index)) {
        UpdatePluginLastModTimeIfCurrent(index, pluginPath);
        StartHotReloadIfNeeded();
    }
    return result;
}

/**
 * @brief Internal restart function for hot-reload
 */
static BOOL RestartPluginInternal(int index) {
    return RestartPluginInternalWithExpected(index, NULL, NULL);
}

static BOOL RestartPluginInternalWithExpected(int index,
                                             const wchar_t* expectedName,
                                             const wchar_t* expectedPath) {
    PluginInfo pluginSnapshot;
    if (!PluginManager_CopyPlugin(index, &pluginSnapshot)) {
        return FALSE;
    }
    if ((expectedName && wcscmp(pluginSnapshot.name, expectedName) != 0) ||
        (expectedPath && wcscmp(pluginSnapshot.path, expectedPath) != 0)) {
        LOG_WARNING("Plugin changed before hot-reload restart; restart cancelled");
        return FALSE;
    }

    BOOL pathMatched = FALSE;
    StopPluginIfPathMatches(index, pluginSnapshot.path, &pathMatched);
    if (!pathMatched) {
        LOG_WARNING("Plugin changed before hot-reload restart; restart cancelled");
        return FALSE;
    }

    PluginInfo currentPlugin;
    if (!PluginManager_CopyPlugin(index, &currentPlugin) ||
        wcscmp(currentPlugin.name, pluginSnapshot.name) != 0 ||
        wcscmp(currentPlugin.path, pluginSnapshot.path) != 0) {
        LOG_WARNING("Plugin changed after hot-reload stop; restart cancelled");
        return FALSE;
    }

    /* Show "Loading..." message */
    wchar_t loadingText[256];
    _snwprintf_s(loadingText, 256, _TRUNCATE, L"Loading %ls...", currentPlugin.displayName);
    PluginData_SetText(loadingText);
    PluginData_SetActive(TRUE);

    /* Force redraw */
    HWND hwnd = PluginProcess_GetNotifyWindow();
    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }

    return StartPluginIfPathMatches(index, pluginSnapshot.path);
}

static BOOL StopPluginIfPathMatches(int index, const wchar_t* expectedPath, BOOL* pathMatched) {
    if (pathMatched) {
        *pathMatched = FALSE;
    }
    if (!g_pluginManagerInitialized || !expectedPath) return FALSE;

    EnterCriticalSection(&g_pluginLifecycleCS);
    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount ||
        wcscmp(g_plugins[index].path, expectedPath) != 0) {
        LeaveCriticalSection(&g_pluginCS);
        LeaveCriticalSection(&g_pluginLifecycleCS);
        return FALSE;
    }

    if (pathMatched) {
        *pathMatched = TRUE;
    }

    PluginInfo detachedPlugin;
    memset(&detachedPlugin, 0, sizeof(detachedPlugin));
    if (!DetachPluginProcessLocked(index, &detachedPlugin)) {
        LeaveCriticalSection(&g_pluginCS);
        LeaveCriticalSection(&g_pluginLifecycleCS);
        return FALSE;
    }

    g_lastRunningPluginIndex = -1;
    g_activePluginIndex = -1;

    LeaveCriticalSection(&g_pluginCS);
    PluginProcess_TerminateDetached(&detachedPlugin);
    PluginData_Clear();
    StopHotReloadIfIdle();
    LeaveCriticalSection(&g_pluginLifecycleCS);
    return TRUE;
}

BOOL PluginManager_StopPlugin(int index) {
    if (!g_pluginManagerInitialized) return FALSE;

    EnterCriticalSection(&g_pluginLifecycleCS);
    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LeaveCriticalSection(&g_pluginCS);
        LeaveCriticalSection(&g_pluginLifecycleCS);
        return FALSE;
    }

    PluginInfo detachedPlugin;
    memset(&detachedPlugin, 0, sizeof(detachedPlugin));
    wchar_t pluginDisplayName[64];
    wcsncpy(pluginDisplayName, g_plugins[index].displayName, 63);
    pluginDisplayName[63] = L'\0';

    if (!DetachPluginProcessLocked(index, &detachedPlugin)) {
        LOG_WARNING("Plugin %ls is not running", pluginDisplayName);
        LeaveCriticalSection(&g_pluginCS);
        LeaveCriticalSection(&g_pluginLifecycleCS);
        return FALSE;
    }

    g_lastRunningPluginIndex = -1;
    g_activePluginIndex = -1;

    LeaveCriticalSection(&g_pluginCS);
    PluginProcess_TerminateDetached(&detachedPlugin);
    PluginData_Clear();
    StopHotReloadIfIdle();
    LeaveCriticalSection(&g_pluginLifecycleCS);
    return TRUE;
}

BOOL PluginManager_TogglePlugin(int index) {
    if (!g_pluginManagerInitialized) return FALSE;

    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    BOOL isRunning = g_plugins[index].isRunning;
    wchar_t pluginPath[MAX_PATH];
    wcsncpy(pluginPath, g_plugins[index].path, MAX_PATH - 1);
    pluginPath[MAX_PATH - 1] = L'\0';
    LeaveCriticalSection(&g_pluginCS);

    if (isRunning) {
        BOOL pathMatched = FALSE;
        return StopPluginIfPathMatches(index, pluginPath, &pathMatched);
    } else {
        return StartPluginIfPathMatches(index, pluginPath);
    }
}

static BOOL StartPluginIfPathMatches(int index, const wchar_t* expectedPath) {
    return StartPluginWithExpectedPath(index, expectedPath);
}

BOOL PluginManager_IsPluginRunning(int index) {
    if (!g_pluginManagerInitialized) return FALSE;

    BOOL isRunning;
    BOOL shouldClearDisplay = FALSE;
    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    PluginInfo* plugin = &g_plugins[index];

    /* Check via process module */
    if (plugin->isRunning && !PluginProcess_IsAlive(plugin)) {
        if (g_activePluginIndex == index) {
            g_activePluginIndex = -1;
            shouldClearDisplay = TRUE;
        }
        if (g_lastRunningPluginIndex == index) {
            g_lastRunningPluginIndex = -1;
        }
    }

    isRunning = plugin->isRunning;
    LeaveCriticalSection(&g_pluginCS);

    if (shouldClearDisplay) {
        PluginData_Clear();
        StopHotReloadIfIdle();
    }

    return isRunning;
}

BOOL PluginManager_NeedsSecurityCheck(int index) {
    if (!g_pluginManagerInitialized) return FALSE;

    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    /* Get plugin path */
    wchar_t pluginPath[MAX_PATH];
    wcsncpy(pluginPath, g_plugins[index].path, MAX_PATH - 1);
    pluginPath[MAX_PATH - 1] = L'\0';

    LeaveCriticalSection(&g_pluginCS);

    /* Convert to UTF-8 for security check */
    char pluginPathUtf8[MAX_PATH];
    if (!WideToUtf8Fixed(pluginPath, pluginPathUtf8, MAX_PATH)) {
        LOG_ERROR("Failed to convert plugin path to UTF-8: %ls", pluginPath);
        return TRUE;
    }

    /* Return TRUE if NOT trusted (needs security check) */
    return !IsPluginTrusted(pluginPathUtf8);
}

void PluginManager_StopAllPlugins(void) {
    if (!g_pluginManagerInitialized) return;

    PluginInfo* detachedPlugins = AllocatePluginSnapshotArray();

    EnterCriticalSection(&g_pluginLifecycleCS);

    if (detachedPlugins) {
        EnterCriticalSection(&g_pluginCS);
        int detachedCount = DetachAllRunningPluginProcessesLocked(detachedPlugins, MAX_PLUGINS);
        // Always clear indices (even if no plugin was running)
        // This handles cases where plugin already exited (e.g., after <exit> tag)
        g_activePluginIndex = -1;
        g_lastRunningPluginIndex = -1;
        LeaveCriticalSection(&g_pluginCS);

        for (int i = 0; i < detachedCount; i++) {
            PluginProcess_TerminateDetached(&detachedPlugins[i]);
        }
        free(detachedPlugins);
    } else {
        LOG_WARNING("Plugin stop-all using one-by-one cleanup after snapshot allocation failed");
        DetachAndTerminateRunningPluginsIndividually(-1);

        EnterCriticalSection(&g_pluginCS);
        g_activePluginIndex = -1;
        g_lastRunningPluginIndex = -1;
        LeaveCriticalSection(&g_pluginCS);
    }

    PluginData_Clear();
    StopHotReloadThread(FALSE);
    LeaveCriticalSection(&g_pluginLifecycleCS);
}

BOOL PluginManager_OpenPluginFolder(void) {
    wchar_t pluginDir[MAX_PATH];
    if (!PluginManager_GetPluginDirW(pluginDir, MAX_PATH)) {
        return FALSE;
    }

    // Ensure directory exists
    CreateDirectoryW(pluginDir, NULL);

    // Open in explorer
    ShellExecuteW(NULL, L"open", pluginDir, NULL, NULL, SW_SHOW);
    return TRUE;
}

void PluginManager_SetNotifyWindow(HWND hwnd) {
    PluginProcess_SetNotifyWindow(hwnd);
}

int PluginManager_GetActivePluginIndex(void) {
    if (!g_pluginManagerInitialized) return -1;

    EnterCriticalSection(&g_pluginCS);

    int activeIndex = g_activePluginIndex;
    if (activeIndex < 0 || activeIndex >= g_pluginCount) {
        activeIndex = -1;
    }

    LeaveCriticalSection(&g_pluginCS);
    return activeIndex;
}

BOOL PluginManager_RestartPlugin(int index) {
    return RestartPluginInternal(index);
}

BOOL PluginManager_RestartPendingHotReload(LONG requestGeneration) {
    if (requestGeneration == 0 || !g_pluginManagerInitialized) {
        return FALSE;
    }

    PluginHotReloadRequest request = {0};
    BOOL hasRequest = FALSE;

    EnterCriticalSection(&g_pluginCS);
    if (g_hotReloadRequestPending &&
        InterlockedCompareExchange(&g_hotReloadRequestGeneration, 0, 0) == requestGeneration) {
        request = g_hotReloadRequest;
        g_hotReloadRequestPending = FALSE;
        hasRequest = TRUE;
    }
    LeaveCriticalSection(&g_pluginCS);

    if (!hasRequest) {
        LOG_WARNING("[HotReload] Ignoring stale hot-reload request generation %ld",
                    requestGeneration);
        return FALSE;
    }

    LOG_INFO("[HotReload] Restarting plugin %d from verified snapshot", request.index);
    return RestartPluginInternalWithExpected(request.index, request.name, request.path);
}

void PluginManager_HandleProcessExit(DWORD processId) {
    if (!g_pluginManagerInitialized || processId == 0) return;

    BOOL shouldClearDisplay = FALSE;
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

    LeaveCriticalSection(&g_pluginCS);

    if (hProcessToClose) {
        CloseHandle(hProcessToClose);
    }
    if (hThreadToClose) {
        CloseHandle(hThreadToClose);
    }

    if (shouldClearDisplay) {
        PluginData_Clear();
        HWND hwnd = PluginProcess_GetNotifyWindow();
        if (hwnd) {
            InvalidateRect(hwnd, NULL, TRUE);
        }
        StopHotReloadIfIdle();
    }
}
