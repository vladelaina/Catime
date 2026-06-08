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
 * Start, stop, or retune the render animation timer from the shared render state.
 *
 * @param hwnd Window handle that owns TIMER_ID_RENDER_ANIMATION
 * @param hasRenderableContent TRUE when the current frame has visible text/images
 * @param hasColorTagGradient TRUE when the current markdown frame needs gradient animation
 * @return TRUE if the timer is active after the update
 */
BOOL UpdateDrawingRenderAnimationTimer(HWND hwnd,
                                       BOOL hasRenderableContent,
                                       BOOL hasColorTagGradient);

/**
 * Stop the render animation timer and clear cached timer state.
 */
void StopDrawingRenderAnimationTimer(HWND hwnd);

/**
 * Release cached markdown render data used across paint calls.
 */
void CleanupDrawingRenderCache(void);

#endif /* DRAWING_RENDER_H */
