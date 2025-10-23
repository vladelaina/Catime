/**
 * @file startup.h
 * @brief Windows startup integration and application launch mode management
 * @version 3.0 - Enhanced with data-driven design and comprehensive documentation
 * 
 * Provides intelligent Windows startup shortcut management and startup behavior
 * configuration for timer applications. Features include:
 * 
 * - Automatic startup shortcut creation/removal
 * - Startup mode configuration (count-up, show time, no display, countdown)
 * - Data-driven mode switching for easy extensibility
 * - Safe path operations and COM resource management
 * 
 * Startup Modes:
 * - COUNT_UP: Timer counts up from zero
 * - SHOW_TIME: Display current system time
 * - NO_DISPLAY: Hidden mode with no timer display
 * - DEFAULT: Standard countdown timer with configured default time
 */

#ifndef STARTUP_H
#define STARTUP_H

#include <windows.h>
#include <shlobj.h>

/**
 * @brief Check if application auto-start is enabled
 * @return TRUE if startup shortcut exists in user's Startup folder, FALSE otherwise
 * 
 * Checks for the presence of a shortcut file in the Windows Startup folder.
 * This function does not verify the shortcut's validity or target path.
 * 
 * @note Uses GetFileAttributesW for existence check
 * @see CreateShortcut() for creating the startup shortcut
 */
BOOL IsAutoStartEnabled(void);

/**
 * @brief Create startup shortcut for auto-start with Windows
 * @return TRUE on success, FALSE on failure
 * 
 * Creates a shortcut in the Windows Startup folder that:
 * - Points to the current executable
 * - Includes --startup command line argument
 * - Uses COM IShellLink interface for creation
 * 
 * The shortcut will cause the application to launch automatically when
 * Windows starts.
 * 
 * @note Requires COM to be initialized (handled internally)
 * @note Will fail if Startup folder is inaccessible or COM creation fails
 * @see RemoveShortcut() for removing the startup shortcut
 */
BOOL CreateShortcut(void);

/**
 * @brief Remove startup shortcut to disable auto-start
 * @return TRUE on success or if shortcut doesn't exist, FALSE on failure
 * 
 * Deletes the startup shortcut from the Windows Startup folder.
 * Returns TRUE even if the shortcut doesn't exist (idempotent operation).
 * 
 * @note Returns TRUE if ERROR_FILE_NOT_FOUND
 * @note May fail due to file permissions or locked file
 * @see CreateShortcut() for creating the startup shortcut
 */
BOOL RemoveShortcut(void);

/**
 * @brief Update existing startup shortcut to current executable location
 * @return TRUE on success or if no shortcut exists, FALSE on failure
 * 
 * If a startup shortcut exists, removes it and recreates it to ensure
 * it points to the current executable location. This is useful when the
 * application has been moved or updated.
 * 
 * If no shortcut exists, returns TRUE without doing anything.
 * 
 * @note Implemented as Remove + Create for simplicity
 * @see IsAutoStartEnabled() for checking existence
 */
BOOL UpdateStartupShortcut(void);

/**
 * @brief Apply configured startup mode behavior to timer application
 * @param hwnd Main window handle for timer operations
 * 
 * Reads STARTUP_MODE from configuration file and applies corresponding
 * behavior using data-driven configuration:
 * 
 * Available Modes:
 * ┌──────────────┬────────────────────────────────────────────┐
 * │ Mode         │ Behavior                                   │
 * ├──────────────┼────────────────────────────────────────────┤
 * │ COUNT_UP     │ Timer counts up from zero                  │
 * │ SHOW_TIME    │ Display current system time                │
 * │ NO_DISPLAY   │ Hidden mode, timer disabled                │
 * │ DEFAULT      │ Countdown with configured default time     │
 * └──────────────┴────────────────────────────────────────────┘
 * 
 * Falls back to DEFAULT mode if configuration is missing or invalid.
 * Automatically triggers window repaint to reflect changes.
 * 
 * @note Accesses global timer state variables (CLOCK_SHOW_CURRENT_TIME, etc.)
 * @note Should be called during application initialization after window creation
 */
void ApplyStartupMode(HWND hwnd);

#endif /* STARTUP_H */
