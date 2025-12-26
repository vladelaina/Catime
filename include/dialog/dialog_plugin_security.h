/**
 * @file dialog_plugin_security.h
 * @brief Plugin security confirmation dialog (modeless)
 */

#ifndef DIALOG_PLUGIN_SECURITY_H
#define DIALOG_PLUGIN_SECURITY_H

#include <windows.h>

/**
 * @brief Show plugin security confirmation dialog (modeless)
 * @param hwndParent Parent window handle
 * @param pluginPath Full path to plugin file (must not be NULL)
 * @param pluginName Display name of plugin (must not be NULL)
 * @param pluginIndex Index of plugin in plugin list
 * 
 * Results are sent via WM_DIALOG_PLUGIN_SECURITY message:
 * - wParam = IDYES (trust and remember)
 * - wParam = IDOK (run once)
 * - wParam = IDCANCEL (cancelled)
 */
void ShowPluginSecurityDialog(HWND hwndParent, const char* pluginPath, const char* pluginName, int pluginIndex);

/**
 * @brief Get pending plugin path (for result handling)
 */
const char* GetPendingPluginPath(void);

/**
 * @brief Get pending plugin index (for result handling)
 */
int GetPendingPluginIndex(void);

/**
 * @brief Clear pending plugin info after handling result
 */
void ClearPendingPluginInfo(void);

#endif /* DIALOG_PLUGIN_SECURITY_H */
