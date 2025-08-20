/**
 * @file startup.h
 * @brief Windows startup shortcut management
 * 
 * Functions for managing application auto-start with Windows system boot
 */

#ifndef STARTUP_H
#define STARTUP_H

#include <windows.h>
#include <shlobj.h>

/**
 * @brief Check if application auto-start is enabled
 * @return TRUE if auto-start is enabled, FALSE otherwise
 */
BOOL IsAutoStartEnabled(void);

/**
 * @brief Create startup shortcut for auto-start
 * @return TRUE on success, FALSE on failure
 */
BOOL CreateShortcut(void);

/**
 * @brief Remove startup shortcut to disable auto-start
 * @return TRUE on success, FALSE on failure
 */
BOOL RemoveShortcut(void);

/**
 * @brief Update existing startup shortcut
 * @return TRUE on success, FALSE on failure
 */
BOOL UpdateStartupShortcut(void);

#endif