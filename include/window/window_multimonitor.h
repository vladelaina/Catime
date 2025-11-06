/**
 * @file window_multimonitor.h
 * @brief Multi-monitor detection and window positioning
 */

#ifndef WINDOW_MULTIMONITOR_H
#define WINDOW_MULTIMONITOR_H

#include <windows.h>

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Adjust window position for visibility on active monitor
 * @param hwnd Window handle
 * @param forceOnScreen TRUE to reposition if off-screen/inactive display
 * 
 * @details Handles:
 * - Disconnected monitor
 * - Disabled monitor ("Second screen only")
 * - Off-screen position
 * 
 * Centers on best active monitor and saves if repositioned.
 */
void AdjustWindowPosition(HWND hwnd, BOOL forceOnScreen);

#endif /* WINDOW_MULTIMONITOR_H */

