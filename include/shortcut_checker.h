/**
 * @file shortcut_checker.h
 * @brief Desktop shortcut management for package manager installs
 * 
 * Auto-creates shortcuts for package manager installs (Store/WinGet) to improve
 * discoverability. Respects user deletion (doesn't recreate if manually removed).
 * State persistence prevents redundant checks on subsequent runs.
 */

#ifndef SHORTCUT_CHECKER_H
#define SHORTCUT_CHECKER_H

#include <windows.h>

/**
 * @brief Check and manage desktop shortcut
 * @return 0 on success, 1 on error
 * 
 * @details Decision logic:
 * - NOT FOUND + Already checked → Skip (respect user deletion)
 * - NOT FOUND + Package install → Create shortcut
 * - NOT FOUND + Manual install → Skip (don't auto-create)
 * - POINTS TO CURRENT → OK (do nothing)
 * - POINTS TO OTHER → Update to current executable
 * 
 * Package detection paths:
 * - Store: C:\Program Files\WindowsApps\*
 * - WinGet: *\AppData\Local\Microsoft\WinGet\Packages\*
 * - WinGet: *\AppData\Local\Microsoft\*WinGet*
 * - WinGet: *\WinGet\catime.exe
 * 
 * @note Handles COM initialization internally
 * @note Checks user desktop and public desktop
 * @note State persisted in config to avoid redundant checks
 */
int CheckAndCreateShortcut(void);

#endif /* SHORTCUT_CHECKER_H */
