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

#endif /* MAIN_SINGLE_INSTANCE_H */

