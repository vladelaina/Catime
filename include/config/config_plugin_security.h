/**
 * @file config_plugin_security.h
 * @brief Plugin security and trust management
 */

#ifndef CONFIG_PLUGIN_SECURITY_H
#define CONFIG_PLUGIN_SECURITY_H

#include <windows.h>

/**
 * @brief Check if a plugin is trusted
 * @param pluginPath Path to plugin file
 * @return TRUE if trusted and hash matches
 */
BOOL IsPluginTrusted(const char* pluginPath);

/**
 * @brief Add plugin to trust list
 * @param pluginPath Path to plugin file
 * @return TRUE if successful
 */
BOOL TrustPlugin(const char* pluginPath);

/**
 * @brief Remove plugin from trust list
 * @param pluginPath Path to plugin file
 * @return TRUE if successful
 */
BOOL UntrustPlugin(const char* pluginPath);

/**
 * @brief Load plugin trust state from config
 */
void LoadPluginTrustFromConfig(void);

/**
 * @brief Cleanup plugin trust resources
 */
void CleanupPluginTrustCS(void);

#endif /* CONFIG_PLUGIN_SECURITY_H */
