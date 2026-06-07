/**
 * @file config_watcher.h
 * @brief Live configuration reload via file system monitoring
 * 
 * Uses directory change notifications (event-driven) instead of polling to minimize CPU usage.
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
 * Background thread required because directory notification waits block until changes occur.
 * Watches the config directory and validates config.ini by timestamp/size snapshot
 * to prevent spurious reloads from unrelated files.
 * 200ms debounce batches rapid writes into single reload event.
 * 
 * @note Idempotent (multiple calls ignored to prevent duplicate watchers)
 */
void ConfigWatcher_Start(HWND hwnd);

/**
 * @brief Stops monitoring and releases resources
 * 
 * @details
 * Signals the watcher thread and waits briefly for cleanup. The watcher uses
 * waitable directory notifications, so stop does not need to cancel pending I/O.
 * 
 * @note Idempotent (safe to call when no watcher running)
 * @note Can restart after stopping if needed
 */
void ConfigWatcher_Stop(void);

/**
 * @brief Stop monitoring during final process teardown.
 * @return TRUE if no watcher thread remains, FALSE if the wait failed.
 *
 * @details
 * Unlike ConfigWatcher_Stop(), this waits without the short UI-facing timeout
 * so shared config resources can be released only after the watcher is gone.
 */
BOOL ConfigWatcher_Shutdown(void);

/**
 * @brief Mark the queued config reload message as being handled.
 */
void ConfigWatcher_BeginConfigReloadHandling(void);

/**
 * @brief Mark config reload handling complete and repost if a change arrived during handling.
 * @param hwnd Window handle that owns config reload messages
 */
void ConfigWatcher_EndConfigReloadHandling(HWND hwnd);

#endif /* CONFIG_WATCHER_H */
