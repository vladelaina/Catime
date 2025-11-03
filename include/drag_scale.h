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

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/** @brief Scale per wheel notch (10% increment) */
#define SCALE_FACTOR_STEP 1.1f

/** @brief Config save debounce delay (prevents I/O spam) */
#define CONFIG_SAVE_DELAY_MS 500

/** @brief Timer ID for config save (avoids conflicts with system timers) */
#define TIMER_ID_CONFIG_SAVE 1005

/** @brief Timer ID for edit mode refresh (avoids 1001-1009 range) */
#define TIMER_ID_EDIT_MODE_REFRESH 2001

/** @brief Edit mode refresh interval */
#define TIMER_REFRESH_INTERVAL_MS 150

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
 * @brief End dragging and validate position
 * @param hwnd Window handle
 * 
 * @details Releases capture, validates position within screen bounds
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

/* ============================================================================
 * Window Scaling Functions
 * ============================================================================ */

/**
 * @brief Handle mouse wheel scaling
 * @param hwnd Window handle
 * @param delta Wheel delta (positive=zoom in, negative=zoom out)
 * @return TRUE if scaled, FALSE if not in edit mode or limits reached
 * 
 * @details
 * Proportional scaling, clamped to MIN/MAX_SCALE_FACTOR, schedules save.
 */
BOOL HandleScaleWindow(HWND hwnd, int delta);

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

#endif