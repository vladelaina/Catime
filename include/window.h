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

/* Global effect flags */
extern BOOL CLOCK_GLOW_EFFECT;
extern BOOL CLOCK_GLASS_EFFECT;
extern BOOL CLOCK_NEON_EFFECT;
extern BOOL CLOCK_HOLOGRAPHIC_EFFECT;
extern BOOL CLOCK_LIQUID_EFFECT;

/* Global configuration variables */

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
