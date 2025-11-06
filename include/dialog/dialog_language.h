/**
 * @file dialog_language.h
 * @brief Dialog localization system with three-tier lookup
 * 
 * 1. Special controls (custom handling)
 * 2. Standard controls (generic text)
 * 3. Fallback text (missing translations)
 * 
 * Graceful degradation ensures dialogs always display something useful.
 */

#ifndef DIALOG_LANGUAGE_H
#define DIALOG_LANGUAGE_H

#include <windows.h>

/**
 * @brief Initialize localization system
 * @return TRUE (currently no-op, reserved for future use)
 * 
 * @note Call during app startup before creating dialogs
 */
BOOL InitDialogLanguageSupport(void);

/**
 * @brief Apply localized text to dialog and all child controls
 * @param hwndDlg Dialog handle
 * @param dialogID Dialog resource ID (e.g., IDD_ABOUT_DIALOG)
 * @return TRUE on success, FALSE if hwndDlg is NULL
 * 
 * @details
 * Localizes title, enumerates controls, processes special controls.
 * Handles missing translations gracefully (keeps original or uses fallback).
 * 
 * @note Call in WM_INITDIALOG handler
 * 
 * @example
 * ```c
 * case WM_INITDIALOG:
 *     ApplyDialogLanguage(hwndDlg, IDD_MY_DIALOG);
 *     return TRUE;
 * ```
 */
BOOL ApplyDialogLanguage(HWND hwndDlg, int dialogID);

/**
 * @brief Get localized string without applying it
 * @param dialogID Dialog resource ID
 * @param controlID Control ID (use -1 for dialog title)
 * @return Wide-string pointer or NULL if not found
 * 
 * @details
 * Search order: special controls → buttons → dialog titles.
 * Useful for dynamic text generation, tooltips, status messages.
 * 
 * @warning Returned pointer is static/system memory - do not free or modify.
 *          Valid until language changes.
 * 
 * @example
 * ```c
 * const wchar_t* title = GetDialogLocalizedString(IDD_ABOUT_DIALOG, -1);
 * ```
 */
const wchar_t* GetDialogLocalizedString(int dialogID, int controlID);

#endif /* DIALOG_LANGUAGE_H */
