/**
 * @file window.h
 * @brief Window management - unified header for all window modules
 * 
 * This file aggregates all window-related functionality:
 * - Core window creation and lifecycle
 * - Visual effects (blur, click-through)
 * - Application initialization
 * - Desktop integration and Z-order
 */

#ifndef WINDOW_H
#define WINDOW_H

#include <limits.h>
#include <math.h>

/* Include all window module headers */
#include "window/window_core.h"
#include "window/window_visual_effects.h"
#include "window/window_initialization.h"
#include "window/window_desktop_integration.h"

/* ============================================================================
 * Scaling constraints
 * ============================================================================ */

#define MIN_SCALE_FACTOR 0.5f

static inline int ScaleWindowDimensionClamped(int baseDimension, float scaleFactor) {
    double scaled;

    if (baseDimension <= 0) return 1;

    scaled = (double)baseDimension * (double)scaleFactor;
    if (!isfinite(scaled) || scaled < 1.0) return 1;
    if (scaled > (double)INT_MAX) return INT_MAX;
    return (int)scaled;
}

#endif /* WINDOW_H */
