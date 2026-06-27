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
 * @brief Use the launched plugin's folder as the output.txt directory
 * @param pluginPath Full path to the plugin script
 */
void PluginData_SetOutputDirectoryFromPluginPath(const wchar_t* pluginPath);

/**
 * @brief Get the current output.txt path used by plugin mode
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @return TRUE if path was copied
 */
BOOL PluginData_GetOutputPath(wchar_t* buffer, size_t bufferSize);

/**
 * @brief Get the source file path for the content currently being displayed
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @return TRUE if path was copied
 *
 * @details Returns plugin output.txt in plugin mode and the dedicated
 * custom_display.txt path for custom text display. Markdown checkboxes and
 * relative images should use this instead of PluginData_GetOutputPath().
 */
BOOL PluginData_GetDisplaySourcePath(wchar_t* buffer, size_t bufferSize);

/**
 * @brief Set plugin display text directly (e.g., for "Loading..." message)
 * @param text Text to display
 */
void PluginData_SetText(const wchar_t* text);

/**
 * @brief Set status text without starting the output-file watcher
 * @param text Text to display
 */
void PluginData_SetStatusText(const wchar_t* text);

/**
 * @brief Preview manually supplied display text using the plugin display parser
 * @param text UTF-16 text to parse and display
 * @return TRUE if the preview text was accepted
 *
 * @details Used by the custom text display UI. Side-effect tags such as
 * <notify> and <exit> are rendered or removed without triggering their
 * plugin-runtime effects.
 */
BOOL PluginData_SetPreviewText(const wchar_t* text);

/**
 * @brief Preview manually supplied display text with a backing source file
 * @param text UTF-16 text to parse and display
 * @param sourcePath File path that backs the displayed text
 * @return TRUE if the preview text was accepted
 */
BOOL PluginData_SetPreviewTextWithSource(const wchar_t* text, const wchar_t* sourcePath);

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
 * @brief Process pending notification from plugin
 * @param hwnd Window handle for notification display
 * 
 * Called from main thread in response to WM_PLUGIN_NOTIFY message.
 * This ensures UI operations happen on the main thread.
 */
void PluginData_ProcessPendingNotification(HWND hwnd);

/**
 * @brief Handle coalesced plugin data redraw on the UI thread
 * @param hwnd Main window handle
 */
void PluginData_HandleRedrawRequest(HWND hwnd);

/** @brief Custom message ID for plugin notifications */
#define WM_PLUGIN_NOTIFY (WM_APP + 200)

#endif /* PLUGIN_DATA_H */
