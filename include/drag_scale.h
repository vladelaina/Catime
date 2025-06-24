/**
 * @file drag_scale.h
 * @brief Window dragging and scaling functionality interface
 * 
 * This file defines the application window's dragging and scaling functionality interfaces,
 * including mouse dragging of windows and mouse wheel scaling of windows.
 */

#ifndef DRAG_SCALE_H
#define DRAG_SCALE_H

#include <windows.h>

// Record topmost state before edit mode
extern BOOL PREVIOUS_TOPMOST_STATE;

/**
 * @brief Handle window dragging events
 * @param hwnd Window handle
 * @return BOOL Whether the event was handled
 * 
 * In edit mode, handle mouse dragging window events.
 * Update window position based on mouse movement distance.
 */
BOOL HandleDragWindow(HWND hwnd);

/**
 * @brief Handle window scaling events
 * @param hwnd Window handle
 * @param delta Mouse wheel increment
 * @return BOOL Whether the event was handled
 * 
 * In edit mode, handle mouse wheel window scaling events.
 * Adjust window and font size based on wheel direction.
 */
BOOL HandleScaleWindow(HWND hwnd, int delta);

/**
 * @brief Start dragging window
 * @param hwnd Window handle
 * 
 * In edit mode, start window dragging operation.
 * Record initial mouse position and set capture.
 */
void StartDragWindow(HWND hwnd);

/**
 * @brief End dragging window
 * @param hwnd Window handle
 * 
 * End window dragging operation.
 * Release mouse capture and adjust window position.
 */
void EndDragWindow(HWND hwnd);

/**
 * @brief Start edit mode
 * @param hwnd Window handle
 * 
 * Before enabling edit mode, ensure the window is in topmost state,
 * record original topmost state for restoration when exiting edit mode.
 */
void StartEditMode(HWND hwnd);

/**
 * @brief End edit mode
 * @param hwnd Window handle
 * 
 * Exit edit mode, restore window's original topmost state,
 * clear blur effect and update related settings.
 */
void EndEditMode(HWND hwnd);

#endif // DRAG_SCALE_H