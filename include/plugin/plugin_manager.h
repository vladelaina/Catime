/**
 * @file plugin_manager.h
 * @brief Plugin manager for loading and managing external plugins
 */

#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include <windows.h>

/* Maximum number of plugins */
#define MAX_PLUGINS 32

/* Plugin directory path */
#define PLUGIN_DIR_PATH "%LOCALAPPDATA%\\Catime\\resources\\plugins"

/* Plugin info structure */
typedef struct {
    wchar_t name[64];           // Plugin name (e.g., L"catime_monitor.exe")
    wchar_t displayName[64];    // Display name (e.g., L"Monitor") - supports Unicode
    wchar_t path[MAX_PATH];     // Full path to executable - supports Unicode
    BOOL isRunning;             // Is plugin currently running
    PROCESS_INFORMATION pi;     // Process information
    FILETIME lastModTime;       // Last modification time for hot-reload detection
} PluginInfo;

/**
 * @brief Initialize plugin manager
 */
void PluginManager_Init(void);

/**
 * @brief Shutdown plugin manager (stop all plugins)
 */
void PluginManager_Shutdown(void);

/**
 * @brief Scan plugin directory and load plugin list
 * @return Number of plugins found
 */
int PluginManager_ScanPlugins(void);

/**
 * @brief Request async plugin scan (non-blocking)
 * Scan runs in background thread, results available on next menu open
 */
void PluginManager_RequestScanAsync(void);

/**
 * @brief Get plugin count
 * @return Number of plugins
 */
int PluginManager_GetPluginCount(void);

/**
 * @brief Get plugin info by index
 * @param index Plugin index (0-based)
 * @return Pointer to plugin info, or NULL if invalid index
 */
const PluginInfo* PluginManager_GetPlugin(int index);

/**
 * @brief Start a plugin
 * @param index Plugin index
 * @return TRUE if started successfully, FALSE if security dialog shown or failed
 * @note If plugin is not trusted, a modeless security dialog is shown and
 *       the actual start happens via WM_DIALOG_PLUGIN_SECURITY message handler
 */
BOOL PluginManager_StartPlugin(int index);

/**
 * @brief Start plugin after security dialog confirmation
 * @param index Plugin index
 * @param trustPlugin TRUE if user chose "Trust & Run", FALSE for "Run Once"
 * @return TRUE if plugin started successfully
 */
BOOL PluginManager_StartPluginAfterSecurityCheck(int index, BOOL trustPlugin);

/**
 * @brief Stop a plugin
 * @param index Plugin index
 * @return TRUE if stopped successfully
 */
BOOL PluginManager_StopPlugin(int index);

/**
 * @brief Toggle plugin (start if stopped, stop if running)
 * @param index Plugin index
 * @return TRUE if operation successful
 */
BOOL PluginManager_TogglePlugin(int index);

/**
 * @brief Check if plugin is running
 * @param index Plugin index
 * @return TRUE if running
 */
BOOL PluginManager_IsPluginRunning(int index);

/**
 * @brief Check if plugin needs security confirmation before starting
 * @param index Plugin index
 * @return TRUE if security dialog should be shown, FALSE if plugin is trusted
 */
BOOL PluginManager_NeedsSecurityCheck(int index);

/**
 * @brief Get currently active plugin index
 * @return Active plugin index, or -1 if none
 */
int PluginManager_GetActivePluginIndex(void);

/**
 * @brief Get plugin directory path (expanded)
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @return TRUE if successful
 */
BOOL PluginManager_GetPluginDir(char* buffer, size_t bufferSize);

/**
 * @brief Stop all plugins
 */
void PluginManager_StopAllPlugins(void);

/**
 * @brief Open plugin folder in File Explorer
 * @return TRUE if successful
 */
BOOL PluginManager_OpenPluginFolder(void);

/**
 * @brief Set window handle for plugin hot-reload notifications
 * @param hwnd Window handle to receive redraw notifications
 */
void PluginManager_SetNotifyWindow(HWND hwnd);

#endif /* PLUGIN_MANAGER_H */
