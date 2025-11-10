/**
 * @file dialog_procedure.h
 * @brief Dialog procedures for timer input, settings, and info dialogs
 * 
 * **REFACTORED HEADER - Now delegates to modular components**
 * 
 * This file maintains backward compatibility by including all
 * refactored dialog modules:
 * - dialog_common.h    : Shared utilities (context, subclassing, positioning)
 * - dialog_error.h     : Error dialogs
 * - dialog_input.h     : Generic input dialogs (timer, pomodoro, etc.)
 * - dialog_info.h      : About, font license, website, CLI help
 * - dialog_pomodoro.h  : Pomodoro-specific dialogs
 * - dialog_notification.h : Notification configuration
 */

#ifndef DIALOG_PROCEDURE_H
#define DIALOG_PROCEDURE_H

#include <windows.h>

/* Include all refactored dialog modules */
#include "dialog_common.h"
#include "dialog_error.h"
#include "dialog_input.h"
#include "dialog_info.h"
#include "dialog_pomodoro.h"
#include "dialog_notification.h"

/* ============================================================================
 * Backward Compatibility Aliases
 * ============================================================================ */

/**
 * @note All functions are now declared in their respective module headers.
 *       This file only provides documentation and includes.
 *
 *       For implementation details, see:
 *       - dialog_input.h for DlgProc, g_hwndInputDialog, g_pomodoroSelectedIndex
 *       - dialog_info.h for ShowAboutDialog, AboutDlgProc, ShowFontLicenseDialog
 *       - dialog_error.h for ShowErrorDialog
 *       - dialog_pomodoro.h for ShowPomodoroLoopDialog, ShowPomodoroComboDialog
 *       - dialog_notification.h for notification dialogs
 *       - dialog_common.h for utility functions like dialog centering
 *       - utils/time_parser.h for time parsing functions
 */

/* ============================================================================
 * Legacy Compatibility - MoveDialogToPrimaryScreen
 * ============================================================================ */

/**
 * @brief Backward compatibility alias for Dialog_CenterOnPrimaryScreen
 * @param hwndDlg Dialog handle
 * 
 * @details This function is deprecated. Use Dialog_CenterOnPrimaryScreen() instead.
 * @note Implemented in dialog_common.c for compatibility
 */
void MoveDialogToPrimaryScreen(HWND hwndDlg);

#endif