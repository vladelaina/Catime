/**
 * @file config_watcher.h
 * @brief Background watcher to monitor config.ini changes and notify UI thread
 */

#ifndef CONFIG_WATCHER_H
#define CONFIG_WATCHER_H

#include <windows.h>

/**
 * Start watching the directory containing config.ini. Notifications are posted
 * to the provided window handle using custom WM_APP messages.
 */
void ConfigWatcher_Start(HWND hwnd);

/**
 * Stop the watcher thread and release resources.
 */
void ConfigWatcher_Stop(void);

#endif


