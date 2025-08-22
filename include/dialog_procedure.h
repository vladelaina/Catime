/**
 * @file dialog_procedure.h
 * @brief Dialog procedures and dialog management
 * 
 * Functions for creating and managing various application dialogs
 */

#ifndef DIALOG_PROCEDURE_H
#define DIALOG_PROCEDURE_H

#include <windows.h>

/**
 * @brief General dialog procedure for input dialogs
 * @param hwndDlg Dialog window handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 */
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Display application about dialog
 * @param hwndParent Parent window handle
 */
void ShowAboutDialog(HWND hwndParent);

/**
 * @brief Dialog procedure for about dialog
 * @param hwndDlg Dialog window handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 */
INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Display error dialog for invalid input
 * @param hwndParent Parent window handle
 */
void ShowErrorDialog(HWND hwndParent);

/**
 * @brief Display Pomodoro loop count configuration dialog
 * @param hwndParent Parent window handle
 */
void ShowPomodoroLoopDialog(HWND hwndParent);

/**
 * @brief Dialog procedure for Pomodoro loop dialog
 * @param hwndDlg Dialog window handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 */
INT_PTR CALLBACK PomodoroLoopDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Display website configuration dialog
 * @param hwndParent Parent window handle
 */
void ShowWebsiteDialog(HWND hwndParent);

/**
 * @brief Display Pomodoro combination settings dialog
 * @param hwndParent Parent window handle
 */
void ShowPomodoroComboDialog(HWND hwndParent);

/**
 * @brief Parse time input string to seconds
 * @param input Input time string
 * @param seconds Output buffer for parsed seconds
 * @return TRUE if parsing successful, FALSE otherwise
 */
BOOL ParseTimeInput(const char* input, int* seconds);

/**
 * @brief Display notification messages configuration dialog
 * @param hwndParent Parent window handle
 */
void ShowNotificationMessagesDialog(HWND hwndParent);

/**
 * @brief Display notification display settings dialog
 * @param hwndParent Parent window handle
 */
void ShowNotificationDisplayDialog(HWND hwndParent);

/**
 * @brief Display notification settings dialog
 * @param hwndParent Parent window handle
 */
void ShowNotificationSettingsDialog(HWND hwndParent);

/**
 * @brief Show font license agreement dialog
 * @param hwndParent Parent window handle
 * @return Dialog result (IDOK if agreed, IDCANCEL if declined)
 */
INT_PTR ShowFontLicenseDialog(HWND hwndParent);

/** @brief Global handle to current input dialog */
extern HWND g_hwndInputDialog;

#endif