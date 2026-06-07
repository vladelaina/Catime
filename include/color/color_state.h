/**
 * @file color_state.h
 * @brief Color state management and configuration persistence
 * 
 * Centralizes all color-related global state and provides accessor functions
 * to control state modifications. Handles configuration file I/O.
 */

#ifndef COLOR_STATE_H
#define COLOR_STATE_H

#include <windows.h>
#include "color_parser.h"

/* ============================================================================
 * Type definitions
 * ============================================================================ */

/**
 * @brief User's saved color palette entry
 */
typedef struct {
    const char* hexColor;
} PredefinedColor;

/** @brief Maximum saved palette entries shown in the dynamic color menu */
#define MAX_COLOR_OPTIONS 256

/* ============================================================================
 * Global state variables (use accessors instead of direct access)
 * ============================================================================ */

/** @brief Dynamically allocated palette */
extern PredefinedColor* COLOR_OPTIONS;

/** @brief Palette size (tracked separately due to reallocation) */
extern size_t COLOR_OPTIONS_COUNT;

/** @brief Active clock color */
extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];

/* ============================================================================
 * Color Palette Management
 * ============================================================================ */

/**
 * @brief Loads saved color palette or initializes defaults
 * 
 * @details
 * Handles missing '#' prefix for backward compatibility.
 * Must be called before showing color UI.
 */
void LoadColorConfig(void);

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
 * @brief Safely replace the runtime palette from a config string
 * @param color_options Comma-separated color/gradient list
 * @return TRUE if a non-empty validated palette replaced the current one
 *
 * @details Parses into temporary storage first. The active palette is left
 * unchanged if parsing produces no valid colors or memory allocation fails.
 */
BOOL ReplaceColorOptionsFromConfigValue(const char* color_options);

/* ============================================================================
 * Configuration Persistence
 * ============================================================================ */

/**
 * @brief Normalize a color value before persisting or previewing
 * @param color_input Color string to normalize
 * @param outValue Output buffer for normalized value
 * @param outSize Output buffer size
 * @return TRUE on success, FALSE if the input is invalid or too large
 */
BOOL NormalizeColorConfigValue(const char* color_input, char* outValue, size_t outSize);

/**
 * @brief Persists current color to config (auto-normalized)
 * @param color_input Color string to save
 * @return TRUE if the runtime state and config are synchronized
 * 
 * @details Immediate write ensures persistence even on crash.
 */
BOOL WriteConfigColor(const char* color_input);

/**
 * @brief Persist and apply the quick color palette atomically
 * @param color_options Comma-separated color/gradient list
 * @return TRUE if config and runtime palette are synchronized
 */
BOOL WriteConfigColorOptions(const char* color_options);

#endif /* COLOR_STATE_H */
