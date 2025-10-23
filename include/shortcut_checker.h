/**
 * @file shortcut_checker.h
 * @brief Desktop shortcut management for package manager installations
 * @version 2.0 - Enhanced with modular design and comprehensive documentation
 * 
 * Provides intelligent desktop shortcut management for applications installed
 * through package managers (Microsoft Store, WinGet). Features include:
 * 
 * - Automatic shortcut creation for package manager installations
 * - Smart detection of installation source (Store/WinGet/Manual)
 * - Shortcut validation and auto-update for relocated executables
 * - User preference tracking (respects manual shortcut deletion)
 * - Unified COM object lifecycle management
 * 
 * The system uses data-driven package detection rules and maintains state
 * in configuration files to avoid redundant operations on subsequent runs.
 */

#ifndef SHORTCUT_CHECKER_H
#define SHORTCUT_CHECKER_H

#include <windows.h>

/**
 * @brief Check for existing shortcut and create/update if needed
 * 
 * Main entry point for desktop shortcut management. Implements smart logic:
 * 
 * Decision Tree:
 * ┌─────────────────────────────────────────────────────────────┐
 * │ Shortcut Status                                              │
 * ├─────────────────────────────────────────────────────────────┤
 * │ NOT FOUND                                                    │
 * │   ├─ Already checked → Skip (respect user deletion)         │
 * │   ├─ Package install → Create shortcut                      │
 * │   └─ Manual install  → Skip (don't auto-create)             │
 * │                                                              │
 * │ POINTS TO CURRENT → OK (do nothing)                         │
 * │                                                              │
 * │ POINTS TO OTHER → Update shortcut to current executable     │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * Package Manager Detection:
 * - Microsoft Store: C:\Program Files\WindowsApps\*
 * - WinGet: *\AppData\Local\Microsoft\WinGet\Packages\*
 * - WinGet: *\AppData\Local\Microsoft\*WinGet*
 * - WinGet: *\WinGet\catime.exe
 * 
 * @return 0 on success, 1 on error
 * 
 * @note Requires COM to be initialized (function handles initialization)
 * @note Checks both user desktop and public desktop locations
 * @note State is persisted in configuration file to avoid redundant checks
 * 
 * @see IsShortcutCheckDone() for state tracking
 * @see SetShortcutCheckDone() for state management
 */
int CheckAndCreateShortcut(void);

#endif /* SHORTCUT_CHECKER_H */
