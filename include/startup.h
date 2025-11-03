/**
 * @file startup.h
 * @brief Windows startup integration and launch mode config
 * 
 * Shortcut management uses COM IShellLink for reliability.
 * --startup argument in shortcut enables different launch behavior (e.g., hidden mode).
 * Data-driven mode switching simplifies adding new startup behaviors.
 */

#ifndef STARTUP_H
#define STARTUP_H

#include <windows.h>
#include <shlobj.h>

/**
 * @brief Check if auto-start enabled
 * @return TRUE if shortcut exists
 * 
 * @note Existence only, doesn't verify validity
 */
BOOL IsAutoStartEnabled(void);

/**
 * @brief Create startup shortcut with --startup argument
 * @return TRUE on success
 * 
 * @details
 * Points to current executable with --startup arg.
 * Handles COM initialization internally.
 */
BOOL CreateShortcut(void);

/**
 * @brief Remove startup shortcut (idempotent)
 * @return TRUE on success or if doesn't exist
 * 
 * @note Returns TRUE for ERROR_FILE_NOT_FOUND
 */
BOOL RemoveShortcut(void);

/**
 * @brief Update shortcut to current executable (handles moved apps)
 * @return TRUE on success or if no shortcut
 * 
 * @details Implemented as Remove + Create for simplicity
 */
BOOL UpdateStartupShortcut(void);

/**
 * @brief Apply configured startup mode from config
 * @param hwnd Window handle
 * 
 * @details Modes:
 * - COUNT_UP: Timer counts up from zero
 * - SHOW_TIME: Display current system time
 * - NO_DISPLAY: Hidden mode
 * - DEFAULT: Countdown with default time
 * 
 * Falls back to DEFAULT if config missing/invalid.
 * 
 * @note Call during init after window creation
 */
void ApplyStartupMode(HWND hwnd);

#endif /* STARTUP_H */
