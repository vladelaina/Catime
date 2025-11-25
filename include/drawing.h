/**
 * @file drawing.h
 * @brief Timer display rendering with double buffering (unified header)
 * 
 * Double buffering prevents flicker during frequent updates (especially for centiseconds).
 * RenderContext struct reduces parameter passing in rendering pipeline.
 */

#ifndef DRAWING_H
#define DRAWING_H

#include <windows.h>
#include "config.h"
#include "drawing/drawing_timer_precision.h"
#include "drawing/drawing_time_format.h"
#include "drawing/drawing_render.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Buffer sizes chosen to accommodate:
 * - TIME_TEXT_MAX_LEN: Extended to 4096 to support rich Markdown content from plugins
 * - FONT_NAME_MAX_LEN: Maximum font name length per Windows API (LF_FACESIZE = 32, extended for full paths)
 */
#define TIME_TEXT_MAX_LEN 4096
#define FONT_NAME_MAX_LEN 256

/** @brief Multiple passes for bold effect (simulated via offset rendering) */
#define TEXT_RENDER_PASSES 8

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/** TimeComponents is now defined in drawing_time_format.h to avoid circular dependency */

/**
 * @brief Rendering context (reduces parameter passing)
 */
typedef struct {
    const char* fontFileName;
    const char* fontInternalName;
    COLORREF textColor;
    float fontScaleFactor;
} RenderContext;

/* ============================================================================
 * Public API - Main Entry Point
 * ============================================================================ */

/**
 * @brief Main paint handler
 * @param hwnd Window handle
 * @param ps Paint structure
 * 
 * @details
 * Pipeline: setup double buffer → determine mode → format text → 
 * render to backbuffer → present.
 */
void HandleWindowPaint(HWND hwnd, PAINTSTRUCT *ps);

/* ============================================================================
 * Public API - Timer Millisecond Tracking
 * ============================================================================ */

/**
 * @brief Reset centisecond baseline
 * 
 * @details Call when timer starts/resumes/resets
 */
void ResetTimerMilliseconds(void);

/**
 * @brief Freeze centiseconds at current value
 * 
 * @details Call when pausing timer
 */
void PauseTimerMilliseconds(void);

#endif