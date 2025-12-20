/**
 * @file dialog_input.h
 * @brief Generic input dialog and time parsing (modeless version)
 */

#ifndef DIALOG_INPUT_H
#define DIALOG_INPUT_H

#include <windows.h>

/* ============================================================================
 * Modeless Dialog API
 * ============================================================================ */

/**
 * @brief Show countdown input dialog (modeless)
 * @param hwndParent Parent window handle
 * 
 * @details
 * Opens modeless dialog for custom countdown input.
 * Result is sent via WM_DIALOG_COUNTDOWN message to parent.
 * Only one instance allowed at a time.
 */
void ShowCountdownInputDialog(HWND hwndParent);

/**
 * @brief Show shortcut time settings dialog (modeless)
 * @param hwndParent Parent window handle
 * 
 * @details
 * Opens modeless dialog for quick countdown time configuration.
 * Config is saved directly, WM_DIALOG_SHORTCUT sent on completion.
 */
void ShowShortcutTimeDialog(HWND hwndParent);

/**
 * @brief Show startup time settings dialog (modeless)
 * @param hwndParent Parent window handle
 * 
 * @details
 * Opens modeless dialog for default startup time configuration.
 * Config is saved directly on completion.
 */
void ShowStartupTimeDialog(HWND hwndParent);

/**
 * @brief Show pomodoro time edit dialog (modeless)
 * @param hwndParent Parent window handle
 * @param timeIndex Index of pomodoro time to edit (0=work, 1=short break, 2=long break)
 * 
 * @details
 * Opens modeless dialog for editing specific pomodoro time.
 * Config is saved directly on completion.
 */
void ShowPomodoroTimeEditDialog(HWND hwndParent, int timeIndex);

/* ============================================================================
 * Generic Input Dialog (Internal)
 * ============================================================================ */

/**
 * @brief Generic input dialog procedure
 * @return Message processing result
 * 
 * @details
 * Supports multiple modes via lParam:
 * - CLOCK_IDD_DIALOG1: Custom countdown input
 * - CLOCK_IDD_SHORTCUT_DIALOG: Quick countdown times
 * - CLOCK_IDD_STARTUP_DIALOG: Default start time
 * - CLOCK_IDD_POMODORO_TIME_DIALOG: Pomodoro time
 * - CLOCK_IDD_POMODORO_LOOP_DIALOG: Loop count
 * 
 * @note Now operates as modeless dialog, uses DestroyWindow instead of EndDialog
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

/**
 * @brief Selected pomodoro time index for editing
 *
 * @details
 * Used to display current value when opening pomodoro time dialog.
 * Set to -1 when not in use.
 */
extern int g_pomodoroSelectedIndex;

#endif /* DIALOG_INPUT_H */

