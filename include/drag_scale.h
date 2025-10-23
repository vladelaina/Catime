/**
 * @file drag_scale.h
 * @brief Window dragging, scaling and edit mode functionality
 * 
 * Provides interactive window manipulation features including:
 * - Drag-to-reposition with mouse capture
 * - Mouse wheel scaling with center-point preservation
 * - Edit mode state management with visual feedback
 * - Automatic configuration persistence with debouncing
 * 
 * @version 2.0 - Refactored for improved maintainability
 */

#ifndef DRAG_SCALE_H
#define DRAG_SCALE_H

#include <windows.h>

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/** @brief Scale factor multiplier per mouse wheel notch (10% increment) */
#define SCALE_FACTOR_STEP 1.1f

/** @brief Delay in milliseconds before saving config to disk (debouncing) */
#define CONFIG_SAVE_DELAY_MS 500

/** @brief Timer ID for delayed configuration save (unique ID to avoid conflicts with system timers) */
#define TIMER_ID_CONFIG_SAVE 1005

/** 
 * @brief Timer ID for window refresh after exiting edit mode
 * Uses ID 2001 to avoid conflicts with other timer IDs (1001-1009 range is heavily used)
 */
#define TIMER_ID_EDIT_MODE_REFRESH 2001

/** @brief Interval for refresh timer after exiting edit mode */
#define TIMER_REFRESH_INTERVAL_MS 150

/* ============================================================================
 * Global State Variables
 * ============================================================================ */

/** @brief Previous topmost window state before entering edit mode */
extern BOOL PREVIOUS_TOPMOST_STATE;

/* ============================================================================
 * Core Edit Mode Functions
 * ============================================================================ */

/**
 * @brief Enter edit mode for interactive window manipulation
 * @param hwnd Window handle to enable edit mode for
 * 
 * Actions performed:
 * - Saves current topmost state for restoration
 * - Ensures window is topmost during editing
 * - Enables blur-behind visual effect
 * - Disables click-through behavior
 * - Updates cursor and refreshes display
 */
void StartEditMode(HWND hwnd);

/**
 * @brief Exit edit mode and restore normal window behavior
 * @param hwnd Window handle to disable edit mode for
 * 
 * Actions performed:
 * - Removes blur effect and restores transparency
 * - Re-enables click-through behavior
 * - Restores original topmost state
 * - Schedules configuration save
 */
void EndEditMode(HWND hwnd);

/* ============================================================================
 * Window Dragging Functions
 * ============================================================================ */

/**
 * @brief Initialize window dragging operation with mouse capture
 * @param hwnd Window handle to start dragging
 * 
 * Sets dragging state and captures mouse input for smooth movement tracking.
 */
void StartDragWindow(HWND hwnd);

/**
 * @brief Finalize window dragging and ensure valid position
 * @param hwnd Window handle to stop dragging
 * 
 * Releases mouse capture and validates window position within screen bounds.
 */
void EndDragWindow(HWND hwnd);

/**
 * @brief Process window dragging based on mouse movement delta
 * @param hwnd Window handle being dragged
 * @return TRUE if window was repositioned, FALSE if not in drag mode
 * 
 * Calculates movement delta from last mouse position and updates window
 * position accordingly. Schedules configuration save for persistence.
 */
BOOL HandleDragWindow(HWND hwnd);

/* ============================================================================
 * Window Scaling Functions
 * ============================================================================ */

/**
 * @brief Handle window scaling via mouse wheel input
 * @param hwnd Window handle to scale
 * @param delta Mouse wheel delta value (positive = zoom in, negative = zoom out)
 * @return TRUE if scaling was applied, FALSE if not in edit mode or limits reached
 * 
 * Applies proportional scaling centered on current window position.
 * Scale factor is clamped between MIN_SCALE_FACTOR and MAX_SCALE_FACTOR.
 * Automatically schedules configuration save after scaling.
 */
BOOL HandleScaleWindow(HWND hwnd, int delta);

/* ============================================================================
 * Utility Functions (Internal Use)
 * ============================================================================ */

/**
 * @brief Schedule delayed configuration save to reduce I/O frequency
 * @param hwnd Window handle for timer context
 * 
 * Implements debouncing: cancels previous timer and sets new one.
 * Prevents excessive disk writes during continuous drag/scale operations.
 */
void ScheduleConfigSave(HWND hwnd);

#endif