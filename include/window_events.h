/**
 * @file window_events.h
 * @brief Basic window event handling interface
 * 
 * This file defines the application window's basic event handling functionality interfaces,
 * including window creation, destruction, resizing, and position adjustment functions.
 */

#ifndef WINDOW_EVENTS_H
#define WINDOW_EVENTS_H

#include <windows.h>

/**
 * @brief Handle window creation event
 * @param hwnd Window handle
 * @return BOOL Processing result
 */
BOOL HandleWindowCreate(HWND hwnd);

/**
 * @brief Handle window destruction event
 * @param hwnd Window handle
 */
void HandleWindowDestroy(HWND hwnd);

/**
 * @brief Handle window reset event
 * @param hwnd Window handle
 */
void HandleWindowReset(HWND hwnd);

/**
 * @brief Handle window resize event
 * @param hwnd Window handle
 * @param delta Mouse wheel increment
 * @return BOOL Whether the event was handled
 */
BOOL HandleWindowResize(HWND hwnd, int delta);

/**
 * @brief Handle window move event
 * @param hwnd Window handle
 * @return BOOL Whether the event was handled
 */
BOOL HandleWindowMove(HWND hwnd);

#endif // WINDOW_EVENTS_H