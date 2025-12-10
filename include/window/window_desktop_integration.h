/**
 * @file window_desktop_integration.h
 * @brief Desktop integration and Z-order management
 */

#ifndef WINDOW_DESKTOP_INTEGRATION_H
#define WINDOW_DESKTOP_INTEGRATION_H

#include <windows.h>

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Set always-on-top behavior
 * @param hwnd Window handle
 * @param topmost TRUE for topmost, FALSE for normal (parented to Progman)
 * 
 * @details
 * Topmost: Above all non-topmost windows
 * Normal: Parented to Progman (resists Win+D minimize-all)
 * 
 * Persists to configuration.
 */
void SetWindowTopmost(HWND hwnd, BOOL topmost);

/**
 * @brief Attach to desktop wallpaper level
 * @param hwnd Window handle
 * 
 * @details Parents to WorkerW (desktop worker)
 * @note Won't respond to Win+D or taskbar clicks
 */
void ReattachToDesktop(HWND hwnd);

/**
 * @brief Enforce topmost when window overlaps taskbar
 * @param hwnd Window handle
 * @return TRUE if window overlaps taskbar area, FALSE otherwise
 * 
 * @details If window is in topmost mode and overlaps with taskbar area,
 * re-apply topmost to ensure visibility. Called periodically by main timer.
 * Uses standard SetWindowPos API - safe and game-compatible.
 */
BOOL EnforceTopmostOverTaskbar(HWND hwnd);

#endif /* WINDOW_DESKTOP_INTEGRATION_H */

