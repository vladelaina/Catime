/**
 * @file config_defaults.h
 * @brief Configuration default values and metadata
 * 
 * Centralized definition of all configuration items with their default values.
 * This eliminates duplication across CreateDefaultConfig, ReadConfig, and WriteConfig.
 */

#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H

#include <windows.h>
#include "config.h"

/* ============================================================================
 * Configuration value types
 * ============================================================================ */

typedef enum {
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_INT,
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_ENUM
} ConfigValueType;

/* ============================================================================
 * Configuration metadata structure
 * ============================================================================ */

typedef struct {
    const char* section;          /**< INI section name */
    const char* key;              /**< INI key name */
    const char* defaultValue;     /**< Default value as string */
    ConfigValueType type;         /**< Value type for validation */
    const char* description;      /**< Human-readable description */
} ConfigItemMeta;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Create default configuration file with system language detection
 * @param config_path Path to config file (UTF-8)
 * 
 * @details
 * Detects system language and creates config.ini with appropriate defaults.
 * Calls WriteDefaultsToConfig internally.
 */
void CreateDefaultConfig(const char* config_path);

/**
 * @brief Write all default values to existing config file
 * @param config_path Path to config file (UTF-8)
 * 
 * @details
 * Used by CreateDefaultConfig and for resetting to defaults.
 * Preserves existing values not in the defaults list.
 */
void WriteDefaultsToConfig(const char* config_path);

/**
 * @brief Get default value for a specific config item
 * @param section Section name
 * @param key Key name
 * @return Default value string, or NULL if not found
 * 
 * @note Returned pointer is to static data, do not free
 */
const char* GetDefaultValue(const char* section, const char* key);

/**
 * @brief Get configuration metadata array
 * @param count Output parameter for array size
 * @return Pointer to metadata array
 * 
 * @note For iteration and bulk operations
 */
const ConfigItemMeta* GetConfigMetadata(int* count);

/**
 * @brief Detect system language for default configuration
 * @return Detected language enumeration value
 */
int DetectSystemLanguage(void);

#endif /* CONFIG_DEFAULTS_H */

