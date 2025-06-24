/**
 * @file color.h
 * @brief Color processing functionality interface
 * 
 * This file defines the application's color processing functionality interface, including color structures,
 * global variable declarations, and color management function declarations.
 */

#ifndef COLOR_H
#define COLOR_H

#include <windows.h>

/**
 * @brief Predefined color structure
 * 
 * Stores predefined color hexadecimal values
 */
typedef struct {
    const char* hexColor;  ///< Hexadecimal color value, format like "#RRGGBB"
} PredefinedColor;

/**
 * @brief CSS color name structure
 * 
 * Stores CSS standard color names and corresponding hexadecimal values
 */
typedef struct {
    const char* name;     ///< Color name, such as "red"
    const char* hex;      ///< Corresponding hexadecimal value, such as "#FF0000"
} CSSColor;

/// @name Global variable declarations
/// @{
extern PredefinedColor* COLOR_OPTIONS;   ///< Predefined color options array
extern size_t COLOR_OPTIONS_COUNT;       ///< Number of predefined color options
extern char PREVIEW_COLOR[10];           ///< Preview color value
extern BOOL IS_COLOR_PREVIEWING;         ///< Whether color is being previewed
extern char CLOCK_TEXT_COLOR[10];        ///< Current text color value
/// @}

/// @name Color management functions
/// @{

/**
 * @brief Initialize default language and color settings
 * 
 * Read color settings from configuration file, create default configuration if not found.
 */
void InitializeDefaultLanguage(void);

/**
 * @brief Add color option
 * @param hexColor Hexadecimal color value
 * 
 * Add a color option to the global color options array.
 */
void AddColorOption(const char* hexColor);

/**
 * @brief Clear all color options
 * 
 * Free memory occupied by all color options and reset color option count.
 */
void ClearColorOptions(void);

/**
 * @brief Write color to configuration file
 * @param color_input Color value to write
 * 
 * Write the specified color value to the application configuration file.
 */
void WriteConfigColor(const char* color_input);

/**
 * @brief Normalize color format
 * @param input Input color string
 * @param output Output normalized color string
 * @param output_size Output buffer size
 * 
 * Convert color strings in various formats to standard hexadecimal color format.
 */
void normalizeColor(const char* input, char* output, size_t output_size);

/**
 * @brief Check if color is valid
 * @param input Color string to check
 * @return BOOL Returns TRUE if color is valid, otherwise FALSE
 * 
 * Verify whether the given color representation can be parsed as a valid color.
 */
BOOL isValidColor(const char* input);

/**
 * @brief Color edit box subclass procedure
 * @param hwnd Window handle
 * @param msg Message ID
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return LRESULT Message processing result
 * 
 * Handle color edit box message events, implementing color preview functionality.
 */
LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Color settings dialog procedure
 * @param hwndDlg Dialog window handle
 * @param msg Message ID
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return INT_PTR Message processing result
 * 
 * Handle color settings dialog message events.
 */
INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Check if color already exists
 * @param hexColor Hexadecimal color value
 * @return BOOL Returns TRUE if color already exists, otherwise FALSE
 * 
 * Check if the specified color already exists in the color options list.
 */
BOOL IsColorExists(const char* hexColor);

/**
 * @brief Show color selection dialog
 * @param hwnd Parent window handle
 * @return COLORREF Selected color value, returns (COLORREF)-1 if user cancels
 * 
 * Display Windows standard color selection dialog, allowing users to select colors.
 */
COLORREF ShowColorDialog(HWND hwnd);

/**
 * @brief Color dialog hook procedure
 * @param hdlg Dialog window handle
 * @param msg Message ID
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return UINT_PTR Message processing result
 * 
 * Handle custom message events for the color selection dialog, implementing color preview functionality.
 */
UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
/// @}

#endif // COLOR_H