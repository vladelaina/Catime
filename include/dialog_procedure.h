/**
 * @file dialog_procedure.h
 * @brief Dialog procedures for timer input, settings, and info dialogs
 * 
 * Flexible time parsing supports multiple formats (duration, absolute, units).
 * Auto-centering on primary monitor handles multi-monitor setups correctly.
 * Keyboard shortcuts (Ctrl+A, Enter) enable rapid keyboard-only workflow.
 */

#ifndef DIALOG_PROCEDURE_H
#define DIALOG_PROCEDURE_H

#include <windows.h>

/* ============================================================================
 * Timer input dialogs
 * ============================================================================ */

/**
 * @brief Timer input dialog procedure (countdown/countup/pomodoro)
 * @return Message processing result
 * 
 * @details
 * Ctrl+A for select all, Enter to submit, empty input cancels.
 * Auto-focuses edit, validates input, shows error on failure.
 * 
 * @note Uses global inputText buffer for result
 */
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Parse flexible time input to seconds
 * @param input Time string (UTF-8)
 * @param seconds Output
 * @return TRUE on success
 * 
 * @details Formats:
 * - Plain: "25" = 25min
 * - Units: "1h 30m", "90s", "1h 30m 15s"
 * - Compact: "130 45" = 1:30:45
 * - Absolute: "14:30t" = countdown to 2:30 PM
 * - Target: "130t" = countdown to 1:30
 * 
 * Unit aliases: h/hr/hour/时, m/min/minute/分, s/sec/second/秒
 * 
 * @note Max: 99:59:59. Returns FALSE for negative/invalid.
 */
BOOL ParseTimeInput(const char* input, int* seconds);

/* ============================================================================
 * Information dialogs
 * ============================================================================ */

/**
 * @brief Show about dialog (non-modal)
 * @param hwndParent Parent handle
 * 
 * @details Rich text with markdown, clickable links, auto-sizes
 */
void ShowAboutDialog(HWND hwndParent);

/**
 * @brief About dialog procedure with rich text
 * @return Message processing result
 * 
 * @details
 * RichEdit for markdown, clickable hyperlinks, dismisses on key/click.
 */
INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Show validation error dialog (modal)
 * @param hwndParent Parent handle
 * 
 * @details Localized message, single OK button, centered
 */
void ShowErrorDialog(HWND hwndParent);

/**
 * @brief Show font license agreement (modal)
 * @param hwndParent Parent handle
 * @return IDOK if accepted, IDCANCEL if declined
 * 
 * @details Scrollable license text, blocks until user responds
 */
INT_PTR ShowFontLicenseDialog(HWND hwndParent);

/* ============================================================================
 * Pomodoro configuration dialogs
 * ============================================================================ */

/**
 * @brief Show Pomodoro loop count dialog
 * @param hwndParent Parent handle
 * 
 * @details
 * Configure loops (1-99), validates input, empty cancels.
 */
void ShowPomodoroLoopDialog(HWND hwndParent);

/**
 * @brief Pomodoro loop dialog procedure
 */
INT_PTR CALLBACK PomodoroLoopDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Show Pomodoro time combo dialog (work/short/long breaks)
 * @param hwndParent Parent handle
 * 
 * @details
 * Three input fields, flexible parsing, live validation, atomic save.
 */
void ShowPomodoroComboDialog(HWND hwndParent);

/* ============================================================================
 * Settings dialogs
 * ============================================================================ */

/**
 * @brief Show URL config dialog (no validation for flexibility)
 * @param hwndParent Parent handle
 * 
 * @details Empty input disables click action
 */
void ShowWebsiteDialog(HWND hwndParent);

/**
 * @brief Show notification message config dialog
 * @param hwndParent Parent handle
 * 
 * @details Title/body text, placeholder support, empty uses defaults
 */
void ShowNotificationMessagesDialog(HWND hwndParent);

/**
 * @brief Show notification display settings
 * @param hwndParent Parent handle
 * 
 * @details Duration, position, animation, opacity, topmost
 */
void ShowNotificationDisplayDialog(HWND hwndParent);

/**
 * @brief Show comprehensive notification settings
 * @param hwndParent Parent handle
 * 
 * @details
 * Sound file picker, volume slider, preview, pause/resume, links to
 * display/message settings. Real-time volume adjustment and audio preview.
 */
void ShowNotificationSettingsDialog(HWND hwndParent);

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * @brief Center dialog on primary monitor (call in WM_INITDIALOG)
 * @param hwndDlg Dialog handle
 * 
 * @details
 * Uses work area (excludes taskbar), adjusts if off-screen, preserves size.
 * Safe for multi-monitor.
 */
void MoveDialogToPrimaryScreen(HWND hwndDlg);

/* ============================================================================
 * Global state
 * ============================================================================ */

/** 
 * @brief Active input dialog handle
 * 
 * @details
 * For IsDialogMessage(), preventing duplicates, keyboard routing.
 * NULL when closed.
 */
extern HWND g_hwndInputDialog;

#endif