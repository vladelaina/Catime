/**
 * @file drawing.h
 * @brief Window drawing and painting functions
 * 
 * Functions for handling window painting and graphics rendering
 */

#ifndef DRAWING_H
#define DRAWING_H

#include <windows.h>

/**
 * @brief Handle window paint events
 * @param hwnd Window handle to paint
 * @param ps Paint structure containing drawing context
 */
void HandleWindowPaint(HWND hwnd, PAINTSTRUCT *ps);

/**
 * @brief Reset timer-based millisecond tracking
 * Should be called when timer starts, resumes, or resets
 */
void ResetTimerMilliseconds(void);

/**
 * @brief Save current milliseconds when pausing
 * Should be called when timer is paused to freeze the display
 */
void PauseTimerMilliseconds(void);

#endif