/**
 * @file dialog_language.h
 * @brief Dialog multi-language support module header file
 * 
 * This file defines function interfaces for dialog multi-language support, allowing dialog text to be localized according to the application's language settings.
 */

#ifndef DIALOG_LANGUAGE_H
#define DIALOG_LANGUAGE_H

#include <windows.h>

/**
 * @brief Initialize dialog multi-language support
 * 
 * Call this function at application startup to initialize the dialog multi-language support system.
 * 
 * @return BOOL Whether initialization was successful
 */
BOOL InitDialogLanguageSupport(void);

/**
 * @brief Apply multi-language support to dialog
 * 
 * Update text elements in the dialog based on current application language settings.
 * This function should be called during dialog WM_INITDIALOG message handling.
 * 
 * @param hwndDlg Dialog handle
 * @param dialogID Dialog resource ID
 * @return BOOL Whether operation was successful
 */
BOOL ApplyDialogLanguage(HWND hwndDlg, int dialogID);

/**
 * @brief Get localized text for dialog element
 * 
 * Get localized text for dialog element based on current application language settings.
 * 
 * @param dialogID Dialog resource ID
 * @param controlID Control resource ID
 * @return const wchar_t* Localized text, returns NULL if not found
 */
const wchar_t* GetDialogLocalizedString(int dialogID, int controlID);

#endif /* DIALOG_LANGUAGE_H */ 