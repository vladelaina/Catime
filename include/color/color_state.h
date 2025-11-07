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
    const char* hexColor;  /**< Normalized hex format for consistent comparison */
} PredefinedColor;

/* ============================================================================
 * Global state variables (use accessors instead of direct access)
 * ============================================================================ */

/** @brief Dynamically allocated palette (supports unlimited colors) */
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

/* ============================================================================
 * Configuration Persistence
 * ============================================================================ */

/**
 * @brief Persists current color to config (auto-normalized)
 * @param color_input Color string to save
 * 
 * @details Immediate write ensures persistence even on crash.
 */
void WriteConfigColor(const char* color_input);

#endif /* COLOR_STATE_H */

