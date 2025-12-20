/**
 * @file main_timer.h
 * @brief High-precision main window timer using multimedia timer
 * 
 * Provides smooth millisecond display by using timeSetEvent instead of SetTimer.
 * Shares timing infrastructure with tray animation system.
 */

#ifndef MAIN_TIMER_H
#define MAIN_TIMER_H

#include <windows.h>

/**
 * @brief Initialize high-precision timer for main window
 * @param hwnd Main window handle
 * @param intervalMs Timer interval in milliseconds
 * @return TRUE if successful
 */
BOOL MainTimer_Init(HWND hwnd, UINT intervalMs);

/**
 * @brief Update timer interval (e.g., when switching to/from milliseconds mode)
 * @param intervalMs New interval in milliseconds
 */
void MainTimer_SetInterval(UINT intervalMs);

/**
 * @brief Cleanup timer resources
 */
void MainTimer_Cleanup(void);

/**
 * @brief Check if high-precision timer is active
 * @return TRUE if using multimedia timer
 */
BOOL MainTimer_IsHighPrecision(void);

#endif /* MAIN_TIMER_H */
