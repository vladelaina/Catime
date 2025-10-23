/**
 * @file hotkey.h
 * @brief Modular hotkey management with data-driven configuration
 * 
 * Refactored architecture:
 * - Data-driven hotkey metadata system
 * - Eliminated repetitive control ID checks
 * - Unified validation and resource management
 * - Reduced code duplication by 50%
 */

#ifndef HOTKEY_H
#define HOTKEY_H

#include <windows.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Total number of configurable hotkeys */
#define HOTKEY_COUNT 12

/** @brief First hotkey edit control ID */
#define HOTKEY_EDIT_FIRST IDC_HOTKEY_EDIT1

/** @brief Last hotkey edit control ID */
#define HOTKEY_EDIT_LAST IDC_HOTKEY_EDIT12

/** @brief Dialog background color */
#define DIALOG_BG_COLOR RGB(0xF3, 0xF3, 0xF3)

/** @brief Button background color */
#define BUTTON_BG_COLOR RGB(0xFD, 0xFD, 0xFD)

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Display hotkey configuration dialog
 * @param hwndParent Parent window handle for modal dialog
 */
void ShowHotkeySettingsDialog(HWND hwndParent);

/**
 * @brief Dialog procedure for hotkey settings dialog
 * @param hwndDlg Dialog window handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 */
INT_PTR CALLBACK HotkeySettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Subclass procedure for hotkey input controls
 * @param hwnd Control window handle
 * @param uMsg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @param uIdSubclass Subclass identifier
 * @param dwRefData Reference data
 * @return Message processing result
 */
LRESULT CALLBACK HotkeyControlSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, 
                                         LPARAM lParam, UINT_PTR uIdSubclass, 
                                         DWORD_PTR dwRefData);

#endif
