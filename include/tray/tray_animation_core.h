/**
 * @file tray_animation_core.h
 * @brief Animation lifecycle management and state coordination
 * 
 * Central coordinator that orchestrates decoder, loader, timer, and menu modules.
 * Manages main and preview animation states.
 */

#ifndef TRAY_ANIMATION_CORE_H
#define TRAY_ANIMATION_CORE_H

#include <windows.h>

/**
 * @brief Start animation system
 * @param hwnd Window handle for message posting
 * @param intervalMs Base interval for folder animations (default 150ms)
 * 
 * @details
 * Initializes timer, loads animation from config, starts updates.
 */
void StartTrayAnimation(HWND hwnd, UINT intervalMs);

/**
 * @brief Stop animation and free all resources
 * @param hwnd Window handle (for timer cleanup)
 */
void StopTrayAnimation(HWND hwnd);

/**
 * @brief Get current animation name
 * @return Animation identifier (e.g., "__logo__", "cat.gif")
 */
const char* GetCurrentAnimationName(void);

/**
 * @brief Set animation and persist to config
 * @param name Animation identifier
 * @return TRUE if changed and loaded
 * 
 * @details
 * Validates animation exists, loads frames, updates config.
 * Seamlessly promotes preview if switching to current preview.
 */
BOOL SetCurrentAnimationName(const char* name);

/**
 * @brief Preview animation without persistence
 * @param name Animation identifier
 * 
 * @details
 * Loads into preview slot, displays immediately.
 * Can be promoted to main via SetCurrentAnimationName or cancelled.
 */
void StartAnimationPreview(const char* name);

/**
 * @brief Cancel preview and restore original animation
 */
void CancelAnimationPreview(void);

/**
 * @brief Preload animation from config (for startup)
 * 
 * @details
 * Loads frames without starting timer.
 * Called before tray icon creation to have first frame ready.
 */
void PreloadAnimationFromConfig(void);

/**
 * @brief Get first frame for tray icon creation
 * @return HICON or NULL
 */
HICON GetInitialAnimationHicon(void);

/**
 * @brief Apply animation path from config without persistence
 * @param value Animation path (from config file)
 * 
 * @details
 * For config watcher - applies changes without writing back.
 */
void ApplyAnimationPathValueNoPersist(const char* value);

/**
 * @brief Set base interval for folder animations
 * @param ms Interval in milliseconds (<=0 resets to 150ms)
 */
void TrayAnimation_SetBaseIntervalMs(UINT ms);

/**
 * @brief Set minimum interval floor
 * @param ms Minimum interval (0 disables)
 */
void TrayAnimation_SetMinIntervalMs(UINT ms);

/**
 * @brief Recompute timer delay (no-op, kept for compatibility)
 */
void TrayAnimation_RecomputeTimerDelay(void);

/**
 * @brief Clear current animation name to force reload on next apply
 */
void TrayAnimation_ClearCurrentName(void);

/**
 * @brief Update percent icon if in CPU/Memory mode
 * 
 * @details
 * Called from tray tooltip update timer.
 * Only updates if current animation is __cpu__ or __mem__.
 */
void TrayAnimation_UpdatePercentIconIfNeeded(void);

/**
 * @brief Handle WM_TRAY_UPDATE_ICON message (must run in main thread)
 * @return TRUE if handled
 */
BOOL TrayAnimation_HandleUpdateMessage(void);

/**
 * @brief Check if animation preview is currently active
 * @return TRUE if animation preview is active
 */
extern BOOL g_isPreviewActive;

#endif /* TRAY_ANIMATION_CORE_H */

