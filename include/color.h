/**
 * @file color.h
 * @brief Unified color management interface
 * 
 * This header aggregates all color-related functionality:
 * - Color parsing and validation (color_parser.h)
 * - State management and persistence (color_state.h)
 * - System color picker dialog (color_dialog.h)
 * - Text input dialog (color_input_dialog.h)
 * 
 * Include this file for backward compatibility with existing code.
 * New code should include specific headers as needed.
 */

#ifndef COLOR_H
#define COLOR_H

#include <windows.h>

/* ============================================================================
 * Aggregate all color module headers
 * ============================================================================ */

#include "color_parser.h"
#include "color_state.h"
#include "color_dialog.h"
#include "color_input_dialog.h"

/* ============================================================================
 * Backward compatibility aliases
 * ============================================================================ */

/**
 * @brief Historical name for LoadColorConfig()
 * @deprecated Use LoadColorConfig() directly
 * 
 * @note Function name is historical artifact - actually initializes colors
 */
static inline void InitializeDefaultLanguage(void) {
    LoadColorConfig();
}

#endif /* COLOR_H */