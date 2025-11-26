/**
 * @file color_parser.h
 * @brief Pure color parsing and conversion algorithms (no dependencies)
 * 
 * Supports multiple input formats:
 * - CSS color names (case-insensitive): "red", "blue", "white", etc.
 * - Hex formats: #RGB, #RRGGBB (with or without #)
 * - RGB formats: rgb(255,0,0), "255,0,0", "255 0 0", etc.
 * 
 * All functions are pure (no global state), making them easily testable
 * and reusable in other projects.
 */

#ifndef COLOR_PARSER_H
#define COLOR_PARSER_H

#include <windows.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define COLOR_BUFFER_SIZE 64
#define COLOR_HEX_BUFFER 64
#define HEX_COLOR_LENGTH 7

/* ============================================================================
 * Core Parsing Functions
 * ============================================================================ */

/**
 * @brief Convert any supported format to canonical #RRGGBB hex
 * @param input Color in any format (CSS name, hex, RGB)
 * @param output Buffer for normalized hex (min 10 bytes)
 * @param output_size Buffer size
 * 
 * @details
 * Supports multiple RGB separators (comma, Chinese comma, semicolon, pipe)
 * for international keyboards. Returns input unchanged on failure.
 * 
 * @example
 * normalizeColor("red", buf, size)        → "#FF0000"
 * normalizeColor("#f00", buf, size)       → "#FF0000"
 * normalizeColor("rgb(255,0,0)", buf, size) → "#FF0000"
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
 * @brief Convert Windows COLORREF to hex string
 * @param color COLORREF value
 * @param output Buffer for hex string (min 10 bytes)
 * @param size Buffer size
 * 
 * @example ColorRefToHex(RGB(255,0,0), buf, size) → "#FF0000"
 */
void ColorRefToHex(COLORREF color, char* output, size_t size);

/**
 * @brief Replace pure black with near-black to prevent invisible text
 * @param color Input color
 * @param output Output buffer
 * @param output_size Buffer size
 * 
 * @details #000000 → #000001 (visually indistinguishable)
 */
void ReplaceBlackColor(const char* color, char* output, size_t output_size);

#endif /* COLOR_PARSER_H */

