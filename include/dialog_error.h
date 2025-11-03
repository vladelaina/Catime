/**
 * @file dialog_error.h
 * @brief Simple error dialog for input validation failures
 */

#ifndef DIALOG_ERROR_H
#define DIALOG_ERROR_H

#include <windows.h>

/**
 * @brief Show modal error dialog with localized message
 * @param hwndParent Parent window handle
 * 
 * @details
 * Displays: "输入格式无效，请重新输入。" / "Invalid input format, please try again."
 * Single OK button, auto-centers on primary screen.
 */
void ShowErrorDialog(HWND hwndParent);

/**
 * @brief Error dialog procedure
 */
INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

#endif /* DIALOG_ERROR_H */

