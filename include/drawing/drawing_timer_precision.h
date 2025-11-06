/**
 * @file drawing_timer_precision.h
 * @brief High-precision sub-second timer state management
 */

#ifndef DRAWING_TIMER_PRECISION_H
#define DRAWING_TIMER_PRECISION_H

#include <windows.h>

/**
 * Reset timer millisecond tracking
 * @note Call when timer starts or restarts
 */
void ResetTimerMilliseconds(void);

/**
 * Capture current millisecond state before pause
 * @note Prevents display jumps when resuming
 */
void PauseTimerMilliseconds(void);

#endif /* DRAWING_TIMER_PRECISION_H */

