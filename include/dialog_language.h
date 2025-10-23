/**
 * @file dialog_language.h
 * @brief Dialog localization and multi-language support system
 * 
 * Provides comprehensive localization support for Windows dialogs and their
 * child controls. The system handles:
 * - Dialog title translations
 * - Control text localization with fallback support
 * - Special text processing (newline conversion, version formatting)
 * - Support for multiple control types (buttons, static text, edit boxes, etc.)
 * 
 * The localization system uses a combination of:
 * 1. Static lookup tables for special controls requiring custom handling
 * 2. Generic text-based lookup for standard controls
 * 3. Fallback text when translations are unavailable
 * 
 * @version 2.0 - Enhanced documentation and improved structure
 */

#ifndef DIALOG_LANGUAGE_H
#define DIALOG_LANGUAGE_H

#include <windows.h>

/**
 * @brief Initialize dialog language support system
 * 
 * Prepares the localization system for use. This function should be called
 * once during application startup before any dialogs are created.
 * 
 * @return TRUE on success, FALSE on failure
 * 
 * @note Currently returns TRUE immediately as no initialization is required.
 *       Reserved for future use if initialization becomes necessary.
 * 
 * @see ApplyDialogLanguage
 */
BOOL InitDialogLanguageSupport(void);

/**
 * @brief Apply localized text to dialog and all its child controls
 * 
 * Automatically localizes a dialog window by:
 * 1. Setting the dialog title from localization tables
 * 2. Enumerating all child controls and applying appropriate translations
 * 3. Processing special controls (version text, newline conversion, etc.)
 * 
 * This function should be called in the WM_INITDIALOG handler of dialog
 * procedures to ensure all text is properly localized when the dialog
 * first appears.
 * 
 * @param hwndDlg Dialog window handle
 * @param dialogID Dialog resource identifier (e.g., IDD_ABOUT_DIALOG)
 * 
 * @return TRUE if localization was applied successfully, FALSE if hwndDlg is NULL
 * 
 * @note The function gracefully handles missing translations by keeping
 *       original text unchanged or using fallback text.
 * 
 * @example
 * @code
 * INT_PTR CALLBACK MyDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
 *     switch (msg) {
 *         case WM_INITDIALOG:
 *             ApplyDialogLanguage(hwndDlg, IDD_MY_DIALOG);
 *             return TRUE;
 *         // ... other cases
 *     }
 *     return FALSE;
 * }
 * @endcode
 * 
 * @see GetDialogLocalizedString
 */
BOOL ApplyDialogLanguage(HWND hwndDlg, int dialogID);

/**
 * @brief Get localized string for specific dialog control
 * 
 * Retrieves the localized text for a specific control within a dialog without
 * actually setting it. Useful for:
 * - Dynamic text generation
 * - Programmatic text manipulation
 * - Tooltips and status messages
 * 
 * The function searches in the following order:
 * 1. Special controls table (for controls with custom handling)
 * 2. Special buttons table
 * 3. Dialog titles table (if controlID is -1)
 * 
 * @param dialogID Dialog resource identifier (e.g., IDD_ABOUT_DIALOG)
 * @param controlID Control resource identifier (e.g., IDOK, IDC_BUTTON_OK)
 *                  Use -1 to request the dialog title
 * 
 * @return Pointer to localized wide-character string, or NULL if:
 *         - The dialog/control combination is not found in lookup tables
 *         - No translation is available for the requested item
 * 
 * @warning The returned pointer points to static or localization system memory.
 *          Do not free() or modify the returned string. The pointer remains
 *          valid for the lifetime of the application or until language changes.
 * 
 * @example
 * @code
 * // Get dialog title
 * const wchar_t* title = GetDialogLocalizedString(IDD_ABOUT_DIALOG, -1);
 * if (title) {
 *     wprintf(L"Dialog title: %s\n", title);
 * }
 * 
 * // Get control text
 * const wchar_t* buttonText = GetDialogLocalizedString(IDD_ABOUT_DIALOG, IDOK);
 * if (buttonText) {
 *     SetWindowTextW(hButton, buttonText);
 * }
 * @endcode
 * 
 * @see ApplyDialogLanguage
 */
const wchar_t* GetDialogLocalizedString(int dialogID, int controlID);

#endif /* DIALOG_LANGUAGE_H */
