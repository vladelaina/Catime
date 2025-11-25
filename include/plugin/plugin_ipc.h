/**
 * @file plugin_ipc.h
 * @brief Plugin IPC (Inter-Process Communication) module
 *
 * This module handles all communication between Catime and external plugins
 * using the WM_COPYDATA message protocol.
 */

#ifndef PLUGIN_IPC_H
#define PLUGIN_IPC_H

#include <windows.h>

/* ============================================================================
 * Plugin IPC Protocol Constants
 * ============================================================================ */

/**
 * @brief Magic number for all Catime plugin messages (JSON format)
 * @note Format: 0xCA71 (upper 16 bits) + plugin type (lower 16 bits)
 */
#define CATIME_IPC_MAGIC_BASE   0xCA710000

/**
 * @brief Plugin type identifiers
 */
#define CATIME_PLUGIN_MONITOR   0x0001
#define CATIME_PLUGIN_WEATHER   0x0002
#define CATIME_PLUGIN_SYSTEM    0x0003

/**
 * @brief Complete magic numbers for each plugin type
 */
#define CATIME_IPC_MONITOR      (CATIME_IPC_MAGIC_BASE | CATIME_PLUGIN_MONITOR)
#define CATIME_IPC_WEATHER      (CATIME_IPC_MAGIC_BASE | CATIME_PLUGIN_WEATHER)
#define CATIME_IPC_SYSTEM       (CATIME_IPC_MAGIC_BASE | CATIME_PLUGIN_SYSTEM)

/**
 * @brief Maximum data size for plugin messages (bytes)
 */
#define PLUGIN_MAX_DATA_SIZE    256

/* ============================================================================
 * Plugin Data Management
 * ============================================================================ */

/**
 * @brief Initialize plugin IPC subsystem
 */
void PluginIPC_Init(void);

/**
 * @brief Shutdown plugin IPC subsystem
 */
void PluginIPC_Shutdown(void);

/**
 * @brief Handle WM_COPYDATA message from plugins
 * @param hwnd Window handle
 * @param pcds Pointer to COPYDATASTRUCT
 * @return TRUE if message was handled, FALSE otherwise
 */
BOOL PluginIPC_HandleMessage(HWND hwnd, PCOPYDATASTRUCT pcds);

/**
 * @brief Get current plugin display text
 * @param buffer Output buffer for text
 * @param maxLen Maximum buffer length
 * @return TRUE if plugin data is available, FALSE otherwise
 */
BOOL PluginIPC_GetDisplayText(wchar_t* buffer, size_t maxLen);

/**
 * @brief Check if plugin data is available
 * @return TRUE if plugin has sent data, FALSE otherwise
 */
BOOL PluginIPC_HasData(void);

/**
 * @brief Clear plugin data
 */
void PluginIPC_ClearData(void);

/**
 * @brief Get plugin type of current data
 * @return Plugin type identifier (CATIME_PLUGIN_*)
 */
DWORD PluginIPC_GetCurrentPluginType(void);

#endif /* PLUGIN_IPC_H */
