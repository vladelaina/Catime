/**
 * @file package_identity.h
 * @brief Runtime detection for MSIX/Microsoft Store package identity
 */

#ifndef UTILS_PACKAGE_IDENTITY_H
#define UTILS_PACKAGE_IDENTITY_H

#include <windows.h>
#include <stddef.h>

/**
 * @brief Determine whether the current process has an MSIX/AppX identity
 * @return TRUE for packaged processes, FALSE for ordinary Win32 processes
 *
 * Uses the package identity APIs dynamically so older supported Windows
 * versions can still load Catime.
 */
BOOL IsRunningPackagedApp(void);

/**
 * @brief Get the current package family name
 * @param familyName Output buffer
 * @param familyNameSize Buffer size in wide characters
 * @return TRUE when running packaged and the family name was retrieved
 */
BOOL GetCurrentPackageFamilyNameSafeW(wchar_t* familyName, size_t familyNameSize);

#endif /* UTILS_PACKAGE_IDENTITY_H */
