/**
 * @file tray_menu.h
 * @brief Tray context menu system with dynamic content
 * 
 * Modular helpers eliminate duplication (config reading, path conversion).
 * Dynamic menu items loaded from config (Pomodoro presets, recent files, fonts, animations).
 * Menu IDs organized by functional groups for maintainability.
 */

#ifndef CLOCK_TRAY_MENU_H
#define CLOCK_TRAY_MENU_H

#include <windows.h>
#include "../../resource/resource.h"

/**
 * @brief Show context menu (left-click)
 * @param hwnd Window handle
 * @param anchor Optional Shell-provided screen coordinate
 * 
 * @details
 * Timer controls, time display, Pomodoro presets (loaded from config).
 * Dynamic content reloaded each display.
 */
void ShowContextMenu(HWND hwnd, const POINT* anchor);

/**
 * @brief Show config menu (right-click)
 * @param hwnd Window handle
 * @param anchor Optional Shell-provided screen coordinate
 * 
 * @details
 * Comprehensive settings: edit mode, timeout actions, presets, time format,
 * fonts (recursive scan), colors, animations (GIF/WebP/ANI + speed metrics),
 * help, language.
 * 
 * Modular helpers for maintainability.
 */
void ShowColorMenu(HWND hwnd, const POINT* anchor);

#endif
