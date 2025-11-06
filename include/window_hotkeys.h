/**
 * @file window_hotkeys.h
 * @brief Global hotkey registration and dispatch system
 */

#ifndef WINDOW_HOTKEYS_H
#define WINDOW_HOTKEYS_H

#include <windows.h>

/* ============================================================================
 * Hotkey Registration
 * ============================================================================ */

/**
 * @brief Load and register hotkeys (auto-clears conflicts in INI)
 */
BOOL RegisterGlobalHotkeys(HWND hwnd);

/**
 * @brief Unregister all hotkeys
 */
void UnregisterGlobalHotkeys(HWND hwnd);

/**
 * @brief Route hotkey to handler
 * @return TRUE if handled, FALSE otherwise
 */
BOOL DispatchHotkey(HWND hwnd, int hotkeyId);

#endif /* WINDOW_HOTKEYS_H */

