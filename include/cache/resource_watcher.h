/**
 * @file resource_watcher.h
 * @brief File system watcher for resource folders
 */

#ifndef RESOURCE_WATCHER_H
#define RESOURCE_WATCHER_H

#include <windows.h>

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Start watching resource folders for changes
 * @return TRUE on success, FALSE on failure
 */
BOOL ResourceWatcher_Start(void);

/**
 * @brief Stop watching resource folders
 */
void ResourceWatcher_Stop(void);

/**
 * @brief Check if watcher is running
 */
BOOL ResourceWatcher_IsRunning(void);

#endif /* RESOURCE_WATCHER_H */
