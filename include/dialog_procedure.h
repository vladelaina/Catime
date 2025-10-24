/**
 * @file dialog_procedure.h
 * @brief Comprehensive dialog system with multi-language support
 * @version 2.0 - Refactored for better maintainability
 * 
 * Provides window procedures and management functions for all application dialogs:
 * - Timer input dialogs (countdown, stopwatch, Pomodoro)
 * - Configuration dialogs (notifications, display, hotkeys)
 * - Information dialogs (about, help, licenses)
 * - Settings dialogs (website, loop count, combinations)
 * 
 * Features:
 * - Full multi-language support via language.h
 * - Flexible time input parsing (duration, absolute time, units)
 * - Live preview and validation
 * - Automatic screen centering on primary monitor
 * - Keyboard shortcuts (Ctrl+A, Enter to submit)
 * - Rich text rendering with markdown support
 */

#ifndef DIALOG_PROCEDURE_H
#define DIALOG_PROCEDURE_H

#include <windows.h>

/* ============================================================================
 * Timer input dialogs
 * ============================================================================ */

/**
 * @brief General dialog procedure for timer input dialogs
 * @param hwndDlg Dialog window handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 * 
 * @details Handles input for:
 * - Countdown timer
 * - Stopwatch (count up)
 * - Pomodoro timer
 * 
 * Features:
 * - Flexible time parsing (see ParseTimeInput)
 * - Ctrl+A select all support
 * - Enter key submits dialog
 * - Empty input cancels dialog
 * - Auto-focuses edit control on init
 * - Validates input and shows error dialog
 * - Centers on primary screen
 * 
 * @note Uses global inputText buffer for result
 */
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Parse flexible time input string to seconds
 * @param input Input time string (UTF-8)
 * @param seconds Output buffer for parsed seconds
 * @return TRUE if parsing successful, FALSE otherwise
 * 
 * @details Supported formats:
 * 1. Plain number: "25" = 25 minutes
 * 2. Duration units: "1h 30m", "90s", "1h 30m 15s"
 * 3. Compact duration: "130 45" = 1:30:45
 * 4. Absolute time: "14:30t" = countdown to 2:30 PM
 * 5. Compact target: "130t" = countdown to 1:30
 * 
 * Duration unit aliases:
 * - Hours: h, H, hr, hour, hours, 时, 小时
 * - Minutes: m, M, min, minute, minutes, 分, 分钟
 * - Seconds: s, S, sec, second, seconds, 秒
 * 
 * @note Returns FALSE for negative or invalid times
 * @note Maximum supported time: 99:59:59 (359999 seconds)
 */
BOOL ParseTimeInput(const char* input, int* seconds);

/* ============================================================================
 * Information dialogs
 * ============================================================================ */

/**
 * @brief Display application about/info dialog
 * @param hwndParent Parent window handle
 * 
 * @details Shows:
 * - Application name and version
 * - Author and copyright info
 * - GitHub repository link (clickable)
 * - License information
 * - Rich text with markdown formatting
 * 
 * @note Non-modal dialog, can coexist with main window
 * @see AboutDlgProc
 */
void ShowAboutDialog(HWND hwndParent);

/**
 * @brief Dialog procedure for about dialog with rich text support
 * @param hwndDlg Dialog window handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 * 
 * @details
 * - Renders markdown-formatted text via RichEdit control
 * - Clickable hyperlinks (opens in default browser)
 * - Auto-sizes to fit content
 * - Dismisses on any key press or mouse click outside
 * - Multi-language support
 */
INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Display error dialog for invalid input
 * @param hwndParent Parent window handle
 * 
 * @details
 * - Shows localized error message
 * - Modal dialog (blocks parent)
 * - Single OK button to dismiss
 * - Centers on primary screen
 * - Typically called after validation failure
 */
void ShowErrorDialog(HWND hwndParent);

/**
 * @brief Show font license agreement dialog
 * @param hwndParent Parent window handle
 * @return Dialog result (IDOK if agreed, IDCANCEL if declined)
 * 
 * @details
 * - Displays license terms for bundled fonts (MIT, OFL, SIL)
 * - Rich text with markdown formatting
 * - Scrollable for long license texts
 * - User must accept before using fonts
 * - Returns IDOK on accept, IDCANCEL on decline
 * 
 * @note Modal dialog, blocks until user responds
 */
INT_PTR ShowFontLicenseDialog(HWND hwndParent);

/* ============================================================================
 * Pomodoro configuration dialogs
 * ============================================================================ */

/**
 * @brief Display Pomodoro loop count configuration dialog
 * @param hwndParent Parent window handle
 * 
 * @details
 * - Configure number of Pomodoro loops (1-99)
 * - Shows current loop count
 * - Validates input (numeric, positive)
 * - Saves to config on OK
 * - Empty input cancels without saving
 */
void ShowPomodoroLoopDialog(HWND hwndParent);

/**
 * @brief Dialog procedure for Pomodoro loop dialog
 * @param hwndDlg Dialog window handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 */
INT_PTR CALLBACK PomodoroLoopDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Display Pomodoro time combination settings dialog
 * @param hwndParent Parent window handle
 * 
 * @details Configure durations for:
 * - Work session (focus time)
 * - Short break (between sessions)
 * - Long break (after N loops)
 * 
 * Features:
 * - Three separate input fields
 * - Flexible time parsing (see ParseTimeInput)
 * - Live validation
 * - Shows current values on init
 * - Saves all three values atomically
 */
void ShowPomodoroComboDialog(HWND hwndParent);

/* ============================================================================
 * Settings dialogs
 * ============================================================================ */

/**
 * @brief Display website/URL configuration dialog
 * @param hwndParent Parent window handle
 * 
 * @details
 * - Configure notification click action URL
 * - Supports any valid URL or file path
 * - No validation (allows flexible use)
 * - Empty input clears URL (disables click action)
 * - Saves to config immediately
 */
void ShowWebsiteDialog(HWND hwndParent);

/**
 * @brief Display notification message text configuration dialog
 * @param hwndParent Parent window handle
 * 
 * @details Configure:
 * - Notification title text
 * - Notification body text
 * - Supports multi-language placeholders
 * - Empty fields use default messages
 * - Live preview (if implemented)
 * - Saves to config on OK
 */
void ShowNotificationMessagesDialog(HWND hwndParent);

/**
 * @brief Display notification display/appearance settings dialog
 * @param hwndParent Parent window handle
 * 
 * @details Configure:
 * - Display duration (milliseconds)
 * - Window position (screen corner)
 * - Animation style
 * - Opacity/transparency
 * - Always on top setting
 * 
 * @note Complex dialog with multiple controls
 */
void ShowNotificationDisplayDialog(HWND hwndParent);

/**
 * @brief Display comprehensive notification settings dialog
 * @param hwndParent Parent window handle
 * 
 * @details Unified settings dialog for:
 * - Sound file selection (file picker)
 * - Volume control (0-100 slider)
 * - Loop/repeat settings
 * - Preview button to test sound
 * - Pause/resume buttons
 * - Display settings link
 * - Message settings link
 * 
 * Features:
 * - File browser for sound selection
 * - Real-time volume adjustment
 * - Live audio preview
 * - System beep option
 * - Saves on OK or Apply
 */
void ShowNotificationSettingsDialog(HWND hwndParent);

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * @brief Move dialog to center of primary monitor
 * @param hwndDlg Dialog window handle
 * 
 * @details
 * - Gets primary monitor work area (excluding taskbar)
 * - Centers dialog horizontally and vertically
 * - Adjusts if dialog would be off-screen
 * - Safe for multi-monitor setups
 * - Non-intrusive (preserves dialog size)
 * 
 * @note Should be called in WM_INITDIALOG handler
 */
void MoveDialogToPrimaryScreen(HWND hwndDlg);

/* ============================================================================
 * Global state
 * ============================================================================ */

/** 
 * @brief Global handle to currently active input dialog
 * 
 * Used for:
 * - IsDialogMessage processing in main message loop
 * - Preventing multiple dialog instances
 * - Keyboard shortcut routing
 * 
 * @note Set to NULL when dialog closes
 */
extern HWND g_hwndInputDialog;

#endif