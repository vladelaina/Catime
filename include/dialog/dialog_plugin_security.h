/**
 * @file dialog_plugin_security.h
 * @brief Plugin security confirmation dialog
 */

#ifndef DIALOG_PLUGIN_SECURITY_H
#define DIALOG_PLUGIN_SECURITY_H

#include <windows.h>

/**
 * @brief Show plugin security confirmation dialog
 * @param hwndParent Parent window handle
 * @param pluginPath Full path to plugin file (must not be NULL)
 * @param pluginName Display name of plugin (must not be NULL)
 * @return IDYES (trust and remember), IDOK (run once), IDCANCEL (or on dialog failure)
 */
INT_PTR ShowPluginSecurityDialog(HWND hwndParent, const char* pluginPath, const char* pluginName);

#endif /* DIALOG_PLUGIN_SECURITY_H */
