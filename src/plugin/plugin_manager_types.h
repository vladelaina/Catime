/**
 * @file plugin_manager_types.h
 * @brief Private data types and limits for plugin management.
 */

#ifndef PLUGIN_MANAGER_TYPES_H
#define PLUGIN_MANAGER_TYPES_H

#include "plugin/plugin_manager.h"

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

#endif /* PLUGIN_MANAGER_TYPES_H */
