/**
 * @file config_watcher.h
 * @brief Configuration file change monitoring system
 * 
 * Provides real-time monitoring of config.ini file changes using Windows
 * ReadDirectoryChangesW API. When changes are detected, notifies the main
 * window to reload affected configuration sections.
 * 
 * Features:
 * - Non-blocking directory monitoring using overlapped I/O
 * - Automatic debouncing to handle rapid file changes
 * - Targeted file filtering (only monitors config.ini)
 * - Comprehensive change notifications for all config sections
 * 
 * @version 2.0 - Enhanced documentation and modular design
 */

#ifndef CONFIG_WATCHER_H
#define CONFIG_WATCHER_H

#include <windows.h>

/**
 * @brief Start configuration file watcher thread
 * 
 * Initializes and starts a background thread that monitors the configuration
 * directory for changes to config.ini. When changes are detected, the thread
 * posts messages to the specified window handle to trigger configuration reloads.
 * 
 * The watcher monitors the following change types:
 * - File name changes (rename, create, delete)
 * - Last write time changes (file modifications)
 * - File size changes
 * 
 * @param hwnd Window handle to receive WM_APP_*_CHANGED notifications
 * 
 * @note If a watcher is already running, this function does nothing.
 * @note The watcher automatically debounces changes with a 200ms delay to
 *       avoid processing rapid successive file writes.
 * 
 * @see ConfigWatcher_Stop
 */
void ConfigWatcher_Start(HWND hwnd);

/**
 * @brief Stop configuration file watcher and cleanup resources
 * 
 * Signals the watcher thread to stop, waits for it to exit cleanly, and
 * releases all associated resources (thread handle, stop event, etc.).
 * 
 * This function is blocking and will wait indefinitely for the watcher thread
 * to terminate gracefully. The watcher thread is designed to respond quickly
 * to stop signals, typically exiting within milliseconds.
 * 
 * @note It is safe to call this function even if no watcher is running.
 * @note After calling this function, you can start a new watcher with
 *       ConfigWatcher_Start if needed.
 * 
 * @see ConfigWatcher_Start
 */
void ConfigWatcher_Stop(void);

#endif /* CONFIG_WATCHER_H */
