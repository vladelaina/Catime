/**
 * @file tray_animation_timer.h
 * @brief High-precision animation timer with adaptive frame rate
 * 
 * Uses multimedia timer (timeSetEvent) with interval-aware precision.
 * Separate internal tick from tray update to prevent Explorer throttling.
 * Adaptive frame rate adjusts to system load.
 */

#ifndef TRAY_ANIMATION_TIMER_H
#define TRAY_ANIMATION_TIMER_H

#include <windows.h>

/**
 * @brief Frame rate controller for smooth animation
 */
typedef struct {
    UINT targetInterval;
    double trayAccumulatorMs;
} FrameRateController;

/**
 * @brief Timer callback signature
 * @param userData User data pointer
 */
typedef void (*AnimationTimerCallback)(void* userData);

/**
 * @brief Initialize frame rate controller
 * @param ctrl Controller instance
 * @param targetMs Target update interval
 */
void FrameRateController_Init(FrameRateController* ctrl, UINT targetMs);

/**
 * @brief Check if tray update should occur
 * @param ctrl Controller instance
 * @param elapsedMs Real elapsed time since the previous animation tick
 * @return TRUE if accumulated enough time
 */
BOOL FrameRateController_ShouldUpdateTray(FrameRateController* ctrl, double elapsedMs);

/**
 * @brief Initialize high-precision timer
 * @param hwnd Window handle for message posting
 * @param internalIntervalMs Internal tick interval (default 10ms)
 * @param callback Timer callback function
 * @param userData User data for callback
 * @return TRUE on success, FALSE if unavailable (use SetTimer fallback)
 * 
 * @details
 * Attempts to use multimedia timer with precision matched to the internal tick.
 * Callback executes in worker thread - use thread-safe operations only.
 */
BOOL InitializeAnimationTimer(HWND hwnd, UINT internalIntervalMs,
                               AnimationTimerCallback callback, void* userData);

/**
 * @brief Cleanup timer and restore system settings
 */
BOOL CleanupAnimationTimer(void);

/**
 * @brief Check whether the animation timer is currently running
 * @return TRUE if any timer backend is active
 */
BOOL IsAnimationTimerActive(void);

/**
 * @brief Check if using high-precision timer
 * @return TRUE if multimedia timer active, FALSE if using SetTimer
 */
BOOL IsUsingHighPrecisionTimer(void);

#endif /* TRAY_ANIMATION_TIMER_H */

