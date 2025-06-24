/**
 * @file timer_events.h
 * @brief Timer event handling interface
 * 
 * This file defines the application's timer event handling function interfaces,
 * including event handling for countdown and count-up modes.
 */

#ifndef TIMER_EVENTS_H
#define TIMER_EVENTS_H

#include <windows.h>

/**
 * @brief Handle timer messages
 * @param hwnd Window handle
 * @param wp Message parameter
 * @return BOOL Whether the message was handled
 * 
 * Process WM_TIMER messages, including:
 * 1. Countdown mode: Update remaining time and execute timeout actions
 * 2. Count-up mode: Accumulate elapsed time
 * 3. Display current time mode
 */
BOOL HandleTimerEvent(HWND hwnd, WPARAM wp);

#endif // TIMER_EVENTS_H