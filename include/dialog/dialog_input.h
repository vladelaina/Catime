/**
 * @file dialog_input.h
 * @brief Generic input dialog and time parsing
 */

#ifndef DIALOG_INPUT_H
#define DIALOG_INPUT_H

#include <windows.h>

/* ============================================================================
 * Time Input Parsing
 * ============================================================================ */

/**
 * @brief Parse flexible time input to seconds
 * @param input Time string (UTF-8)
 * @param seconds Output
 * @return TRUE on success
 * 
 * @details Formats:
 * - Plain: "25" = 25min
 * - Units: "1h 30m", "90s", "1h 30m 15s"
 * - Compact: "130 45" = 1:30:45
 * 
 * @note Max: 99:59:59. Returns FALSE for negative/invalid.
 */
BOOL ParseTimeInput(const char* input, int* seconds);

/* ============================================================================
 * Generic Input Dialog
 * ============================================================================ */

/**
 * @brief Generic input dialog procedure
 * @return Message processing result
 * 
 * @details
 * Supports multiple modes via lParam:
 * - CLOCK_IDD_SHORTCUT_DIALOG: Quick countdown times
 * - CLOCK_IDD_STARTUP_DIALOG: Default start time
 * - CLOCK_IDD_POMODORO_TIME_DIALOG: Pomodoro time
 * - CLOCK_IDD_POMODORO_LOOP_DIALOG: Loop count
 * 
 * @note Uses global inputText buffer for result
 */
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Global input text buffer
 * 
 * @details
 * Dialog result is stored here. Check after dialog closes.
 * Size: 256 wide characters.
 */
extern wchar_t inputText[256];

/**
 * @brief Active input dialog handle
 * 
 * @details
 * For IsDialogMessage(), preventing duplicates.
 * NULL when closed.
 */
extern HWND g_hwndInputDialog;

#endif /* DIALOG_INPUT_H */

