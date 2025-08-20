/**
 * @file window_events.h
 * @brief Window lifecycle and event handling functions
 * 
 * Provides functions for handling window creation, destruction, resizing, and movement events
 */

#ifndef WINDOW_EVENTS_H
#define WINDOW_EVENTS_H

#include <windows.h>

/**
 * @brief Handle window creation initialization
 * @param hwnd Window handle
 * @return TRUE on success, FALSE on failure
 */
BOOL HandleWindowCreate(HWND hwnd);

/**
 * @brief Handle window destruction cleanup
 * @param hwnd Window handle
 */
void HandleWindowDestroy(HWND hwnd);

/**
 * @brief Reset window to default state
 * @param hwnd Window handle
 */
void HandleWindowReset(HWND hwnd);

/**
 * @brief Handle window resize events
 * @param hwnd Window handle
 * @param delta Resize delta value
 * @return TRUE if resize was handled, FALSE otherwise
 */
BOOL HandleWindowResize(HWND hwnd, int delta);

/**
 * @brief Handle window movement events
 * @param hwnd Window handle
 * @return TRUE if move was handled, FALSE otherwise
 */
BOOL HandleWindowMove(HWND hwnd);

#endif