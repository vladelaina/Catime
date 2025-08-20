/**
 * @file cli.h
 * @brief Command-line interface handling
 * 
 * Functions for processing command-line arguments and displaying CLI help
 */

#ifndef CLI_H
#define CLI_H

#include <windows.h>

/**
 * @brief Display command-line help dialog
 * @param hwnd Parent window handle
 */
void ShowCliHelpDialog(HWND hwnd);

/**
 * @brief Process command-line arguments
 * @param hwnd Window handle for timer operations
 * @param cmdLine Command-line string to parse
 * @return TRUE if arguments were handled, FALSE otherwise
 */
BOOL HandleCliArguments(HWND hwnd, const char* cmdLine);

/**
 * @brief Get handle to CLI help dialog window
 * @return Window handle or NULL if dialog not open
 */
HWND GetCliHelpDialog(void);

/**
 * @brief Close CLI help dialog if open
 */
void CloseCliHelpDialog(void);

#endif