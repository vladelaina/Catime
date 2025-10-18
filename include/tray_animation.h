/**
 * @file tray_animation.h
 * @brief System tray icon animation controller
 *
 * Loads frames from %LOCALAPPDATA%\Catime\resources\animations (folder or single GIF/WebP)
 * and animates the tray icon at a fixed interval.
 */

#ifndef TRAY_ANIMATION_H
#define TRAY_ANIMATION_H

#include <windows.h>

/** @brief Max frames supported for tray icon animations. */
#define MAX_TRAY_FRAMES 64

/**
 * @brief Start tray icon animation.
 * @param hwnd Main window handle owning the tray icon (CLOCK_ID_TRAY_APP_ICON)
 * @param intervalMs Frame interval in milliseconds (fixed speed)
 */
void StartTrayAnimation(HWND hwnd, UINT intervalMs);

/**
 * @brief Stop tray icon animation and free resources.
 * @param hwnd Main window handle
 */
void StopTrayAnimation(HWND hwnd);

/**
 * @brief Get current animation folder name
 */
const char* GetCurrentAnimationName(void);

/**
 * @brief Set current animation folder name and persist to config
 * @return TRUE if changed and reloaded successfully
 */
BOOL SetCurrentAnimationName(const char* name);

/**
 * @brief Start temporary animation preview (no persistence)
 */
void StartAnimationPreview(const char* name);

/**
 * @brief Cancel temporary animation preview and restore previous animation
 */
void CancelAnimationPreview(void);

/**
 * @brief Preload animation frames based on config without starting timers.
 * Used at startup to obtain the first frame before adding the tray icon.
 */
void PreloadAnimationFromConfig(void);

/**
 * @brief Get initial tray HICON for startup. Returns first animation frame if available, otherwise NULL.
 */
HICON GetInitialAnimationHicon(void);

/**
 * @brief Handle menu commands related to animations.
 * @param hwnd Main window handle
 * @param id The command identifier of the menu item.
 * @return TRUE if the command was handled, FALSE otherwise.
 */
BOOL HandleAnimationMenuCommand(HWND hwnd, UINT id);

/**
 * @brief Apply a new ANIMATION_PATH value without persisting to config
 * @param value The INI value; accepts "__logo__", absolute path with %LOCALAPPDATA% prefix,
 *              or relative name under animations folder.
 */
void ApplyAnimationPathValueNoPersist(const char* value);

/**
 * @brief Set base interval (ms) for folder/static image sequences and recompute timer.
 * @param ms Interval in milliseconds (<=0 to reset to default 150)
 */
void TrayAnimation_SetBaseIntervalMs(UINT ms);

/**
 * @brief Recompute current tray animation timer delay based on latest speed mapping/metric
 *        without advancing the frame.
 */
void TrayAnimation_RecomputeTimerDelay(void);

/**
 * @brief If current animation is a percent mode ("__cpu__"/"__mem__"),
 *        regenerate and apply a new tray HICON based on latest values.
 */
void TrayAnimation_UpdatePercentIconIfNeeded(void);

/**
 * @brief Create a small icon with percentage text for preview purposes
 * @param percent The percentage value (0-100)
 * @return HICON handle, or NULL if creation failed
 */
HICON CreatePercentIcon16(int percent);

/**
 * @brief Set user-configured minimum tray animation interval in milliseconds.
 *        0 disables the floor (uses system minimum only).
 */
void TrayAnimation_SetMinIntervalMs(UINT ms);

/**
 * @brief Handle tray icon update message (WM_TRAY_UPDATE_ICON)
 * This function should be called from the main window procedure when
 * WM_USER+100 message is received. It performs the actual tray icon
 * update in the main UI thread, which is thread-safe.
 * 
 * @return TRUE if message was handled
 */
BOOL TrayAnimation_HandleUpdateMessage(void);

#endif


