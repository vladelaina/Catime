/**
 * @file dialog_procedure.h
 * @brief Dialog message handling procedure interface
 * 
 * This file defines the application's dialog message handling callback function interfaces,
 * handling all dialog message events including initialization, color management, button clicks and keyboard events.
 */

#ifndef DIALOG_PROCEDURE_H
#define DIALOG_PROCEDURE_H

#include <windows.h>

/**
 * @brief Input dialog procedure
 * @param hwndDlg Dialog handle
 * @param msg Message type
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return INT_PTR Message handling result
 * 
 * Handles countdown input dialog:
 * 1. Control initialization and focus setting
 * 2. Background/control color management
 * 3. OK button click handling
 * 4. Enter key response
 * 5. Resource cleanup
 */
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Show about dialog
 * @param hwndParent Parent window handle
 * 
 * Displays an about dialog containing program version, author, and third-party library information.
 * Includes the following link buttons:
 * - Credits (IDC_CREDITS) - Opens webpage https://vladelaina.github.io/Catime/#thanks
 * - Feedback (IDC_FEEDBACK)
 * - GitHub (IDC_GITHUB)
 * - Copyright Notice (IDC_COPYRIGHT_LINK) - Opens webpage https://github.com/vladelaina/Catime?tab=readme-ov-file#%EF%B8%8F%E7%89%88%E6%9D%83%E5%A3%B0%E6%98%8E
 * - Support (IDC_SUPPORT) - Opens webpage https://vladelaina.github.io/Catime/support.html
 */
void ShowAboutDialog(HWND hwndParent);

INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Show error dialog
 * @param hwndParent Parent window handle
 * 
 * Displays a unified error prompt dialog.
 */
void ShowErrorDialog(HWND hwndParent);

/**
 * @brief Show pomodoro loop count setting dialog
 * @param hwndParent Parent window handle
 * 
 * Displays a dialog for setting the pomodoro loop count.
 * Allows the user to input a loop count between 1-99.
 */
void ShowPomodoroLoopDialog(HWND hwndParent);

INT_PTR CALLBACK PomodoroLoopDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Show website URL input dialog
 * @param hwndParent Parent window handle
 * 
 * Displays a dialog for entering the website URL to open when timeout occurs.
 */
void ShowWebsiteDialog(HWND hwndParent);

/**
 * @brief Show pomodoro combination dialog
 * @param hwndParent Parent window handle
 * 
 * Displays a dialog for setting pomodoro time combinations.
 * Allows the user to input custom pomodoro time sequences.
 */
void ShowPomodoroComboDialog(HWND hwndParent);

/**
 * @brief Parse time input
 * @param input Input string (e.g., "25m", "30s", "1h30m")
 * @param seconds Output seconds
 * @return BOOL Returns TRUE on successful parsing, FALSE on failure
 */
BOOL ParseTimeInput(const char* input, int* seconds);

/**
 * @brief Show notification message settings dialog
 * @param hwndParent Parent window handle
 * 
 * Displays notification message settings dialog for modifying various notification prompt texts.
 */
void ShowNotificationMessagesDialog(HWND hwndParent);

/**
 * @brief Show notification display settings dialog
 * @param hwndParent Parent window handle
 * 
 * Displays notification display settings dialog for modifying notification display time and opacity.
 */
void ShowNotificationDisplayDialog(HWND hwndParent);

/**
 * @brief Show integrated notification settings dialog
 * @param hwndParent Parent window handle
 * 
 * Displays an integrated dialog that includes both notification content and notification display settings.
 * Unlike the previous two separate dialogs, this dialog integrates all notification-related settings.
 */
void ShowNotificationSettingsDialog(HWND hwndParent);

/**
 * @brief Global countdown input dialog handle
 * 
 * Used to track the currently displayed countdown input dialog.
 * If NULL, indicates no input dialog is currently displayed.
 */
extern HWND g_hwndInputDialog;

#endif // DIALOG_PROCEDURE_H