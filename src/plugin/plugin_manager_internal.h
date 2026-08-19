/**
 * @file plugin_manager_internal.h
 * @brief Internal interface shared by split plugin-manager modules.
 */

#ifndef PLUGIN_MANAGER_INTERNAL_H
#define PLUGIN_MANAGER_INTERNAL_H

#include "plugin/plugin_manager_types.h"
#include "plugin/plugin_process.h"
#include "plugin/plugin_data.h"
#include "config.h"
#include "config/config_plugin_security.h"
#include "dialog/dialog_plugin_security.h"
#include "utils/natural_sort.h"
#include "utils/directory_watcher.h"
#include "../resource/resource.h"
#include "log.h"

#include <limits.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern PluginInfo g_plugins[MAX_PLUGINS];
extern int g_pluginCount;
extern CRITICAL_SECTION g_pluginCS;
extern CRITICAL_SECTION g_pluginLifecycleCS;
extern BOOL g_pluginManagerInitialized;

extern HANDLE g_hHotReloadThread;
extern HANDLE g_hHotReloadStopEvent;
extern volatile LONG g_hotReloadRunning;
extern volatile int g_lastRunningPluginIndex;
extern volatile int g_activePluginIndex;
extern SRWLOCK g_hotReloadLock;
extern volatile LONG g_hotReloadRequestGeneration;
extern BOOL g_hotReloadRequestPending;
extern PluginHotReloadRequest g_hotReloadRequest;
extern DWORD g_hotReloadStartFailureCooldownUntil;

extern HANDLE g_hAsyncScanThread;
extern HANDLE g_hRetiredAsyncScanThread;
extern DirectoryWatcher g_pluginFolderWatcher;
extern volatile LONG g_asyncScanPending;
extern volatile LONG g_asyncScanShuttingDown;
extern volatile LONG g_asyncScanGeneration;
extern SRWLOCK g_asyncScanLock;
extern BOOL g_asyncScanHasLastSnapshot;
extern PluginDirSnapshot g_asyncScanLastSnapshot;
extern BOOL g_asyncScanHasFailureSnapshot;
extern BOOL g_asyncScanFailureHadSnapshot;
extern PluginDirSnapshot g_asyncScanFailureSnapshot;
extern volatile LONG g_asyncScanFailureCooldownUntil;
extern BOOL g_pluginLocksInitialized;
extern BOOL g_pluginProcessInitialized;

BOOL IsAsyncScanShuttingDown(void);
BOOL IsAsyncScanGenerationCurrent(LONG generation);
BOOL IsHotReloadRunning(void);
void SetHotReloadRunning(BOOL running);
BOOL IsHotReloadStartFailureCoolingDown(DWORD now);
void MarkHotReloadStartFailure(DWORD now);
BOOL EnterCriticalSectionWithTimeout(CRITICAL_SECTION* cs, DWORD timeoutMs);
LONG QueueHotReloadRequestLocked(int index, const wchar_t* name,
                                 const wchar_t* path);

BOOL WideToUtf8Fixed(const wchar_t* src, char* dest, int destCount);
BOOL PluginManager_GetPluginDirW(wchar_t* buffer, size_t bufferSize);
void OnPluginFolderChanged(void* context);
void StartPluginFolderWatcher(void);
void StopPluginFolderWatcher(void);
wchar_t ToLowerAsciiW(wchar_t ch);
BOOL MatchesPluginPatternExtension(const wchar_t* ext, const char* pattern);
BOOL IsSupportedPluginFileW(const wchar_t* fileName);
void UpdateLatestWriteTime(FILETIME* target, const FILETIME* candidate);
void MixPluginSnapshotHash(PluginDirSnapshot* snapshot, ULONGLONG value);
void MixPluginSnapshotPath(PluginDirSnapshot* snapshot, const wchar_t* path);
BOOL BuildRelativePathW(wchar_t* outPath, size_t outSize,
                        const wchar_t* parentRelativePath,
                        const wchar_t* fileName);
BOOL AddPluginEntry(PluginScanContext* ctx, const wchar_t* pluginDir,
                    const wchar_t* fileName, const wchar_t* relativePath);
void ScanPluginFolderRecursive(const wchar_t* pluginDir,
                               const wchar_t* folderPath,
                               const wchar_t* relativePath,
                               PluginScanContext* ctx,
                               int depth, LONG generation);
int ComparePluginInfo(const void* a, const void* b);
BOOL GetFileModTime(const wchar_t* path, FILETIME* modTime);
DWORD WINAPI HotReloadThread(LPVOID lpParam);
BOOL AnyPluginRunningLocked(void);
void StartHotReloadIfNeeded(void);
void CleanupCompletedHotReloadThreadLocked(void);
BOOL StopHotReloadThreadLocked(void);
BOOL StopHotReloadThread(void);
void StopHotReloadIfIdle(void);
void ExtractDisplayName(const wchar_t* filename,
                        wchar_t* displayName, size_t bufferSize);

BOOL ScanPluginDirSnapshotRecursive(const wchar_t* folderPath,
                                    const wchar_t* relativePath,
                                    PluginDirSnapshot* snapshot,
                                    int depth);
BOOL GetPluginDirSnapshot(PluginDirSnapshot* snapshot);
BOOL PluginDirSnapshotsEqual(const PluginDirSnapshot* a,
                             const PluginDirSnapshot* b);
BOOL IsAsyncScanFailureRecentlyCachedLocked(
    BOOL hasSnapshot, const PluginDirSnapshot* snapshot, DWORD now);
void MarkAsyncScanFailureLocked(BOOL hasSnapshot,
                                const PluginDirSnapshot* snapshot);
void ClearAsyncScanFailureLocked(void);
int PluginManager_ScanPluginsForGeneration(LONG generation);
DWORD WINAPI AsyncScanThread(LPVOID lpParam);
BOOL CleanupRetiredAsyncScanThread(DWORD waitMs);
BOOL HasRetiredAsyncScanThread(void);
BOOL StopAsyncScanThread(void);

BOOL DetachPluginProcessLocked(int index, PluginInfo* detachedPlugin);
int DetachAllRunningPluginProcessesLocked(PluginInfo* detachedPlugins,
                                          int capacity);
PluginInfo* AllocatePluginSnapshotArray(void);
int DetachAndTerminateRunningPluginsIndividually(int skipIndex);
BOOL PreparePluginLaunchLocked(int index, const wchar_t* expectedPath,
                               PluginInfo* launchPlugin,
                               PluginInfo* detachedPlugins,
                               int* detachedCount,
                               BOOL* alreadyRunning);
BOOL LaunchPreparedPlugin(int index, const wchar_t* expectedPath);
void UpdatePluginLastModTimeIfCurrent(int index, const wchar_t* pluginPath);
BOOL StartPluginWithExpectedPath(int index, const wchar_t* expectedPath);
BOOL StartTrustedPluginWithExpectedPath(int index, const wchar_t* expectedPath);
BOOL StartPluginAfterSecurityCheckWithSnapshot(int index, BOOL trustPlugin,
                                               const char* expectedPathUtf8,
                                               const char* savedHash);
void StopAllPluginsPreserveData(void);
void PluginManager_InitAsync(void);
int PluginManager_PostAsyncSecurityRequest(int index, const char* path,
                                           const char* displayName,
                                           const char* hash);
BOOL RestartPluginInternal(int index);
BOOL RestartPluginInternalWithExpected(int index,
                                       const wchar_t* expectedName,
                                       const wchar_t* expectedPath);
BOOL StopPluginIfPathMatches(int index, const wchar_t* expectedPath,
                             BOOL* pathMatched);
BOOL StartPluginIfPathMatches(int index, const wchar_t* expectedPath);

#endif /* PLUGIN_MANAGER_INTERNAL_H */
