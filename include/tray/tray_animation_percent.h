/**
 * @file tray_animation_percent.h
 * @brief CPU/Memory percent icon generation
 * 
 * Dynamically renders numeric percentage text on tray icon.
 * Configurable colors for text and background.
 */

#ifndef TRAY_ANIMATION_PERCENT_H
#define TRAY_ANIMATION_PERCENT_H

#include <windows.h>

/**
 * @brief Transparent background marker value
 *
 * When bgColor is set to this value, the icon background becomes transparent
 * and text color automatically adapts based on Windows theme (dark/light).
 */
#define TRANSPARENT_BG_AUTO 0xFFFFFFFF

/**
 * @brief Set percent icon colors
 * @param textColor Foreground color (RGB)
 * @param bgColor Background color (RGB or TRANSPARENT_BG_AUTO)
 */
void SetPercentIconColors(COLORREF textColor, COLORREF bgColor);

/**
 * @brief Get current text color
 * @return Text COLORREF
 */
COLORREF GetPercentIconTextColor(void);

/**
 * @brief Get current background color
 * @return Background COLORREF
 */
COLORREF GetPercentIconBgColor(void);

/**
 * @brief Create 16x16 percent icon with rendered text
 * @param percent Value (0-999, clamped)
 * @return HICON or NULL on failure
 * 
 * @details
 * Renders percentage as text on small icon.
 * Uses Segoe UI Bold for clarity.
 * Auto-scales text to fit icon size.
 */
HICON CreatePercentIcon16(int percent);

#endif /* TRAY_ANIMATION_PERCENT_H */

