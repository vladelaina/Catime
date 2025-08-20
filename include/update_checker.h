/**
 * @file update_checker.h
 * @brief Application update checking functionality
 * 
 * Functions for checking application updates and version comparison
 */

#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <windows.h>

/**
 * @brief Check for application updates with user interaction
 * @param hwnd Window handle for displaying update dialogs
 */
void CheckForUpdate(HWND hwnd);

/**
 * @brief Check for application updates with optional silent mode
 * @param hwnd Window handle for displaying update dialogs
 * @param silentCheck TRUE for silent check, FALSE for interactive
 */
void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck);

/**
 * @brief Compare two version strings
 * @param version1 First version string (e.g., "1.2.3")
 * @param version2 Second version string (e.g., "1.2.4")
 * @return Negative if version1 < version2, 0 if equal, positive if version1 > version2
 */
int CompareVersions(const char* version1, const char* version2);

#endif