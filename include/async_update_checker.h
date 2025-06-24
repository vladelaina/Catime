/**
 * @file async_update_checker.h
 * @brief Minimalist asynchronous application update check functionality interface
 * 
 * This file defines the application's asynchronous update checking functionality interface, ensuring that update checks don't block the main thread.
 */

#ifndef ASYNC_UPDATE_CHECKER_H
#define ASYNC_UPDATE_CHECKER_H

#include <windows.h>

/**
 * @brief Check for application updates asynchronously
 * @param hwnd Window handle
 * @param silentCheck Whether to perform a silent check (only show prompt when updates are available)
 * 
 * Connects to GitHub in a separate thread to check for new versions.
 * This function returns immediately and doesn't block the main thread.
 */
void CheckForUpdateAsync(HWND hwnd, BOOL silentCheck);

/**
 * @brief Clean up update check thread resources
 * 
 * Call this function before program exit to ensure all update check thread related resources are released,
 * preventing memory leaks and resource leaks.
 */
void CleanupUpdateThread(void);

#endif // ASYNC_UPDATE_CHECKER_H