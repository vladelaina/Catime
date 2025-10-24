/**
 * @file cli.h
 * @brief Command-line interface parsing and help dialog management
 * @version 2.0 - Refactored for better maintainability
 * 
 * Provides comprehensive CLI argument processing with:
 * - Table-driven command routing for maintainability
 * - Flexible timer input formats (duration, absolute time, units)
 * - Shortcut commands (s, u, p, q1-q3, pr, r, v, e, h)
 * - Interactive help dialog with multi-language support
 * 
 * Supported CLI formats:
 * - Single char: s (show time), u (count up), p (pomodoro)
 * - Two char: pr (pause/resume), q1-q3 (quick countdowns)
 * - Pomodoro index: p1, p2, ... (start indexed countdown)
 * - Timer input: "25", "1h 30m", "14:30t" (various time formats)
 */

#ifndef CLI_H
#define CLI_H

#include <windows.h>

/**
 * @brief Display or toggle CLI help dialog
 * @param hwnd Parent window handle
 * 
 * @details
 * - If dialog already open: closes it
 * - If dialog not open: creates and shows it
 * - Forces dialog to foreground with aggressive focus stealing
 * - Dialog remains topmost for visibility
 * 
 * @note Dialog is non-modal and can coexist with main window
 * @see CloseCliHelpDialog
 */
void ShowCliHelpDialog(HWND hwnd);

/**
 * @brief Process command-line arguments with table-driven routing
 * @param hwnd Window handle for timer operations
 * @param cmdLine Command-line string to parse (UTF-8)
 * @return TRUE if arguments were handled, FALSE if unrecognized
 * 
 * @details Command processing order:
 * 1. Check multi-character shortcut commands (q1-q3, pr)
 * 2. Check single-character commands (s, u, p, v, e, r, h)
 * 3. Check Pomodoro index commands (p1, p2, ...)
 * 4. Default: parse as timer input (duration or absolute time)
 * 
 * Supported timer formats:
 * - Minutes: "25" = 25 minutes
 * - Duration: "1h 30m", "90s", "1h 30m 15s"
 * - Compact: "130 45" = 1:30:45
 * - Absolute time: "14:30t" = countdown to 2:30 PM
 * - Compact target: "130t" = countdown to 1:30
 * 
 * @note Expands compact formats before parsing
 * @note Invalid input starts default countdown
 */
BOOL HandleCliArguments(HWND hwnd, const char* cmdLine);

/**
 * @brief Get handle to CLI help dialog window if open
 * @return Window handle or NULL if dialog not open
 * 
 * @note Used for IsDialogMessage processing in message loop
 * @see ShowCliHelpDialog
 */
HWND GetCliHelpDialog(void);

/**
 * @brief Close CLI help dialog if open
 * 
 * @details
 * - Checks if dialog window is valid before closing
 * - Cleans up internal dialog handle reference
 * - Safe to call even if dialog is not open
 * 
 * @see ShowCliHelpDialog
 */
void CloseCliHelpDialog(void);

#endif