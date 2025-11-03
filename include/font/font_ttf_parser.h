/**
 * @file font_ttf_parser.h
 * @brief TrueType/OpenType font binary parsing
 * 
 * Extracts metadata from TTF/OTF files without full font loading.
 * Uses direct binary parsing of font tables (big-endian format).
 */

#ifndef FONT_TTF_PARSER_H
#define FONT_TTF_PARSER_H

#include <windows.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum safe string length for TTF name table entries */
#define TTF_STRING_SAFETY_LIMIT 1024

/** @brief TTF 'name' table tag (little-endian representation of big-endian) */
#define TTF_NAME_TABLE_TAG 0x656D616E

/** @brief Font family name ID in TTF name table */
#define TTF_NAME_ID_FAMILY 1

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Extract font family name from TTF/OTF file
 * @param fontFilePath Path to font file (UTF-8)
 * @param fontName Output buffer for family name
 * @param fontNameSize Buffer size
 * @return TRUE on success, FALSE if not a valid TTF or parse error
 * 
 * @details
 * Parses the TTF 'name' table to extract font family name.
 * Prefers Windows Unicode (platform 3, encoding 1) if available.
 * Handles both TTF and OTF formats.
 */
BOOL GetFontNameFromFile(const char* fontFilePath, char* fontName, size_t fontNameSize);

#endif /* FONT_TTF_PARSER_H */

