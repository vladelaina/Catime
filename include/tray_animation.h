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

#endif


