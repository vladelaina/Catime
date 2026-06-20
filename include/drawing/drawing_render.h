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
void HandleWindowPaint(HWND hwnd, const PAINTSTRUCT* ps);

/**
 * Stop the render animation timer and clear cached timer state.
 *
 * Use this for lifecycle or render-failure cleanup. Ordinary display setting
 * changes should request a repaint and let the renderer decide timer state from
 * the next real frame.
 */
void StopDrawingRenderAnimationTimer(HWND hwnd);

/**
 * Release cached markdown render data used across paint calls.
 */
void CleanupDrawingRenderCache(void);

#endif /* DRAWING_RENDER_H */
