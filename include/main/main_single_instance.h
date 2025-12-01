/**
 * @file main_single_instance.h
 * @brief Single instance detection and management
 */

#ifndef MAIN_SINGLE_INSTANCE_H
#define MAIN_SINGLE_INSTANCE_H

#include <windows.h>

/**
 * Find existing instance window handle (including desktop layer)
 * @return Window handle if found, NULL otherwise
 */
HWND FindExistingInstanceWindow(void);

/**
 * Handle single instance logic
 * @param lpCmdLine Command line arguments
 * @param outMutex Output mutex handle
 * @return TRUE to continue startup, FALSE to exit
 */
BOOL HandleSingleInstance(LPWSTR lpCmdLine, HANDLE* outMutex);

/**
 * Get global mutex handle for emergency cleanup
 * Used in crash handlers where normal cleanup path is not available
 * @return Global mutex handle or NULL if not created yet
 */
HANDLE GetGlobalMutexHandle(void);

/**
 * Clear global mutex handle after cleanup
 * Prevents double-free in crash scenarios
 */
void ClearGlobalMutexHandle(void);

/**
 * Verify single instance mutex is still valid
 * Can be called periodically to ensure single instance state
 * @param hMutex Mutex handle to verify
 * @return TRUE if mutex is valid and owned by this process
 */
BOOL VerifySingleInstanceMutex(HANDLE hMutex);

#endif /* MAIN_SINGLE_INSTANCE_H */

