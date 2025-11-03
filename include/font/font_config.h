/**
 * @file font_config.h
 * @brief Font configuration persistence
 * 
 * Handles reading and writing font settings to config.ini
 */

#ifndef FONT_CONFIG_H
#define FONT_CONFIG_H

#include <windows.h>

/* ============================================================================
 * Configuration I/O
 * ============================================================================ */

/**
 * @brief Write font configuration to config file
 * @param fontFileName Font filename (relative or config-style path)
 * @param shouldReload TRUE to reload entire config after write
 * 
 * @details Writes to [Display] FONT_FILE_NAME in config.ini.
 *          Converts to config-style path if needed.
 */
void WriteConfigFont(const char* fontFileName, BOOL shouldReload);

#endif /* FONT_CONFIG_H */

