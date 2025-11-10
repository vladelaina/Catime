/**
 * @file cli.h
 * @brief Command-line interface for rapid timer control
 *
 * Supports multiple input formats for different user preferences:
 * - Shortcuts (s, p, pr): Fast common operations
 * - Natural (1h 30m): Intuitive duration input
 * - Numeric shorthand (25): Minutes, (1 30): Minutes:seconds, (1 30 15): Hours:minutes:seconds
 * - Absolute (14 30t): Deadline-based tasks
 */

#ifndef CLI_H
#define CLI_H

#include <windows.h>

/**
 * @brief Toggles help dialog visibility
 * @param hwnd Parent window handle
 * 
 * @details
 * Non-modal to allow simultaneous help reference and command testing.
 * Uses aggressive focus stealing because Windows fails to reliably bring
 * topmost windows to front on request.
 */
void ShowCliHelpDialog(HWND hwnd);

/**
 * @brief Parses and executes CLI command
 * @param hwnd Window handle for timer operations
 * @param cmdLine Command-line string (UTF-8)
 * @return TRUE if recognized, FALSE otherwise
 * 
 * @details
 * Processing order (specificity to avoid ambiguity):
 * 1. Multi-char shortcuts (q1-q3, pr)
 * 2. Single-char commands (s, u, p, v, e, r, h)
 * 3. Pomodoro indices (p1, p2, ...)
 * 4. Time expressions (default fallback)
 * 
 * Invalid input starts default timer instead of showing errors to maintain
 * rapid workflow (predictable behavior better than interruption).
 * 
 * @note Return indicates recognition, not execution success
 */
BOOL HandleCliArguments(HWND hwnd, const char* cmdLine);

/**
 * @brief Gets help dialog handle for message routing
 * @return Window handle or NULL if not open
 * 
 * @details Required for IsDialogMessage() to enable Tab navigation.
 */
HWND GetCliHelpDialog(void);

/**
 * @brief Closes help dialog safely
 * 
 * @details
 * Validates handle before closing to prevent crashes if dialog was
 * closed externally (user clicked X, parent destroyed).
 * 
 * @note Safe to call multiple times
 */
void CloseCliHelpDialog(void);

#endif