/**
 * @file plugin_process.h
 * @brief Plugin process lifecycle management (internal)
 */

#ifndef PLUGIN_PROCESS_H
#define PLUGIN_PROCESS_H

#include "plugin/plugin_manager.h"
#include <windows.h>

/**
 * @brief Initialize process management (Job Object)
 * @return TRUE if successful
 */
BOOL PluginProcess_Init(void);

/**
 * @brief Shutdown process management
 */
void PluginProcess_Shutdown(void);

/**
 * @brief Launch a plugin script using ShellExecute
 * @param plugin Plugin info to launch
 * @return TRUE if launched successfully
 */
BOOL PluginProcess_Launch(PluginInfo* plugin);

/**
 * @brief Get last launch error message
 * @return Error message (empty string if no error)
 */
const wchar_t* PluginProcess_GetLastError(void);

/**
 * @brief Terminate a running plugin
 * @param plugin Plugin info to terminate
 * @return TRUE if terminated successfully
 */
BOOL PluginProcess_Terminate(PluginInfo* plugin);

/**
 * @brief Check if process is still running
 * @param plugin Plugin info to check
 * @return TRUE if running
 */
BOOL PluginProcess_IsAlive(PluginInfo* plugin);

/**
 * @brief Set callback window for notifications
 * @param hwnd Window handle
 */
void PluginProcess_SetNotifyWindow(HWND hwnd);

/**
 * @brief Get the notify window handle
 * @return Window handle
 */
HWND PluginProcess_GetNotifyWindow(void);

#endif /* PLUGIN_PROCESS_H */
