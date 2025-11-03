/**
 * @file tray_animation.h
 * @brief Tray icon animation with dynamic speed control
 *
 * Supports folder frames, GIF/WebP, or percent mode (CPU/Memory display).
 * Speed scales based on configurable metrics (CPU/Memory/Timer progress).
 * Preload at startup provides first frame before tray icon creation.
 */

#ifndef TRAY_ANIMATION_H
#define TRAY_ANIMATION_H

#include <windows.h>

#define MAX_TRAY_FRAMES 64

/**
 * @brief Start animation
 * @param hwnd Window handle
 * @param intervalMs Base frame interval
 */
void StartTrayAnimation(HWND hwnd, UINT intervalMs);

/**
 * @brief Stop and free resources
 */
void StopTrayAnimation(HWND hwnd);

/**
 * @brief Get current animation name
 */
const char* GetCurrentAnimationName(void);

/**
 * @brief Set animation and persist to config
 * @return TRUE if changed and reloaded
 */
BOOL SetCurrentAnimationName(const char* name);

/**
 * @brief Preview animation (no persistence)
 */
void StartAnimationPreview(const char* name);

/**
 * @brief Cancel preview and restore previous
 */
void CancelAnimationPreview(void);

/**
 * @brief Preload frames from config (for startup, no timers)
 */
void PreloadAnimationFromConfig(void);

/**
 * @brief Get initial frame for tray icon creation
 * @return First frame HICON or NULL
 */
HICON GetInitialAnimationHicon(void);

/**
 * @brief Handle animation menu commands
 */
BOOL HandleAnimationMenuCommand(HWND hwnd, UINT id);

/**
 * @brief Apply animation path without persistence
 * @param value "__logo__", absolute path, or relative name
 */
void ApplyAnimationPathValueNoPersist(const char* value);

/**
 * @brief Set base interval (<=0 resets to 150ms default)
 */
void TrayAnimation_SetBaseIntervalMs(UINT ms);

/**
 * @brief Recompute delay from speed metric (no frame advance)
 * 
 * @details Speed scales based on CPU/Memory/Timer metric config
 */
void TrayAnimation_RecomputeTimerDelay(void);

/**
 * @brief Update percent icon if in CPU/Memory mode
 */
void TrayAnimation_UpdatePercentIconIfNeeded(void);

/**
 * @brief Create 16x16 percent icon for preview
 * @param percent Value (0-100)
 */
HICON CreatePercentIcon16(int percent);

/**
 * @brief Set minimum interval floor (0 disables)
 */
void TrayAnimation_SetMinIntervalMs(UINT ms);

/**
 * @brief Handle WM_TRAY_UPDATE_ICON message
 * @return TRUE if handled
 * 
 * @details Must be called from main thread for thread safety
 */
BOOL TrayAnimation_HandleUpdateMessage(void);

#endif


