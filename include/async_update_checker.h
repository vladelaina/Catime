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
 * @param silentCheck TRUE for background check (no "up to date" dialog), 
 *                    FALSE for interactive check (always show result)
 * 
 * @details Creates a detached background thread that:
 * - Fetches latest version info from update server
 * - Compares with current version
 * - Shows appropriate UI dialogs based on result and silentCheck flag
 * 
 * @note Thread cleans up automatically on completion
 * @note Safe to call multiple times (prevents concurrent checks)
 * @see CleanupUpdateThread
 */
void CheckForUpdateAsync(HWND hwnd, BOOL silentCheck);

/**
 * @brief Clean up update checker thread resources with timeout
 * 
 * Waits for background update check thread to complete (max 1 second)
 * and releases all associated resources. Safe to call even if no
 * thread is running.
 * 
 * @note Should be called during application shutdown
 * @note Blocking call with 1 second timeout
 * @see CheckForUpdateAsync
 */
void CleanupUpdateThread(void);

#endif