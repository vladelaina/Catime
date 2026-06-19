/**
 * @file drag_scale.h
 * @brief Window dragging, scaling, and edit mode with config persistence
 * 
 * Mouse capture ensures smooth dragging even when cursor leaves window.
 * Config save debouncing (500ms) prevents excessive disk I/O during continuous operations.
 */

#ifndef DRAG_SCALE_H
#define DRAG_SCALE_H

#include <windows.h>
#include "../resource/resource.h"

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/* Scale steps are now dynamic via config */

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/** @brief Saved topmost state (for restoration after edit mode) */
extern BOOL PREVIOUS_TOPMOST_STATE;

/* ============================================================================
 * Core Edit Mode Functions
 * ============================================================================ */

/**
 * @brief Enter edit mode
 * @param hwnd Window handle
 * 
 * @details
 * Saves topmost state, forces topmost, enables blur, disables click-through.
 */
void StartEditMode(HWND hwnd);

/**
 * @brief Exit edit mode
 * @param hwnd Window handle
 * 
 * @details
 * Removes blur, re-enables click-through, restores topmost, schedules save.
 */
void EndEditMode(HWND hwnd);

/**
 * @brief Mark that user explicitly changed topmost state during edit mode
 *
 * @details Prevents EndEditMode from restoring stale pre-edit topmost state.
 */
void MarkEditModeTopmostOverride(void);

/* ============================================================================
 * Window Dragging Functions
 * ============================================================================ */

/**
 * @brief Start dragging with mouse capture
 * @param hwnd Window handle
 * 
 * @details Captures mouse for smooth tracking even when cursor leaves window
 */
void StartDragWindow(HWND hwnd);

/**
 * @brief End dragging
 * @param hwnd Window handle
 * 
 * @details Releases capture and refreshes window
 */
void EndDragWindow(HWND hwnd);

/**
 * @brief Handle drag movement
 * @param hwnd Window handle
 * @return TRUE if repositioned, FALSE if not dragging
 * 
 * @details Calculates delta from last position, schedules config save
 */
BOOL HandleDragWindow(HWND hwnd);

/**
 * @brief Recover a drag when a left press was ignored during a short scale guard.
 * @param hwnd Window handle
 * @return TRUE if dragging is active after the attempt
 */
BOOL TryStartDragWindowFromMouseMove(HWND hwnd);

/* ============================================================================
 * Window Scaling Functions
 * ============================================================================ */

/**
 * @brief Handle mouse wheel scaling
 * @param hwnd Window handle
 * @param delta Wheel delta (positive=zoom in, negative=zoom out)
 * @return TRUE if scaled, FALSE if not in edit mode or minimum limit reached
 * 
 * @details
 * Proportional scaling, clamped only to MIN_SCALE_FACTOR, schedules save.
 */
BOOL HandleScaleWindow(HWND hwnd, int delta);

/**
 * @brief Get the pending mouse anchor for a paint-triggered resize.
 * @param hwnd Window handle
 * @param anchor Output screen-space cursor position
 * @return TRUE if an anchor is available for this window
 */
BOOL GetPendingScaleResizeAnchor(HWND hwnd, POINT* anchor);

/**
 * @brief Get the pending mouse anchor and stable relative ratios.
 * @param hwnd Window handle
 * @param anchor Output screen-space cursor position
 * @param ratioX Output horizontal anchor ratio in the pre-resize window
 * @param ratioY Output vertical anchor ratio in the pre-resize window
 * @return TRUE if anchor data is available for this window
 */
BOOL GetPendingScaleResizeAnchorInfo(HWND hwnd, POINT* anchor, double* ratioX, double* ratioY);

/**
 * @brief Check whether a wheel-scale gesture is currently being coalesced.
 * @param hwnd Window handle
 * @return TRUE while continuous wheel scaling is active for this window
 */
BOOL IsScaleWindowGestureActive(HWND hwnd);

/**
 * @brief Get the current wheel-scale gesture serial for this window.
 * @param hwnd Window handle
 * @return Non-zero serial while active, otherwise 0
 */
DWORD GetScaleWindowGestureSerial(HWND hwnd);

/**
 * @brief Clear any pending mouse anchor for a paint-triggered resize.
 * @param hwnd Window handle
 */
void ClearPendingScaleResizeAnchor(HWND hwnd);

/**
 * @brief Consume and clear a pending mouse anchor after a resize used it.
 * @param hwnd Window handle
 */
void ConsumePendingScaleResizeAnchor(HWND hwnd);

/* ============================================================================
 * Utility Functions (Internal Use)
 * ============================================================================ */

/**
 * @brief Schedule debounced config save
 * @param hwnd Window handle
 * 
 * @details
 * Cancels previous timer, sets new one. Prevents excessive I/O during
 * continuous drag/scale operations.
 */
void ScheduleConfigSave(HWND hwnd);

/**
 * @brief Cancel any pending debounced config save
 * @param hwnd Window handle used as fallback for timer cancellation
 */
void CancelScheduledConfigSave(HWND hwnd);

#endif
