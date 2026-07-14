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
 * @return TRUE when the requested state was fully applied, FALSE if it was
 *         only recorded and scheduled for retry.
 */
BOOL SetWindowTopmost(HWND hwnd, BOOL topmost);

/**
 * @brief Apply topmost preference loaded from configuration
 * @param hwnd Window handle
 * @param topmost TRUE for topmost, FALSE for normal desktop-anchor mode
 * @return TRUE when fully applied, FALSE if scheduled for retry
 *
 * @details Updates the user preference and runtime target without writing the
 * same value back to configuration. Intended for config hot-reload.
 */
BOOL SetWindowTopmostFromConfig(HWND hwnd, BOOL topmost);

/**
 * @brief Set topmost behavior without persisting configuration
 * @param hwnd Window handle
 * @param topmost TRUE for topmost, FALSE for normal desktop-anchor mode
 *
 * @details
 * Updates only the runtime target and applies z-order/owner policy. Does not
 * write CLOCK_WINDOW_TOPMOST to config and does not change the saved user
 * preference. Intended for transient behavior (e.g. edit-mode override).
 * @return TRUE when fully applied, FALSE if scheduled for retry
 */
BOOL SetWindowTopmostTransient(HWND hwnd, BOOL topmost);

/**
 * @brief Re-apply current topmost state without writing configuration
 * @param hwnd Window handle
 *
 * @details Uses current runtime topmost target.
 * Intended for startup/visibility recovery/retry paths where state
 * should be restored but not persisted again.
 * @return TRUE when fully applied, FALSE otherwise
 */
BOOL RefreshWindowTopmostState(HWND hwnd);

/**
 * @brief Ensure window is visible and re-apply current topmost/normal mode
 * @param hwnd Window handle
 *
 * @details Uses SW_SHOWNOACTIVATE and current runtime topmost target.
 * Useful for recovery paths after shell/display/window state transitions.
 */
void EnsureWindowVisibleWithTopmostState(HWND hwnd);

/**
 * @brief Hide the main window as an intentional application action
 * @param hwnd Window handle
 *
 * @details Marks the hidden state as deliberate so external visibility
 * recovery does not immediately undo tray/startup/preview hides.
 */
void HideWindowIntentionally(HWND hwnd);

/**
 * @brief React to external visibility changes for topmost mode
 * @param hwnd Window handle
 * @param pwp WM_WINDOWPOSCHANGED payload, or NULL for timer retry
 * @return TRUE if handled, FALSE otherwise
 */
BOOL HandleTopmostVisibilityChange(HWND hwnd, const WINDOWPOS* pwp);

/**
 * @brief Schedule recovery after a hide notification without WINDOWPOS data
 * @param hwnd Window handle
 * @return TRUE if a restore retry was scheduled, FALSE otherwise
 */
BOOL HandleTopmostHiddenEvent(HWND hwnd);

/**
 * @brief Mark the main window as visible after a show notification
 * @param hwnd Window handle
 */
void HandleTopmostShownEvent(HWND hwnd);

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
 * @details Re-applies topmost only when needed:
 * - Window overlaps taskbar area, or
 * - Window unexpectedly lost WS_EX_TOPMOST.
 * Called by timer ticks; avoids unconditional periodic topmost forcing.
 */
BOOL EnforceTopmostOverTaskbar(HWND hwnd);

/** Locate the taskbar owned by a specific monitor, including secondary bars. */
BOOL GetTaskbarRectForMonitor(HMONITOR monitor, RECT* outRect);

/**
 * @brief Retry the current runtime topmost target after an earlier failure
 * @param hwnd Window handle
 * @return TRUE if the timer event was handled
 */
BOOL HandleTopmostApplyRetry(HWND hwnd);

/**
 * @brief Clear desktop-integration retry timers and internal retry state
 * @param hwnd Window handle being torn down, or NULL to reset state only
 */
void CleanupWindowDesktopIntegrationState(HWND hwnd);

#endif /* WINDOW_DESKTOP_INTEGRATION_H */

