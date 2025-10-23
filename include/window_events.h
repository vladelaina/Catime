/**
 * @file window_events.h
 * @brief Window lifecycle and state management event handlers
 * @version 2.0 - Enhanced with comprehensive logging and error handling
 * 
 * Provides centralized event handling for window operations:
 * - Creation and initialization with parent window management
 * - Destruction with ordered resource cleanup
 * - State reset for recovery scenarios
 * - Resize operations via mouse wheel scaling
 * - Movement operations via drag functionality
 * 
 * All functions include detailed logging for debugging and diagnostics.
 */

#ifndef WINDOW_EVENTS_H
#define WINDOW_EVENTS_H

#include <windows.h>

/**
 * @brief Handle window creation and initialization
 * @param hwnd Window handle of the newly created window
 * @return TRUE if initialization succeeded, FALSE on error
 * 
 * Performs complete window setup including:
 * - Parent window enablement (for child windows)
 * - Position and size adjustment
 * - Click-through behavior configuration
 * - Topmost setting application
 * 
 * Logs all initialization steps for diagnostics.
 * 
 * @note Always returns TRUE in current implementation
 * @see AdjustWindowPosition, SetClickThrough, SetWindowTopmost
 */
BOOL HandleWindowCreate(HWND hwnd);

/**
 * @brief Handle window destruction with comprehensive cleanup
 * @param hwnd Window handle being destroyed
 * 
 * Executes ordered cleanup sequence:
 * 1. Save window settings to configuration
 * 2. Stop all timers to prevent callbacks
 * 3. Remove UI elements (tray icon, animations)
 * 4. Release resources (fonts, update threads)
 * 5. Post quit message to exit application
 * 
 * Cleanup order is critical to prevent:
 * - Resource leaks
 * - Access violations
 * - Unsaved configuration loss
 * 
 * Validates each cleanup step and logs warnings on failure.
 * 
 * @warning This function posts WM_QUIT, terminating the application
 * @see SaveWindowSettings, KillTimer, RemoveTrayIcon, PostQuitMessage
 */
void HandleWindowDestroy(HWND hwnd);

/**
 * @brief Reset window to default state and ensure visibility
 * @param hwnd Window handle to reset
 * 
 * Recovery operation that:
 * - Forces topmost behavior to enabled
 * - Persists setting to configuration file
 * - Ensures window is visible (not hidden/minimized)
 * 
 * Useful for scenarios where window becomes inaccessible due to:
 * - Multi-monitor configuration changes
 * - Window positioned off-screen
 * - Topmost setting conflicts
 * 
 * @note This is a destructive operation (overwrites user preference)
 * @see SetWindowTopmost, WriteConfigTopmost, ShowWindow
 */
void HandleWindowReset(HWND hwnd);

/**
 * @brief Handle window resize events via mouse wheel
 * @param hwnd Window handle to resize
 * @param delta Mouse wheel delta value (positive=zoom in, negative=zoom out)
 * @return TRUE if resize was handled successfully, FALSE otherwise
 * 
 * Delegates to scaling system for proportional size adjustment.
 * Delta magnitude determines scaling speed.
 * 
 * @note Actual scaling logic is in drag_scale module
 * @see HandleScaleWindow
 */
BOOL HandleWindowResize(HWND hwnd, int delta);

/**
 * @brief Handle window movement via drag operations
 * @param hwnd Window handle being moved
 * @return TRUE if movement was handled successfully, FALSE otherwise
 * 
 * Delegates to drag system for smooth repositioning.
 * Typically called during WM_LBUTTONDOWN processing.
 * 
 * @note Actual drag logic is in drag_scale module
 * @see HandleDragWindow
 */
BOOL HandleWindowMove(HWND hwnd);

#endif // WINDOW_EVENTS_H
