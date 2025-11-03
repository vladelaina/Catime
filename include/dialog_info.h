/**
 * @file dialog_info.h
 * @brief Informational dialogs (About, Font License, Website/File input)
 */

#ifndef DIALOG_INFO_H
#define DIALOG_INFO_H

#include <windows.h>

/* ============================================================================
 * About Dialog
 * ============================================================================ */

/**
 * @brief Show About dialog (app version, license, links)
 * @param hwndParent Parent window
 * 
 * @details
 * Displays: Version, build date, author, license, GitHub/Bilibili links
 * Supports: Update check button, single instance (prevents duplicates)
 */
void ShowAboutDialog(HWND hwndParent);

/**
 * @brief About dialog procedure
 */
INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/* ============================================================================
 * Font License Dialog
 * ============================================================================ */

/**
 * @brief Show font license agreement dialog
 * @param hwndParent Parent window
 * @return IDOK if accepted, IDCANCEL if rejected
 * 
 * @details
 * Required on first run or when [FONT_LICENSE]shown=0.
 * Blocks timer until accepted.
 */
INT_PTR ShowFontLicenseDialog(HWND hwndParent);

/**
 * @brief Font license dialog procedure
 */
INT_PTR CALLBACK FontLicenseDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/* ============================================================================
 * Website Input Dialog
 * ============================================================================ */

/**
 * @brief Show URL input dialog for timeout action
 * @param hwndParent Parent window
 * 
 * @details
 * For TIMEOUT_OPEN_WEBSITE action.
 * Validates URL format (http://, https://).
 * Stores result in TimeoutWebsite global.
 * Uses current CLOCK_TIMEOUT_WEBSITE_URL as default value.
 */
void ShowWebsiteDialog(HWND hwndParent);

/**
 * @brief Website input dialog procedure
 */
INT_PTR CALLBACK WebsiteDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Global URL buffer for result
 * 
 * @details
 * Populated after ShowWebsiteDialog() closes successfully.
 * Size: 512 bytes (UTF-8).
 */
extern char g_websiteInput[512];

/* ============================================================================
 * CLI Help Dialog (Implemented in cli.c)
 * ============================================================================ */

/**
 * @brief Show command-line help dialog
 * @param hwndParent Parent window
 * 
 * @details
 * Displays: Usage, commands, examples for CLI mode.
 * Read-only multiline edit control.
 * 
 * @note This function is implemented in cli.c, not in dialog_info.c
 *       Declared here for convenience as it's still a dialog function.
 */
void ShowCliHelpDialog(HWND hwndParent);

/**
 * @brief CLI help dialog procedure
 * 
 * @note Implemented in cli.c
 */
INT_PTR CALLBACK CliHelpDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

#endif /* DIALOG_INFO_H */

