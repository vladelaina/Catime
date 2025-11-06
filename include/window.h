/**
 * @file window.h
 * @brief Window management - unified header for all window modules
 * 
 * This file aggregates all window-related functionality:
 * - Core window creation and lifecycle
 * - Visual effects (blur, click-through)
 * - Multi-monitor support
 * - Application initialization
 * - Desktop integration and Z-order
 */

#ifndef WINDOW_H
#define WINDOW_H

/* Include all window module headers */
#include "window/window_core.h"
#include "window/window_visual_effects.h"
#include "window/window_multimonitor.h"
#include "window/window_initialization.h"
#include "window/window_desktop_integration.h"

/* ============================================================================
 * Scaling constraints
 * ============================================================================ */

#define MIN_SCALE_FACTOR 0.5f
#define MAX_SCALE_FACTOR 100.0f

#endif /* WINDOW_H */
