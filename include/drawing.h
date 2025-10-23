/**
 * @file drawing.h
 * @brief Window drawing and painting functions
 * 
 * Modular rendering system for timer display with time formatting,
 * font management, and double-buffered rendering
 */

#ifndef DRAWING_H
#define DRAWING_H

#include <windows.h>
#include "../include/config.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum length of formatted time text buffer */
#define TIME_TEXT_MAX_LEN 50

/** @brief Maximum length of font name buffer */
#define FONT_NAME_MAX_LEN 256

/** @brief Number of text rendering passes for bold effect */
#define TEXT_RENDER_PASSES 8

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Time components structure for unified time representation
 * 
 * Encapsulates hours, minutes, seconds, and centiseconds (hundredths of second)
 * to simplify function signatures and improve code clarity
 */
typedef struct {
    int hours;          /**< Hours component (0-23 for 24h, 1-12 for 12h) */
    int minutes;        /**< Minutes component (0-59) */
    int seconds;        /**< Seconds component (0-59) */
    int centiseconds;   /**< Centiseconds component (0-99) */
} TimeComponents;

/**
 * @brief Rendering context for text display
 * 
 * Encapsulates font and color settings to reduce parameter passing
 * and improve function composability
 */
typedef struct {
    const char* fontFileName;     /**< Font file name to use */
    const char* fontInternalName; /**< Internal font name for rendering */
    COLORREF textColor;           /**< RGB color for text rendering */
    float fontScaleFactor;        /**< Font scaling factor */
} RenderContext;

/* ============================================================================
 * Public API - Main Entry Point
 * ============================================================================ */

/**
 * @brief Handle window paint events
 * @param hwnd Window handle to paint
 * @param ps Paint structure containing drawing context
 * 
 * Main entry point for window painting. Orchestrates the rendering pipeline:
 * 1. Setup double buffering
 * 2. Determine time display mode
 * 3. Format time text
 * 4. Render to backbuffer
 * 5. Present to screen
 */
void HandleWindowPaint(HWND hwnd, PAINTSTRUCT *ps);

/* ============================================================================
 * Public API - Timer Millisecond Tracking
 * ============================================================================ */

/**
 * @brief Reset timer-based centisecond tracking
 * 
 * Should be called when timer starts, resumes, or resets to establish
 * a new baseline for centisecond calculation
 */
void ResetTimerMilliseconds(void);

/**
 * @brief Save current centiseconds when pausing
 * 
 * Should be called when timer is paused to freeze the centisecond display
 * at the current value
 */
void PauseTimerMilliseconds(void);

#endif