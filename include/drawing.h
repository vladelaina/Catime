/**
 * @file drawing.h
 * @brief Window drawing and painting functions
 * 
 * Functions for handling window painting and graphics rendering
 */

#ifndef DRAWING_H
#define DRAWING_H

#include <windows.h>

/**
 * @brief Handle window paint events
 * @param hwnd Window handle to paint
 * @param ps Paint structure containing drawing context
 */
void HandleWindowPaint(HWND hwnd, PAINTSTRUCT *ps);

#endif