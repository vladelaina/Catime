/**
 * @file color.h
 * @brief Flexible color input supporting CSS names, hex codes, and RGB values
 * 
 * Multiple formats accommodate different user backgrounds (web developers, designers, general users).
 * Format normalization ensures consistent internal representation regardless of input method.
 * 
 * Black (#000000) auto-converts to #000001 to prevent invisible text (visually indistinguishable).
 */

#ifndef COLOR_H
#define COLOR_H

#include <windows.h>

/* ============================================================================
 * Type definitions
 * ============================================================================ */

/**
 * @brief User's saved color palette entry
 */
typedef struct {
    const char* hexColor;  /**< Normalized hex format for consistent comparison */
} PredefinedColor;

/**
 * @brief CSS color name to hex mapping
 */
typedef struct {
    const char* name;  /**< Case-insensitive */
    const char* hex;   /**< Pre-normalized */
} CSSColor;

/* ============================================================================
 * Global state variables
 * ============================================================================ */

/** @brief Dynamically allocated palette (supports unlimited colors) */
extern PredefinedColor* COLOR_OPTIONS;

/** @brief Palette size (tracked separately due to reallocation) */
extern size_t COLOR_OPTIONS_COUNT;

/** @brief Temporary storage during live preview ("#RRGGBB\0" + safety) */
extern char PREVIEW_COLOR[10];

/** @brief Tracks whether preview is active (prevents unwanted restoration) */
extern BOOL IS_COLOR_PREVIEWING;

/** @brief Active clock color (separate from preview to enable cancellation) */
extern char CLOCK_TEXT_COLOR[10];

/* ============================================================================
 * Public API functions
 * ============================================================================ */

/**
 * @brief Loads saved color palette or initializes defaults
 * 
 * @details
 * Handles missing '#' prefix for backward compatibility.
 * Must be called before showing color UI.
 * 
 * @note Function name is historical artifact - actually initializes colors
 */
void InitializeDefaultLanguage(void);

/**
 * @brief Adds validated color to palette (auto-normalized, deduplicated)
 * @param hexColor Color to add
 * 
 * @details
 * Deduplication prevents palette bloat. Silent failure on invalid colors
 * avoids interrupting bulk loads from config.
 * 
 * @note Case-insensitive (#FF0000 = #ff0000)
 */
void AddColorOption(const char* hexColor);

/**
 * @brief Releases all palette memory (idempotent)
 * 
 * @details
 * Required before reloading config and during shutdown.
 */
void ClearColorOptions(void);

/**
 * @brief Persists color to config (auto-normalized)
 * @param color_input Color string to save
 * 
 * @details Immediate write ensures persistence even on crash.
 */
void WriteConfigColor(const char* color_input);

/**
 * @brief Converts any supported format to canonical #RRGGBB hex
 * @param input Color in any format (CSS name, hex, RGB)
 * @param output Buffer for normalized hex (min 10 bytes)
 * @param output_size Buffer size
 * 
 * @details
 * Supports multiple RGB separators (comma, Chinese comma, semicolon, pipe)
 * for international keyboards and copy-paste scenarios.
 * Returns input unchanged on failure to enable caller error handling.
 */
void normalizeColor(const char* input, char* output, size_t output_size);

/**
 * @brief Validates color string (accepts any format)
 * @param input Color string
 * @return TRUE if normalizes to valid #RRGGBB, FALSE otherwise
 * 
 * @note Post-normalization validation ("red" returns TRUE)
 */
BOOL isValidColor(const char* input);

/**
 * @brief Subclassed edit control with live preview
 * @param hwnd Edit control handle
 * @param msg Windows message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 * 
 * @details
 * - Live preview on keystroke for immediate feedback
 * - Ctrl+A for select all (standard behavior)
 * - Enter for keyboard submission (accessibility)
 * - Preview on paste/cut (handles clipboard path)
 */
LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Dialog procedure for text-based color input
 * @param hwndDlg Dialog handle
 * @param msg Windows message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 * 
 * @details
 * Empty input cancels (supports "changed my mind" workflow).
 * Validation errors show guidance dialog. Immediate config save.
 */
INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Windows color picker with live preview
 * @param hwnd Parent window handle
 * @return Selected COLORREF or -1 on cancel
 * 
 * @details
 * 16-color custom palette for quick access to frequent colors.
 * Black auto-converts to near-black (#000001) to prevent invisible text.
 * Returns -1 on cancel to distinguish from black (#000000 = 0).
 * 
 * @note Custom colors persist in session only (saved on explicit apply)
 */
COLORREF ShowColorDialog(HWND hwnd);

/**
 * @brief Hook procedure for mouse-based color sampling
 * @param hdlg Color dialog handle
 * @param msg Windows message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Hook processing result
 * 
 * @details
 * Enables "eyedropper" functionality for screen color sampling.
 * Click-to-lock prevents jitter when user finds desired color.
 * Filters dialog background (RGB(240,240,240)) to sample content only.
 * Cancel restores original color for risk-free exploration.
 * 
 * @note Uses GetPixel() with screen DC for cross-window sampling
 */
UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

#endif