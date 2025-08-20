/**
 * @file drag_scale.h
 * @brief Window dragging, scaling and edit mode functionality
 * 
 * Functions for interactive window positioning, resizing, and edit mode management
 */

#ifndef DRAG_SCALE_H
#define DRAG_SCALE_H

#include <windows.h>

/** @brief Previous topmost window state before edit mode */
extern BOOL PREVIOUS_TOPMOST_STATE;

/**
 * @brief Handle window dragging during mouse movement
 * @param hwnd Window handle being dragged
 * @return TRUE if drag was handled, FALSE otherwise
 */
BOOL HandleDragWindow(HWND hwnd);

/**
 * @brief Handle window scaling via mouse wheel
 * @param hwnd Window handle to scale
 * @param delta Mouse wheel delta value
 * @return TRUE if scaling was handled, FALSE otherwise
 */
BOOL HandleScaleWindow(HWND hwnd, int delta);

/**
 * @brief Start window drag operation
 * @param hwnd Window handle to begin dragging
 */
void StartDragWindow(HWND hwnd);

/**
 * @brief End window drag operation
 * @param hwnd Window handle to stop dragging
 */
void EndDragWindow(HWND hwnd);

/**
 * @brief Enter edit mode for window positioning
 * @param hwnd Window handle to make interactive
 */
void StartEditMode(HWND hwnd);

/**
 * @brief Exit edit mode and restore transparency
 * @param hwnd Window handle to make non-interactive
 */
void EndEditMode(HWND hwnd);

#endif