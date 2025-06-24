/**
 * @file startup.h
 * @brief Auto-start functionality interface
 * 
 * This file defines the application's auto-start related function interfaces,
 * including checking if auto-start is enabled, creating and deleting auto-start shortcuts.
 */

#ifndef STARTUP_H
#define STARTUP_H

#include <windows.h>
#include <shlobj.h>

/**
 * @brief Check if the application is set to auto-start on system boot
 * @return BOOL Returns TRUE if auto-start is enabled, otherwise FALSE
 * 
 * Determines if auto-start is enabled by checking if the application's shortcut exists in the startup folder.
 */
BOOL IsAutoStartEnabled(void);

/**
 * @brief Create auto-start shortcut
 * @return BOOL Returns TRUE if creation is successful, otherwise FALSE
 * 
 * Creates a shortcut for the application in the system startup folder, allowing it to run automatically when the system starts.
 */
BOOL CreateShortcut(void);

/**
 * @brief Delete auto-start shortcut
 * @return BOOL Returns TRUE if deletion is successful, otherwise FALSE
 * 
 * Removes the application's shortcut from the system startup folder, disabling its auto-start functionality.
 */
BOOL RemoveShortcut(void);

/**
 * @brief Update auto-start shortcut
 * 
 * Checks if auto-start is enabled, and if so, deletes the old shortcut and creates a new one,
 * ensuring that the auto-start functionality works properly even if the application location changes.
 * 
 * @return BOOL Returns TRUE if the update is successful, otherwise FALSE
 */
BOOL UpdateStartupShortcut(void);

#endif // STARTUP_H
