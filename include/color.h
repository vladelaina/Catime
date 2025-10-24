/**
 * @file color.h
 * @brief Color management with CSS colors, live preview, and custom color picker
 * @version 2.0 - Refactored for better maintainability and reduced code duplication
 * 
 * Comprehensive color system supporting:
 * - CSS named colors (30+ standard web colors)
 * - Hex color formats (#RGB, #RRGGBB)
 * - RGB format with multiple separators (comma, space, etc.)
 * - Live color preview during input
 * - Windows color picker with mouse sampling
 * - Automatic black → near-black conversion for visibility
 * 
 * Features:
 * - Smart color normalization (CSS names, hex shorthand, RGB)
 * - Real-time preview as you type
 * - Mouse-based color picking in color dialog
 * - Persistent custom color palette
 * - International separator support (,，;；| space)
 */

#ifndef COLOR_H
#define COLOR_H

#include <windows.h>

/* ============================================================================
 * Type definitions
 * ============================================================================ */

/**
 * @brief Predefined color option structure for user color palette
 */
typedef struct {
    const char* hexColor;  /**< Hex color string (e.g., "#FF0000") */
} PredefinedColor;

/**
 * @brief CSS named color structure for color lookup
 */
typedef struct {
    const char* name;  /**< Color name (e.g., "red", "blue") */
    const char* hex;   /**< Hex color value (e.g., "#FF0000") */
} CSSColor;

/* ============================================================================
 * Global state variables
 * ============================================================================ */

/** @brief Dynamic array of user-defined color options */
extern PredefinedColor* COLOR_OPTIONS;

/** @brief Count of available color options */
extern size_t COLOR_OPTIONS_COUNT;

/** @brief Current preview color hex string (size 10 for "#RRGGBB\0") */
extern char PREVIEW_COLOR[10];

/** @brief Color preview active state (TRUE during live preview) */
extern BOOL IS_COLOR_PREVIEWING;

/** @brief Current clock text color in hex format */
extern char CLOCK_TEXT_COLOR[10];

/* ============================================================================
 * Public API functions
 * ============================================================================ */

/**
 * @brief Initialize default color palette from config file or defaults
 * 
 * @details
 * - Loads COLOR_OPTIONS from config file
 * - Falls back to 16 predefined colors if config missing
 * - Creates config file if it doesn't exist
 * - Handles colors with or without '#' prefix
 * 
 * @note Should be called during application startup
 */
void InitializeDefaultLanguage(void);

/**
 * @brief Add color to user options list with validation and deduplication
 * @param hexColor Hex color string to add (with or without '#')
 * 
 * @details
 * - Validates hex format (#RRGGBB only, 6 digits)
 * - Normalizes to uppercase (#FFFFFF)
 * - Prevents duplicates (case-insensitive)
 * - Dynamically expands COLOR_OPTIONS array
 * - Skips invalid colors silently
 */
void AddColorOption(const char* hexColor);

/**
 * @brief Free all color options memory
 * 
 * @details
 * - Frees all individual color strings
 * - Frees COLOR_OPTIONS array
 * - Resets COLOR_OPTIONS_COUNT to 0
 * - Safe to call multiple times
 * 
 * @note Call before reloading colors or during shutdown
 */
void ClearColorOptions(void);

/**
 * @brief Write CLOCK_TEXT_COLOR to config file
 * @param color_input Color string to save (hex format)
 * 
 * @details
 * - Updates CLOCK_TEXT_COLOR field in INI file
 * - Uses DISPLAY section
 * - Saves immediately to disk
 */
void WriteConfigColor(const char* color_input);

/**
 * @brief Convert various color formats to normalized hex (#RRGGBB)
 * @param input Input color string (CSS name, hex, or RGB)
 * @param output Output buffer for normalized color
 * @param output_size Size of output buffer (recommend 10+ bytes)
 * 
 * @details Parsing priority:
 * 1. CSS color names (case-insensitive): "red" → "#FF0000"
 * 2. Hex shorthand: "#RGB" → "#RRGGBB", "#f00" → "#FF0000"
 * 3. Hex full: "#RRGGBB" or "RRGGBB"
 * 4. RGB format: "255,0,0" or "rgb(255,0,0)" → "#FF0000"
 * 5. Fallback: returns input unchanged
 * 
 * Supported RGB separators: , ， ; ； | space
 * 
 * @note Always outputs uppercase hex
 */
void normalizeColor(const char* input, char* output, size_t output_size);

/**
 * @brief Validate color string format
 * @param input Color string to validate
 * @return TRUE if valid hex color (#RRGGBB), FALSE otherwise
 * 
 * @details
 * - Accepts CSS names, hex, RGB (auto-normalized)
 * - Validates format: must be #RRGGBB with valid hex digits
 * - Uses normalizeColor internally
 * 
 * @note Empty strings return FALSE
 */
BOOL isValidColor(const char* input);

/**
 * @brief Subclass procedure for color input edit control (live preview)
 * @param hwnd Edit control window handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 * 
 * @details Features:
 * - Ctrl+A for select all
 * - Enter key submits dialog
 * - Live preview on every keystroke
 * - Preview updates on paste/cut
 * - Validates color on the fly
 * 
 * @note Must be attached via SetWindowLongPtr(GWLP_WNDPROC)
 */
LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Dialog procedure for color input text dialog
 * @param hwndDlg Dialog window handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 * 
 * @details
 * - Initializes edit control with current color
 * - Validates input on OK click
 * - Shows error dialog for invalid colors
 * - Empty input cancels dialog
 * - Saves valid color to config
 * - Supports live preview via ColorEditSubclassProc
 */
INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Show Windows color picker dialog with live preview
 * @param hwnd Parent window handle
 * @return Selected COLORREF or -1 if cancelled
 * 
 * @details Features:
 * - Initializes with current CLOCK_TEXT_COLOR
 * - Loads custom colors from COLOR_OPTIONS (up to 16)
 * - Live preview via mouse hover (see ColorDialogHookProc)
 * - Saves chosen color to config
 * - Auto-converts #000000 to #000001 for visibility
 * - Updates main window display immediately
 * 
 * @note Custom colors persist across dialog invocations
 */
COLORREF ShowColorDialog(HWND hwnd);

/**
 * @brief Hook procedure for Windows color dialog (mouse color picking)
 * @param hdlg Dialog handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Hook processing result
 * 
 * @details Advanced features:
 * - Mouse hover: live color preview as cursor moves
 * - Click to lock: left/right click toggles color lock
 * - Custom color sync: saves custom colors to config in real-time
 * - Preview cancel: restores original color on Cancel
 * - Pixel sampling: GetPixel at cursor position
 * - Filters dialog background color (RGB(240,240,240))
 * 
 * @note Attached via CC_ENABLEHOOK flag in CHOOSECOLOR
 */
UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

#endif