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
 * @brief Get colors that would be used for rendering an icon now
 * @param textColor Receives resolved text color, including theme-aware auto text
 * @param bgColor Receives configured background color or TRANSPARENT_BG_AUTO
 * @return TRUE on success
 */
BOOL GetPercentIconColorSnapshot(COLORREF* textColor, COLORREF* bgColor);

/**
 * @brief Get normalized generated tray icon size
 * @param outCx Receives icon width in pixels
 * @param outCy Receives icon height in pixels
 */
void GetGeneratedTrayIconSizeSnapshot(int* outCx, int* outCy);

/**
 * @brief Create 16x16 percent icon with rendered text
 * @param percent Value (0-100, clamped)
 * @return HICON or NULL on failure
 * 
 * @details
 * Renders percentage as text on small icon.
 * Uses the system UI font and auto-scales text to fit icon size.
 */
HICON CreatePercentIcon16(int percent);

/**
 * @brief Create Caps Lock indicator icon
 * @param capsOn TRUE if Caps Lock is on, FALSE otherwise
 * @return HICON or NULL on failure
 * 
 * @details
 * Renders "A" when Caps Lock is on, "a" when off.
 * Uses the system UI font and the same color scheme as percent icons.
 */
HICON CreateCapsLockIcon(BOOL capsOn);

/**
 * @brief Release cached generated percent/Caps Lock icons
 */
void CleanupPercentIconCache(void);

/**
 * @brief Check current Caps Lock state
 * @return TRUE if Caps Lock is on
 */
BOOL IsCapsLockOn(void);

#endif /* TRAY_ANIMATION_PERCENT_H */

