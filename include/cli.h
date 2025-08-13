/**
 * @file cli.h
 * @brief Command line argument handling for starting countdown directly
 */

#ifndef CLI_H
#define CLI_H

#include <windows.h>

/**
 * @brief Show the CLI help dialog (modeless). Safe to call multiple times.
 * @param hwnd Main window handle (used as parent)
 */
void ShowCliHelpDialog(HWND hwnd);

/**
 * @brief Handle command line arguments to start countdown if present
 * @param hwnd Main window handle (needed to reset timers)
 * @param cmdLine Raw command line string from WinMain (lpCmdLine)
 * @return BOOL TRUE if a valid countdown was parsed and applied, otherwise FALSE
 *
 * Supported formats (same as UI input rules, with a few CLI conveniences):
 * - "25" => 25 minutes
 * - "25h" => 25 hours
 * - "25s" => 25 seconds
 * - "25 30" => 25 minutes 30 seconds
 * - "25 30m" => 25 hours 30 minutes
 * - "1 30 20" => 1 hour 30 minutes 20 seconds
 * - "130 20" => 1 hour 30 minutes 20 seconds (CLI convenience: HHMM + SS)
 * - "17 20t" => countdown to 17:20 today (or tomorrow if time has passed)
 * - "1720t" => countdown to 17:20 (CLI convenience: HHMMt)
 * - "9 9 9t" => countdown to 9:9:9
 */
BOOL HandleCliArguments(HWND hwnd, const char* cmdLine);

/**
 * @brief Get current CLI help dialog handle (if visible)
 */
HWND GetCliHelpDialog(void);

/**
 * @brief Close CLI help dialog if exists
 */
void CloseCliHelpDialog(void);

#endif // CLI_H


