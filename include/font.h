/**
 * @file font.h
 * @brief Font system unified interface (aggregates all font modules)
 * 
 * This header aggregates the refactored font system modules:
 * - font_manager: Font loading, preview, GDI resource management
 * - font_ttf_parser: Binary TTF parsing for metadata extraction
 * - font_path_manager: Path resolution and auto-recovery
 * - font_config: Configuration persistence
 * 
 * Include this single header to access all font functionality.
 */

#ifndef FONT_H
#define FONT_H

#include "font/font_manager.h"
#include "font/font_ttf_parser.h"
#include "font/font_path_manager.h"
#include "font/font_config.h"

#endif /* FONT_H */
