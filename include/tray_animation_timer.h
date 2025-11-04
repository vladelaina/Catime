/**
 * @file tray_animation_timer.h
 * @brief High-precision animation timer with adaptive frame rate
 * 
 * Uses multimedia timer (timeSetEvent) for 10ms precision.
 * Separate internal tick (10ms) from tray update (50ms) to prevent Explorer throttling.
 * Adaptive frame rate adjusts to system load.
 */

#ifndef TRAY_ANIMATION_TIMER_H
#define TRAY_ANIMATION_TIMER_H

#include <windows.h>

/**
 * @brief Frame rate controller for smooth animation
 */
typedef struct {
    UINT targetInterval;        /**< Target interval in ms */
    UINT effectiveInterval;     /**< Actual interval (adaptive) */
    double framePosition;       /**< Sub-frame position (0.0-1.0) */
    UINT internalAccumulator;   /**< Accumulated time for updates */
    DWORD lastUpdateTime;       /**< Last tray update timestamp */
    UINT consecutiveLateUpdates;/**< Late update counter */
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
 * @brief Check if frame should advance
 * @param ctrl Controller instance
 * @param deltaMs Time elapsed since last call
 * @param baseDelay Current frame delay
 * @return TRUE if frame should advance
 */
BOOL FrameRateController_ShouldAdvanceFrame(FrameRateController* ctrl, 
                                            UINT deltaMs, UINT baseDelay);

/**
 * @brief Record actual update latency for adaptation
 * @param ctrl Controller instance
 * @param actualElapsed Actual elapsed time
 */
void FrameRateController_RecordLatency(FrameRateController* ctrl, UINT actualElapsed);

/**
 * @brief Check if tray update should occur
 * @param ctrl Controller instance
 * @return TRUE if accumulated enough time
 */
BOOL FrameRateController_ShouldUpdateTray(FrameRateController* ctrl);

/**
 * @brief Initialize high-precision timer
 * @param hwnd Window handle for message posting
 * @param internalIntervalMs Internal tick interval (default 10ms)
 * @param callback Timer callback function
 * @param userData User data for callback
 * @return TRUE on success, FALSE if unavailable (use SetTimer fallback)
 * 
 * @details
 * Attempts to use multimedia timer for best precision.
 * Sets system timer resolution to 1ms.
 * Callback executes in worker thread - use thread-safe operations only.
 */
BOOL InitializeAnimationTimer(HWND hwnd, UINT internalIntervalMs,
                               AnimationTimerCallback callback, void* userData);

/**
 * @brief Cleanup timer and restore system settings
 */
void CleanupAnimationTimer(void);

/**
 * @brief Check if using high-precision timer
 * @return TRUE if multimedia timer active, FALSE if using SetTimer
 */
BOOL IsUsingHighPrecisionTimer(void);

#endif /* TRAY_ANIMATION_TIMER_H */

