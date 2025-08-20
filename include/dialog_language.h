/**
 * @file dialog_language.h
 * @brief Dialog localization support
 * 
 * Functions for applying language translations to dialog controls
 */

#ifndef DIALOG_LANGUAGE_H
#define DIALOG_LANGUAGE_H

#include <windows.h>

/**
 * @brief Initialize dialog language support system
 * @return TRUE on success, FALSE on failure
 */
BOOL InitDialogLanguageSupport(void);

/**
 * @brief Apply localized text to dialog controls
 * @param hwndDlg Dialog window handle
 * @param dialogID Dialog resource identifier
 * @return TRUE on success, FALSE on failure
 */
BOOL ApplyDialogLanguage(HWND hwndDlg, int dialogID);

/**
 * @brief Get localized string for dialog control
 * @param dialogID Dialog resource identifier
 * @param controlID Control resource identifier
 * @return Localized string or NULL if not found
 */
const wchar_t* GetDialogLocalizedString(int dialogID, int controlID);

#endif