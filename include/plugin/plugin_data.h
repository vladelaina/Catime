/**
 * @file plugin_data.h
 * @brief Plugin data management using file monitoring
 */

#ifndef PLUGIN_DATA_H
#define PLUGIN_DATA_H

#include <windows.h>

/**
 * @brief Initialize plugin data subsystem
 * @param hwnd Main window handle for refresh notifications
 */
void PluginData_Init(HWND hwnd);

/**
 * @brief Shutdown plugin data subsystem
 */
void PluginData_Shutdown(void);

/**
 * @brief Get current plugin display text
 * @param buffer Output buffer
 * @param maxLen Buffer size
 * @return TRUE if data is available
 */
BOOL PluginData_GetText(wchar_t* buffer, size_t maxLen);

/**
 * @brief Get current plugin image path
 * @param buffer Output buffer
 * @param maxLen Buffer size
 * @return TRUE if image path is available
 */
BOOL PluginData_GetImagePath(wchar_t* buffer, size_t maxLen);

#endif /* PLUGIN_DATA_H */
