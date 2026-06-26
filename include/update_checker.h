/**
 * @file update_checker.h
 * @brief GitHub release checker with semantic versioning
 * 
 * Semantic Versioning 2.0.0 with prerelease support (alpha < beta < rc < stable).
 * Silent mode keeps automatic checks non-intrusive and stores results for later UI.
 */

#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <stddef.h>
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
 * @param silentCheck TRUE to suppress automatic check dialogs
 * 
 * @details Silent mode for startup checks stores update state without prompting.
 * 
 * @warning Blocks UI thread (use async_update_checker.h instead)
 */
void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck);

/**
 * @brief Request cancellation of an in-flight update check
 *
 * Closes active WinINet handles so a background check can leave blocking
 * network calls during shutdown.
 */
void RequestUpdateCheckCancel(void);

/**
 * @brief Clear any previous cancellation before starting a new update check
 */
void ResetUpdateCheckCancel(void);

/**
 * @brief Release update checker synchronization and tracked network resources
 *
 * Must be called only after update worker threads have stopped.
 */
void CleanupUpdateCheckResources(void);

/**
 * @brief Get the latest known update availability status
 * @param versionBuffer Optional output buffer for the latest version string
 * @param bufferSize Size of versionBuffer in bytes
 * @return TRUE if a newer version is known, FALSE otherwise
 */
BOOL GetNewVersionStatus(char* versionBuffer, size_t bufferSize);

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
