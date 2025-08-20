/**
 * @file async_update_checker.h
 * @brief Asynchronous update checking
 * 
 * Functions for non-blocking application update checking using background threads
 */

#ifndef ASYNC_UPDATE_CHECKER_H
#define ASYNC_UPDATE_CHECKER_H

#include <windows.h>

/**
 * @brief Check for updates asynchronously in background thread
 * @param hwnd Window handle for update notifications
 * @param silentCheck TRUE for silent check, FALSE for interactive
 */
void CheckForUpdateAsync(HWND hwnd, BOOL silentCheck);

/**
 * @brief Clean up update checker thread resources
 */
void CleanupUpdateThread(void);

#endif