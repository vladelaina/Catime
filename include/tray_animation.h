/**
 * @file tray_animation.h
 * @brief System tray icon animation (RunCat-like) controller
 *
 * Loads sequential .ico files from %LOCALAPPDATA%\Catime\resources\animations\cat
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
 * @brief Handle menu commands related to animations.
 * @param hwnd Main window handle
 * @param id The command identifier of the menu item.
 * @return TRUE if the command was handled, FALSE otherwise.
 */
BOOL HandleAnimationMenuCommand(HWND hwnd, UINT id);

#endif


