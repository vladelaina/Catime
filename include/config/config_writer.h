/**
 * @file config_writer.h
 * @brief Configuration writer - collect and save current state
 * 
 * Collects current configuration from global variables and writes to INI file.
 * Separated from reading/applying logic for modularity.
 */

#ifndef CONFIG_WRITER_H
#define CONFIG_WRITER_H

#include <windows.h>
#include "config.h"

/* ============================================================================
 * Configuration item structure for batch writing
 * ============================================================================ */

typedef struct {
    char section[64];
    char key[64];
    char value[256];
} ConfigWriteItem;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Write complete current configuration to file
 * @param config_path Path to config.ini (UTF-8)
 * 
 * @details
 * Collects all current configuration from global variables and writes to INI.
 * Uses atomic write (temp file + rename) for crash safety.
 * 
 * Automatically filters dangerous timeout actions (SHUTDOWN/RESTART/SLEEP → MESSAGE).
 */
void WriteConfig(const char* config_path);

/**
 * @brief Collect all current configuration into item array
 * @param items Output array (must be pre-allocated, min 100 items)
 * @param count Output item count
 * @return TRUE on success, FALSE on error
 * 
 * @details
 * Reads all global configuration variables and converts to ConfigWriteItem array.
 * Useful for batch operations or selective writing.
 */
BOOL CollectCurrentConfig(ConfigWriteItem* items, int* count);

/**
 * @brief Write array of configuration items to file
 * @param config_path Path to config.ini (UTF-8)
 * @param items Array of items to write
 * @param count Number of items
 * @return TRUE on success, FALSE on error
 * 
 * @details
 * Batch writes items to INI file.
 * Does not use atomic write (use WriteConfig for that).
 */
BOOL WriteConfigItems(const char* config_path, const ConfigWriteItem* items, int count);

/**
 * @brief Write specific section only
 * @param config_path Path to config.ini (UTF-8)
 * @param section Section name to write
 * 
 * @details
 * Selectively updates one section (e.g., Timer, Display).
 * Uses atomic update for the specific keys.
 */
void WriteConfigSection(const char* config_path, const char* section);

#endif /* CONFIG_WRITER_H */

