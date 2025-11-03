/**
 * @file update_checker.h
 * @brief GitHub release checker with semantic versioning
 * 
 * Semantic Versioning 2.0.0 with prerelease support (alpha < beta < rc < stable).
 * Silent mode prevents "up-to-date" spam during automatic checks.
 */

#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <windows.h>

/**
 * @brief Check for updates (interactive mode)
 * @param hwnd Parent window
 * 
 * @details Shows all dialogs (new version, up-to-date, errors)
 * 
 * @warning Blocks UI thread (use async_update_checker.h instead)
 */
void CheckForUpdate(HWND hwnd);

/**
 * @brief Check for updates with optional silent mode
 * @param hwnd Parent window
 * @param silentCheck TRUE to suppress "up-to-date" dialogs
 * 
 * @details Silent mode for startup checks (shows only new versions and errors)
 * 
 * @warning Blocks UI thread (use async_update_checker.h instead)
 */
void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck);

/**
 * @brief Compare semantic version strings
 * @param version1 First version
 * @param version2 Second version
 * @return Negative if v1 < v2, 0 if equal, positive if v1 > v2
 * 
 * @details
 * Format: MAJOR.MINOR.PATCH[-PRERELEASE]
 * Prerelease priority: alpha < beta < rc < stable
 * Numeric suffix: alpha2 < alpha10
 */
int CompareVersions(const char* version1, const char* version2);

#endif // UPDATE_CHECKER_H
