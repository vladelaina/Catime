/**
 * @file config_watcher.h
 * @brief Live configuration reload via file system monitoring
 * 
 * Uses ReadDirectoryChangesW (event-driven) instead of polling to minimize CPU usage.
 * 200ms debouncing prevents multiple reloads when editors save files in rapid bursts
 * (temp file, rename, metadata update).
 */

#ifndef CONFIG_WATCHER_H
#define CONFIG_WATCHER_H

#include <windows.h>

/**
 * @brief Starts background file monitoring thread
 * @param hwnd Window handle for change notifications
 * 
 * @details
 * Background thread required because ReadDirectoryChangesW blocks until changes occur.
 * Monitors config.ini only (ignores other files in directory to prevent spurious reloads).
 * 200ms debounce batches rapid writes into single reload event.
 * 
 * @note Idempotent (multiple calls ignored to prevent duplicate watchers)
 * @note Uses overlapped I/O to prevent blocking watcher thread
 */
void ConfigWatcher_Start(HWND hwnd);

/**
 * @brief Stops monitoring and releases resources
 * 
 * @details
 * Blocks indefinitely for thread cleanup (safe because watcher thread responds
 * quickly to stop signals). Acceptable during shutdown when user doesn't notice delay.
 * 
 * @note Idempotent (safe to call when no watcher running)
 * @note Can restart after stopping if needed
 */
void ConfigWatcher_Stop(void);

#endif /* CONFIG_WATCHER_H */
