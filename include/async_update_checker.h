/**
 * @file async_update_checker.h
 * @brief Asynchronous update checking to avoid blocking UI
 * @version 2.0 - Refactored for better maintainability
 * 
 * Provides non-blocking application update checking using background threads.
 * Prevents UI freezing during network operations and version checks.
 */

#ifndef ASYNC_UPDATE_CHECKER_H
#define ASYNC_UPDATE_CHECKER_H

#include <windows.h>

/**
 * @brief Check for updates asynchronously in background thread
 * @param hwnd Main window handle for UI callbacks
 * @param silentCheck TRUE for background check (no dialogs),
 *                    FALSE for interactive check (show result dialogs)
 * 
 * @details Creates a detached background thread that:
 * - Fetches latest version info from update server
 * - Compares with current version
 * - Posts the result back to the main window for UI handling
 * 
 * @note Thread cleans up automatically on completion
 * @note Safe to call multiple times (prevents concurrent checks)
 * @return TRUE if a new update check thread was started, FALSE otherwise
 * @see CleanupUpdateThread
 */
BOOL CheckForUpdateAsync(HWND hwnd, BOOL silentCheck);

/**
 * @brief Request update checker cancellation and reap completed thread resources
 * 
 * Waits briefly for the background update check thread to complete and releases
 * resources if it has exited. Safe to call even if no thread is running.
 * 
 * @note Should be called during application shutdown
 * @note Blocking call with short timeout
 * @see CheckForUpdateAsync
 */
void CleanupUpdateThread(void);

/**
 * @brief Request update checker cancellation and wait briefly for thread exit
 *
 * Use during final process teardown. If WinINet does not return promptly, the
 * thread handle is abandoned so shutdown cannot hang indefinitely.
 */
void CleanupUpdateThreadBlocking(void);

#endif
