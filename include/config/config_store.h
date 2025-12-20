/**
 * @file config_store.h
 * @brief Unified configuration store
 *
 * Single source of truth for all configuration.
 * Memory-first design with lazy disk I/O.
 */

#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <windows.h>
#include "config_keys.h"

/* ============================================================================
 * Configuration access API
 * ============================================================================ */

/**
 * @brief Initialize configuration system
 * @param config_path Path to config file (UTF-8)
 *
 * Loads config from disk if exists, otherwise uses defaults.
 * Must be called before any other config functions.
 */
void Config_Init(const char* config_path);

/**
 * @brief Shutdown configuration system
 *
 * Flushes any pending changes to disk and frees resources.
 */
void Config_Shutdown(void);

/**
 * @brief Get string value
 * @param key Configuration key
 * @return String value (do not free, valid until next Set or Shutdown)
 */
const char* Config_GetString(ConfigKey key);

/**
 * @brief Get integer value
 * @param key Configuration key
 * @return Integer value
 */
int Config_GetInt(ConfigKey key);

/**
 * @brief Get boolean value
 * @param key Configuration key
 * @return Boolean value
 */
BOOL Config_GetBool(ConfigKey key);

/**
 * @brief Get float value
 * @param key Configuration key
 * @return Float value
 */
float Config_GetFloat(ConfigKey key);

/**
 * @brief Set string value
 * @param key Configuration key
 * @param value New value (will be copied)
 */
void Config_SetString(ConfigKey key, const char* value);

/**
 * @brief Set integer value
 * @param key Configuration key
 * @param value New value
 */
void Config_SetInt(ConfigKey key, int value);

/**
 * @brief Set boolean value
 * @param key Configuration key
 * @param value New value
 */
void Config_SetBool(ConfigKey key, BOOL value);

/**
 * @brief Set float value
 * @param key Configuration key
 * @param value New value
 */
void Config_SetFloat(ConfigKey key, float value);

/**
 * @brief Flush pending changes to disk
 *
 * Normally changes are batched. Call this to force immediate write.
 */
void Config_Flush(void);

/**
 * @brief Reload configuration from disk
 *
 * Discards any unsaved changes and reloads from file.
 */
void Config_Reload(void);

/**
 * @brief Mark config as dirty (needs save)
 *
 * Called automatically by Set functions.
 */
void Config_MarkDirty(void);

/**
 * @brief Check if config has unsaved changes
 * @return TRUE if dirty
 */
BOOL Config_IsDirty(void);

/**
 * @brief Get config file path
 * @return Path string (do not free)
 */
const char* Config_GetPath(void);

/**
 * @brief Reset a key to its default value
 * @param key Configuration key
 */
void Config_ResetToDefault(ConfigKey key);

/**
 * @brief Reset all configuration to defaults
 */
void Config_ResetAll(void);

#endif /* CONFIG_STORE_H */
