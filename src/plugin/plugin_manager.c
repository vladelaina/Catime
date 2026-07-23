/**
 * @file plugin_manager.c
 * @brief Plugin manager - core plugin lifecycle and state management
 */

#include "plugin/plugin_manager.h"
#include "plugin/plugin_process.h"
#include "plugin/plugin_extensions.h"
#include "plugin/plugin_data.h"
#include "config.h"
#include "config/config_plugin_security.h"
#include "dialog/dialog_plugin_security.h"
#include "utils/natural_sort.h"
#include "utils/directory_watcher.h"
#include "log.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shellapi.h>
#include <limits.h>

#define MAX_PLUGIN_SCAN_ENTRIES 4096
#define MAX_PLUGIN_RECURSION_DEPTH 10
#define ASYNC_PLUGIN_SCAN_STOP_TIMEOUT_MS 2000
#define ASYNC_PLUGIN_SCAN_FAILURE_COOLDOWN_MS 2000
#define HOT_RELOAD_STOP_TIMEOUT_MS 2000
#define HOT_RELOAD_START_FAILURE_COOLDOWN_MS 2000
#define PLUGIN_MANAGER_SHUTDOWN_LOCK_WAIT_MS 2000
#define PLUGIN_SCAN_FAILED (-1)

typedef struct {
    BOOL exists;
    FILETIME lastWriteTime;
    DWORD entryCount;
    DWORD scannedEntries;
    BOOL truncated;
    ULONGLONG contentHash;
} PluginDirSnapshot;

typedef struct {
    PluginInfo* plugins;
    int count;
    int scannedEntries;
    BOOL full;
    BOOL failed;
} PluginScanContext;

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
static DWORD g_hotReloadStartFailureCooldownUntil = 0;

/* Async plugin scan state */
static HANDLE g_hAsyncScanThread = NULL;
static HANDLE g_hRetiredAsyncScanThread = NULL;
static DirectoryWatcher g_pluginFolderWatcher = {0};
static volatile LONG g_asyncScanPending = 0;
static volatile LONG g_asyncScanShuttingDown = 0;
static volatile LONG g_asyncScanGeneration = 0;
static SRWLOCK g_asyncScanLock = SRWLOCK_INIT;
static BOOL g_asyncScanHasLastSnapshot = FALSE;
static PluginDirSnapshot g_asyncScanLastSnapshot;
static BOOL g_asyncScanHasFailureSnapshot = FALSE;
static BOOL g_asyncScanFailureHadSnapshot = FALSE;
static PluginDirSnapshot g_asyncScanFailureSnapshot;
static volatile LONG g_asyncScanFailureCooldownUntil = 0;
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
static BOOL StopHotReloadThread(void);
static BOOL StopHotReloadThreadLocked(void);
static void StopHotReloadIfIdle(void);
static BOOL StopAsyncScanThread(void);
static BOOL CleanupRetiredAsyncScanThread(DWORD waitMs);
static BOOL HasRetiredAsyncScanThread(void);
static BOOL GetPluginDirSnapshot(PluginDirSnapshot* snapshot);
static BOOL PluginDirSnapshotsEqual(const PluginDirSnapshot* a,
                                    const PluginDirSnapshot* b);
static void ExtractDisplayName(const wchar_t* filename,
                               wchar_t* displayName,
                               size_t bufferSize);
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
    return g_hotReloadStartFailureCooldownUntil != 0 &&
           (LONG)(g_hotReloadStartFailureCooldownUntil - now) > 0;
}

static void MarkHotReloadStartFailure(DWORD now) {
    DWORD cooldownUntil = now + HOT_RELOAD_START_FAILURE_COOLDOWN_MS;
    g_hotReloadStartFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
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

#include "plugin_manager_part01.inc"
#include "plugin_manager_part02.inc"
#include "plugin_manager_part03.inc"
#include "plugin_manager_part04.inc"
#include "plugin_manager_part05.inc"
#include "plugin_manager_part06.inc"
#include "plugin_manager_part07.inc"
#include "plugin_manager_part08.inc"
#include "plugin_manager_part09.inc"
