/**
 * @file color.h
 * @brief Color management and selection system
 * 
 * Functions for color validation, preview, selection, and configuration
 */

#ifndef COLOR_H
#define COLOR_H

#include <windows.h>

/**
 * @brief Predefined color option structure
 */
typedef struct {
    const char* hexColor;  /**< Hex color string (e.g., "#FF0000") */
} PredefinedColor;

/**
 * @brief CSS named color structure
 */
typedef struct {
    const char* name;  /**< Color name (e.g., "red") */
    const char* hex;   /**< Hex color value */
} CSSColor;

/** @brief Array of available color options */
extern PredefinedColor* COLOR_OPTIONS;

/** @brief Count of available color options */
extern size_t COLOR_OPTIONS_COUNT;

/** @brief Current preview color hex string */
extern char PREVIEW_COLOR[10];

/** @brief Color preview active state */
extern BOOL IS_COLOR_PREVIEWING;

/** @brief Current clock text color */
extern char CLOCK_TEXT_COLOR[10];

/**
 * @brief Initialize default language settings
 */
void InitializeDefaultLanguage(void);

/**
 * @brief Add color option to available choices
 * @param hexColor Hex color string to add
 */
void AddColorOption(const char* hexColor);

/**
 * @brief Clear all color options
 */
void ClearColorOptions(void);

/**
 * @brief Write color configuration to settings
 * @param color_input Color string to save
 */
void WriteConfigColor(const char* color_input);

/**
 * @brief Normalize color string to standard format
 * @param input Input color string
 * @param output Output buffer for normalized color
 * @param output_size Size of output buffer
 */
void normalizeColor(const char* input, char* output, size_t output_size);

/**
 * @brief Check if color string is valid
 * @param input Color string to validate
 * @return TRUE if valid, FALSE otherwise
 */
BOOL isValidColor(const char* input);

/**
 * @brief Subclass procedure for color input edit control
 * @param hwnd Control window handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 */
LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Dialog procedure for color configuration dialog
 * @param hwndDlg Dialog window handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 */
INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Check if color already exists in options
 * @param hexColor Hex color string to check
 * @return TRUE if exists, FALSE otherwise
 */
BOOL IsColorExists(const char* hexColor);

/**
 * @brief Show system color picker dialog
 * @param hwnd Parent window handle
 * @return Selected color or -1 if cancelled
 */
COLORREF ShowColorDialog(HWND hwnd);

/**
 * @brief Hook procedure for color dialog customization
 * @param hdlg Dialog handle
 * @param msg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Hook processing result
 */
UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

#endif