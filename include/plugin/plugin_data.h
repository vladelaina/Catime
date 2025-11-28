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
 * When active is FALSE, PluginData_GetText will return FALSE
 * even if the data file contains content. This prevents stale data from
 * previous plugin runs from being displayed on startup.
 */
void PluginData_SetActive(BOOL active);

/**
 * @brief Check if plugin mode is active
 * @return TRUE if plugin mode is active
 */
BOOL PluginData_IsActive(void);

/**
 * @brief Check if plugin text contains <catime> tag
 * @return TRUE if <catime></catime> tag is present in plugin text
 * 
 * When TRUE, timer mode switches should not clear plugin data,
 * as the time will be embedded within the plugin text.
 */
BOOL PluginData_HasCatimeTag(void);

/**
 * @brief Check and handle <exit> tag in plugin output
 * 
 * Syntax:
 * - <exit></exit> - Exit after 3 seconds (default)
 * - <exit>5</exit> - Exit after 5 seconds
 * 
 * During countdown, the <exit>N</exit> tag is replaced with the countdown number.
 * Example: "Exiting in <exit>3</exit>s" becomes "Exiting in 3s", "Exiting in 2s", etc.
 * After countdown completes, stops the plugin and exits plugin mode.
 * Invalid values (non-positive or non-numeric) will show raw content for debugging.
 */
void PluginData_CheckExitTag(void);

/**
 * @brief Cancel any pending exit countdown
 */
void PluginData_CancelExit(void);

#endif /* PLUGIN_DATA_H */
