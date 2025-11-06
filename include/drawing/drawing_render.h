/**
 * @file drawing_render.h
 * @brief GDI rendering pipeline with double-buffering
 */

#ifndef DRAWING_RENDER_H
#define DRAWING_RENDER_H

#include <windows.h>

/**
 * Main paint handler for WM_PAINT
 * @param hwnd Window handle
 * @param ps Paint structure from BeginPaint
 * @note Double-buffering eliminates flicker
 * @note Window auto-resizes to fit text
 */
void HandleWindowPaint(HWND hwnd, PAINTSTRUCT* ps);

#endif /* DRAWING_RENDER_H */

