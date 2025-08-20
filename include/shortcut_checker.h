/**
 * @file shortcut_checker.h
 * @brief Desktop shortcut management
 * 
 * Functions for checking and creating application shortcuts
 */

#ifndef SHORTCUT_CHECKER_H
#define SHORTCUT_CHECKER_H

#include <windows.h>

/**
 * @brief Check for existing shortcut and create if needed
 * @return Status code indicating operation result
 */
int CheckAndCreateShortcut(void);

#endif