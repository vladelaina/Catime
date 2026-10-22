/**
 * @file render.h
 * @brief Cairo/Pango text rendering (solid color or gradient) for the timer.
 */
#ifndef CATIME_LINUX_RENDER_H
#define CATIME_LINUX_RENDER_H

#include <cairo.h>

/** (Re)load font + color from config. Call after config changes. */
void render_load(void);

/** Reference size for the current font/scale (stable across digit changes). */
void render_get_size(int *w, int *h);

/** Draw @p text centered in the given @p w x @p h region onto @p cr. */
void render_draw(cairo_t *cr, int w, int h, const char *text);

#endif /* CATIME_LINUX_RENDER_H */
