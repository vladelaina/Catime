/**
 * @file window_events.h
 * @brief Window lifecycle event handlers
 * 
 * Ordered cleanup prevents resource leaks and access violations.
 * Reset operation recovers from inaccessible window states (off-screen, topmost conflicts).
 */

#ifndef WINDOW_EVENTS_H
#define WINDOW_EVENTS_H

#include <windows.h>

/**
 * @brief Handle window creation
 * @return TRUE (always)
 * 
 * @details
 * Setup: parent enablement, position/size, click-through, topmost.
 * Logs all steps for diagnostics.
 */
BOOL HandleWindowCreate(HWND hwnd);

/**
 * @brief Handle window destruction
 * 
 * @details Ordered cleanup:
 * 1. Save settings
 * 2. Stop timers (prevent callbacks)
 * 3. Remove UI (tray, animations)
 * 4. Release resources (fonts, threads)
 * 5. Post WM_QUIT
 * 
 * Order prevents leaks, access violations, unsaved config.
 * 
 * @warning Posts WM_QUIT (terminates application)
 */
void HandleWindowDestroy(HWND hwnd);

/**
 * @brief Reset to default state (recovery operation)
 * 
 * @details
 * Forces topmost enabled, persists to config, ensures visible.
 * Recovers from: multi-monitor changes, off-screen, topmost conflicts.
 * 
 * @warning Destructive (overwrites user preference)
 */
void HandleWindowReset(HWND hwnd);

/**
 * @brief Handle resize via mouse wheel
 * @param delta Wheel delta (positive=zoom in)
 * @return TRUE if handled
 * 
 * @details Delegates to drag_scale module
 */
BOOL HandleWindowResize(HWND hwnd, int delta);

/**
 * @brief Handle movement via drag
 * @return TRUE if handled
 * 
 * @details Delegates to drag_scale module
 */
BOOL HandleWindowMove(HWND hwnd);

#endif // WINDOW_EVENTS_H
