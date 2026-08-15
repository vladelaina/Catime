/**
 * @file window_hotkeys.h
 * @brief Global hotkey registration and dispatch system
 */

#ifndef WINDOW_HOTKEYS_H
#define WINDOW_HOTKEYS_H

#include <windows.h>

#define HOTKEY_REGISTRATION_RETRY_TIMER_ID 42433u

/* ============================================================================
 * Hotkey Registration
 * ============================================================================ */

/**
 * @brief Load and register configured global hotkeys
 *
 * Hotkeys already owned by another Catime process are left in the
 * configuration so launching a second instance cannot erase the first
 * instance's working shortcuts.
 */
BOOL RegisterGlobalHotkeys(HWND hwnd);

/**
 * @brief Unregister all hotkeys
 */
void UnregisterGlobalHotkeys(HWND hwnd);

/** Unregister hotkeys and release this process's cross-process ownership. */
void ShutdownGlobalHotkeys(HWND hwnd);

/**
 * @brief Route hotkey to handler
 * @return TRUE if handled, FALSE otherwise
 */
BOOL DispatchHotkey(HWND hwnd, int hotkeyId);

#endif /* WINDOW_HOTKEYS_H */
