/**
 * @file shortcut_checker.h
 * @brief Functionality for detecting and creating desktop shortcuts
 *
 * Provides functionality to detect if the program is installed from the app store or WinGet,
 * and create desktop shortcuts when needed.
 */

#ifndef SHORTCUT_CHECKER_H
#define SHORTCUT_CHECKER_H

#include <windows.h>

/**
 * @brief Check and create desktop shortcut
 * 
 * Checks if the program is installed from the Windows App Store or WinGet,
 * and if it is and not marked as already checked in the configuration, checks if
 * there is a shortcut for the program on the desktop, and creates one if there isn't.
 * 
 * @return int 0 indicates no need to create or creation successful, 1 indicates creation failed
 */
int CheckAndCreateShortcut(void);

#endif /* SHORTCUT_CHECKER_H */ 