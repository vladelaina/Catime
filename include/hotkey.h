/**
 * @file hotkey.h
 * @brief Hotkey management interface
 * 
 * This file defines the application's hotkey management interface,
 * handling global hotkey setup, dialog interaction, and configuration saving.
 */

#ifndef HOTKEY_H
#define HOTKEY_H

#include <windows.h>

/**
 * @brief Show hotkey settings dialog
 * @param hwndParent Parent window handle
 * 
 * Display the hotkey settings dialog for setting global hotkeys.
 */
void ShowHotkeySettingsDialog(HWND hwndParent);

/**
 * @brief Hotkey settings dialog message handling procedure
 * @param hwndDlg Dialog handle
 * @param msg Message type
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return INT_PTR Message handling result
 * 
 * Handle all message events for the hotkey settings dialog, including initialization, background color, and button clicks.
 */
INT_PTR CALLBACK HotkeySettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Hotkey control subclass handling function
 * @param hwnd Hotkey control window handle
 * @param uMsg Message type
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @param uIdSubclass Subclass ID
 * @param dwRefData Reference data
 * @return LRESULT Message handling result
 * 
 * Handle hotkey control messages, especially intercepting Alt key and Alt+Shift combinations
 * to prevent Windows system from emitting beep sounds
 */
LRESULT CALLBACK HotkeyControlSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, 
                                         LPARAM lParam, UINT_PTR uIdSubclass, 
                                         DWORD_PTR dwRefData);

#endif // HOTKEY_H 