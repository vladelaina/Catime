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
 * @param topmost TRUE for topmost, FALSE for normal (anchored to desktop shell)
 * 
 * @details
 * Topmost: Above all non-topmost windows
 * Normal: Anchored to WorkerW/Progman (resists Win+D minimize-all)
 * 
 * Persists to configuration.
 */
void SetWindowTopmost(HWND hwnd, BOOL topmost);

/**
 * @brief Set topmost behavior without persisting configuration
 * @param hwnd Window handle
 * @param topmost TRUE for topmost, FALSE for normal desktop-anchor mode
 *
 * @details
 * Updates runtime state and applies z-order/owner policy, but does not
 * write CLOCK_WINDOW_TOPMOST to config. Intended for transient behavior
 * (e.g. edit-mode override) and hot-reload application.
 */
void SetWindowTopmostTransient(HWND hwnd, BOOL topmost);

/**
 * @brief Re-apply current topmost state without writing configuration
 * @param hwnd Window handle
 *
 * @details Uses current runtime value of CLOCK_WINDOW_TOPMOST.
 * Intended for startup/visibility recovery/retry paths where state
 * should be restored but not persisted again.
 */
void RefreshWindowTopmostState(HWND hwnd);

/**
 * @brief Ensure window is visible and re-apply current topmost/normal mode
 * @param hwnd Window handle
 *
 * @details Uses SW_SHOWNOACTIVATE and current CLOCK_WINDOW_TOPMOST.
 * Useful for recovery paths after shell/display/window state transitions.
 */
void EnsureWindowVisibleWithTopmostState(HWND hwnd);

/**
 * @brief Handle system minimize command under topmost policy
 * @param hwnd Window handle
 * @param sysCommand WM_SYSCOMMAND command value (masked or raw)
 * @return TRUE if command was consumed (minimize blocked), FALSE otherwise
 */
BOOL HandleTopmostMinimizeCommand(HWND hwnd, UINT sysCommand);

/**
 * @brief Handle WM_SIZE state changes under topmost policy
 * @param hwnd Window handle
 * @param sizeType WM_SIZE wParam
 * @return TRUE if event was consumed, FALSE otherwise
 */
BOOL HandleTopmostSizeEvent(HWND hwnd, WPARAM sizeType);

/**
 * @brief Attach to desktop wallpaper level
 * @param hwnd Window handle
 * 
 * @details Sets owner to WorkerW/Progman desktop anchor
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

