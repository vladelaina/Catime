/**
 * @file hotkey.h
 * @brief Global hotkey configuration and management
 * 
 * Functions for hotkey settings dialog and hotkey input control handling
 */

#ifndef HOTKEY_H
#define HOTKEY_H

#include <windows.h>

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