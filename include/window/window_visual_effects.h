/**
 * @file window_visual_effects.h
 * @brief Window visual effects (blur, click-through, transparency)
 */

#ifndef WINDOW_VISUAL_EFFECTS_H
#define WINDOW_VISUAL_EFFECTS_H

#include <windows.h>

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Configure window click-through behavior
 * @param hwnd Window handle
 * @param enable TRUE to enable click-through, FALSE to make window interactive
 * 
 * @details Uses soft click-through (WM_NCHITTEST) to allow selective clicking
 */
void SetClickThrough(HWND hwnd, BOOL enable);

/**
 * @brief Check if soft click-through mode is enabled
 * @return TRUE if click-through is enabled
 */
BOOL IsSoftClickThroughEnabled(void);

/**
 * @brief Toggle blur-behind visual effect
 * @param hwnd Window handle
 * @param enable TRUE to enable, FALSE to disable
 * 
 * @details Windows 10+ acrylic, fallback to DWM blur on older systems
 */
void SetBlurBehind(HWND hwnd, BOOL enable);

/**
 * @brief Initialize DWM functions for blur effects
 * @return TRUE if loaded successfully
 * 
 * @details Loads dwmapi.dll dynamically for Windows Vista+ compatibility
 */
BOOL InitDWMFunctions(void);

#endif /* WINDOW_VISUAL_EFFECTS_H */

