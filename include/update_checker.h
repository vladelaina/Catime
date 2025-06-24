/**
 * @file update_checker.h
 * @brief Minimalist application update checking functionality interface
 * 
 * This file defines the application's interfaces for checking updates, opening browser for downloads, and deleting configuration files.
 */

#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <windows.h>

/**
 * @brief Check for application updates
 * @param hwnd Window handle
 * 
 * Connects to GitHub to check if there's a new version. If available, prompts the user whether to download via browser.
 * If the user confirms, opens the browser to the download page, deletes the configuration file, and exits the program.
 */
void CheckForUpdate(HWND hwnd);

/**
 * @brief Silently check for application updates
 * @param hwnd Window handle
 * @param silentCheck Whether to show prompt only when updates are available
 * 
 * Connects to GitHub to check if there's a new version.
 * If silentCheck is TRUE, only shows a prompt when updates are available;
 * If FALSE, shows results regardless of whether updates are available.
 */
void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck);

/**
 * @brief Compare version numbers
 * @param version1 First version number string
 * @param version2 Second version number string
 * @return Returns 1 if version1 > version2, 0 if equal, -1 if version1 < version2
 */
int CompareVersions(const char* version1, const char* version2);

#endif // UPDATE_CHECKER_H 