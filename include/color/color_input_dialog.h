/**
 * @file color_input_dialog.h
 * @brief Text-based color input dialog with live preview (modeless version)
 * 
 * Allows users to type color codes directly (CSS names, hex, RGB).
 * Features real-time validation and preview.
 */

#ifndef COLOR_INPUT_DIALOG_H
#define COLOR_INPUT_DIALOG_H

#include <windows.h>

/**
 * @brief Show color input dialog (modeless)
 * @param hwndParent Parent window handle
 * 
 * @details
 * Opens modeless dialog for color input with live preview.
 * Result is sent via WM_DIALOG_COLOR message to parent.
 * Only one instance allowed at a time.
 */
void ShowColorInputDialog(HWND hwndParent);

/**
 * @brief Dialog procedure for text-based color input
 * @param hwndDlg Dialog handle
 * @param msg Windows message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 * 
 * @details
 * Empty input cancels (supports "changed my mind" workflow).
 * Validation errors show guidance dialog. Immediate config save.
 */
INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Subclassed edit control with live preview
 * @param hwnd Edit control handle
 * @param msg Windows message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 * 
 * @details
 * - Live preview on keystroke for immediate feedback
 * - Ctrl+A for select all (standard behavior)
 * - Enter for keyboard submission (accessibility)
 * - Preview on paste/cut (handles clipboard path)
 */
LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif /* COLOR_INPUT_DIALOG_H */

