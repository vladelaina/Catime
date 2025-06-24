/**
 * @file drawing.h
 * @brief Window drawing functionality interface
 * 
 * This file defines the application window's drawing-related function interfaces,
 * including text rendering, color settings, and window content drawing functionality.
 */

#ifndef DRAWING_H
#define DRAWING_H

#include <windows.h>

/**
 * @brief Handle window painting
 * @param hwnd Window handle
 * @param ps Paint structure
 * 
 * Handle window's WM_PAINT message, performing the following operations:
 * 1. Create memory DC double-buffering to prevent flickering
 * 2. Calculate remaining time/get current time based on mode
 * 3. Dynamically load font resources (supporting real-time preview)
 * 4. Parse color configuration (supporting HEX/RGB formats)
 * 5. Draw text using double-buffering mechanism
 * 6. Automatically adjust window size to fit text content
 */
void HandleWindowPaint(HWND hwnd, PAINTSTRUCT *ps);

#endif // DRAWING_H