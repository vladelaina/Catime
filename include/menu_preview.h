/**
 * @file menu_preview.h
 * @brief Menu option live preview system
 * 
 * Provides real-time preview when hovering over menu items, allowing users
 * to see changes before applying them. Supports colors, fonts, time formats,
 * animations, and more.
 * 
 * Usage pattern:
 * 1. StartPreview(type, data, hwnd) - Show preview
 * 2. User sees effect in real-time
 * 3. ApplyPreview(hwnd) - Commit changes, or
 *    CancelPreview(hwnd) - Revert to original
 */

#ifndef MENU_PREVIEW_H
#define MENU_PREVIEW_H

#include <windows.h>

/* Forward declaration to avoid circular dependency */
#ifndef TIME_FORMAT_TYPE_DEFINED
typedef enum {
    TIME_FORMAT_DEFAULT = 0,
    TIME_FORMAT_ZERO_PADDED = 1,
    TIME_FORMAT_FULL_PADDED = 2
} TimeFormatType;
#define TIME_FORMAT_TYPE_DEFINED
#endif

/* ============================================================================
 * Preview Types
 * ============================================================================ */

/**
 * @brief Preview type discriminator
 */
typedef enum {
    PREVIEW_TYPE_NONE = 0,
    PREVIEW_TYPE_COLOR,
    PREVIEW_TYPE_FONT,
    PREVIEW_TYPE_TIME_FORMAT,
    PREVIEW_TYPE_MILLISECONDS,
    PREVIEW_TYPE_ANIMATION
} PreviewType;

/* ============================================================================
 * Core Preview Functions
 * ============================================================================ */

/**
 * @brief Start preview with type-specific data
 * @param type Preview type to activate
 * @param data Type-specific data (cast internally)
 * @param hwnd Window handle for UI updates
 * 
 * @details
 * - PREVIEW_TYPE_COLOR: data = const char* (hex color)
 * - PREVIEW_TYPE_FONT: data = const char* (font filename)
 * - PREVIEW_TYPE_TIME_FORMAT: data = TimeFormatType*
 * - PREVIEW_TYPE_MILLISECONDS: data = BOOL*
 * - PREVIEW_TYPE_ANIMATION: data = const char* (animation path)
 * 
 * @note Cancels any existing preview before starting new one
 */
void StartPreview(PreviewType type, const void* data, HWND hwnd);

/**
 * @brief Cancel preview and restore original state
 * @param hwnd Window handle for UI refresh
 * 
 * @note Safe to call when no preview is active
 */
void CancelPreview(HWND hwnd);

/**
 * @brief Apply preview changes to configuration
 * @param hwnd Window handle for UI refresh
 * @return TRUE if preview was applied, FALSE if no preview active
 * 
 * @details Automatically saves to config and clears preview state
 */
BOOL ApplyPreview(HWND hwnd);

/**
 * @brief Check if any preview is currently active
 * @return TRUE if preview active, FALSE otherwise
 */
BOOL IsPreviewActive(void);

/**
 * @brief Get current preview type
 * @return Active preview type or PREVIEW_TYPE_NONE
 */
PreviewType GetActivePreviewType(void);

/* ============================================================================
 * Type-Safe Accessors (return preview value if active, otherwise real value)
 * ============================================================================ */

/**
 * @brief Get display color (preview takes precedence)
 * @param outColor Output buffer
 * @param bufferSize Buffer capacity
 * 
 * @details Drawing code should use this instead of CLOCK_TEXT_COLOR directly
 */
void GetActiveColor(char* outColor, size_t bufferSize);

/**
 * @brief Get display font (preview takes precedence)
 * @param outFontName Output buffer for font filename
 * @param outInternalName Output buffer for internal font name
 * @param bufferSize Buffer capacity
 */
void GetActiveFont(char* outFontName, char* outInternalName, size_t bufferSize);

/**
 * @brief Get display time format (preview takes precedence)
 * @return Active time format
 */
TimeFormatType GetActiveTimeFormat(void);

/**
 * @brief Get milliseconds visibility (preview takes precedence)
 * @return Active milliseconds visibility setting
 */
BOOL GetActiveShowMilliseconds(void);

/**
 * @brief Temporarily show hidden window for menu preview
 * @param hwnd Window handle
 *
 * @details If window is currently hidden (No Display mode), this will:
 *          - Show the window temporarily
 *          - Set a default countdown time for preview
 *          - Pause the timer
 *          Should be called when user hovers over font/color menu items
 */
void ShowWindowForPreview(HWND hwnd);

/**
 * @brief Restore window visibility to pre-preview state
 * @param hwnd Window handle
 *
 * @details If window was shown by ShowWindowForPreview(), this will hide it again.
 *          Should be called when menu closes (WM_EXITMENULOOP).
 */
void RestoreWindowVisibility(HWND hwnd);

#endif /* MENU_PREVIEW_H */

