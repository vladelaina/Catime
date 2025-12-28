/**
 * @file dialog_error.h
 * @brief Simple error dialog for input validation failures (modeless)
 */

#ifndef DIALOG_ERROR_H
#define DIALOG_ERROR_H

#include <windows.h>

/**
 * @brief Show modeless error dialog with localized message
 * @param hwndParent Parent window handle
 * 
 * @details
 * Displays: "Invalid input format, please try again."
 * Single OK button, auto-centers on primary screen.
 * Non-blocking - returns immediately after showing dialog.
 */
void ShowErrorDialog(HWND hwndParent);

/**
 * @brief Show modeless error dialog and refocus to edit control when closed
 * @param hwndParent Parent window handle (dialog containing the edit control)
 * @param editControlId ID of the edit control to refocus after dialog closes
 * 
 * @details
 * Same as ShowErrorDialog but automatically refocuses to the specified
 * edit control when the error dialog is dismissed.
 */
void ShowErrorDialogWithRefocus(HWND hwndParent, int editControlId);

/**
 * @brief Error dialog procedure
 */
INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

#endif /* DIALOG_ERROR_H */
