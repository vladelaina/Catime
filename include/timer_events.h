/**
 * @file timer_events.h
 * @brief Timer event handling
 * 
 * Functions for processing WM_TIMER messages and timer-related events
 */

#ifndef TIMER_EVENTS_H
#define TIMER_EVENTS_H

#include <windows.h>

/**
 * @brief Handle timer events from window message loop
 * @param hwnd Window handle receiving timer event
 * @param wp Timer ID parameter
 * @return TRUE if event was handled, FALSE otherwise
 */
BOOL HandleTimerEvent(HWND hwnd, WPARAM wp);

/**
 * @brief Reset millisecond accumulator and timer baseline for accurate timing
 * Should be called when pausing/resuming or restarting timer to prevent time jumps
 */
void ResetMillisecondAccumulator(void);

#endif