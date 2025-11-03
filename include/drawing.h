/**
 * @file drawing.h
 * @brief Timer display rendering with double buffering
 * 
 * Double buffering prevents flicker during frequent updates (especially for centiseconds).
 * RenderContext struct reduces parameter passing in rendering pipeline.
 */

#ifndef DRAWING_H
#define DRAWING_H

#include <windows.h>
#include "../include/config.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TIME_TEXT_MAX_LEN 50
#define FONT_NAME_MAX_LEN 256

/** @brief Multiple passes for bold effect (simulated via offset rendering) */
#define TEXT_RENDER_PASSES 8

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Time components (simplifies function signatures)
 */
typedef struct {
    int hours;          /**< 0-23 (24h) or 1-12 (12h) */
    int minutes;        /**< 0-59 */
    int seconds;        /**< 0-59 */
    int centiseconds;   /**< 0-99 (hundredths) */
} TimeComponents;

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