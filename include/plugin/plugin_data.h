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

/**
 * @brief Clear all plugin data
 */
void PluginData_Clear(void);

/**
 * @brief Set plugin display text directly (e.g., for "Loading..." message)
 * @param text Text to display
 */
void PluginData_SetText(const wchar_t* text);

/**
 * @brief Set plugin mode active state
 * @param active TRUE to enable plugin data display, FALSE to disable
 * 
 * When active is FALSE, PluginData_GetText/GetImagePath will return FALSE
 * even if the data file contains content. This prevents stale data from
 * previous plugin runs from being displayed on startup.
 */
void PluginData_SetActive(BOOL active);

/**
 * @brief Check if plugin mode is active
 * @return TRUE if plugin mode is active
 */
BOOL PluginData_IsActive(void);

#endif /* PLUGIN_DATA_H */
