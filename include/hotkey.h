/**
 * @file hotkey.h
 * @brief Data-driven hotkey management system
 * 
 * Metadata-driven design eliminates repetitive control ID checks (50% code reduction).
 * Unified validation prevents duplicate hotkey assignments across all controls.
 */

#ifndef HOTKEY_H
#define HOTKEY_H

#include <windows.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define HOTKEY_COUNT 12
#define HOTKEY_EDIT_FIRST IDC_HOTKEY_EDIT1
#define HOTKEY_EDIT_LAST IDC_HOTKEY_EDIT12

#define DIALOG_BG_COLOR RGB(0xF3, 0xF3, 0xF3)
#define BUTTON_BG_COLOR RGB(0xFD, 0xFD, 0xFD)

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Show hotkey config dialog (modal)
 * @param hwndParent Parent handle
 */
void ShowHotkeySettingsDialog(HWND hwndParent);

/**
 * @brief Hotkey settings dialog procedure
 */
INT_PTR CALLBACK HotkeySettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Hotkey control subclass procedure
 * 
 * @details Validates input and prevents duplicate assignments
 */
LRESULT CALLBACK HotkeyControlSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, 
                                         LPARAM lParam, UINT_PTR uIdSubclass, 
                                         DWORD_PTR dwRefData);

#endif
