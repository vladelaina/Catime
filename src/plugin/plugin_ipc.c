/**
 * @file plugin_ipc.c
 * @brief Plugin IPC implementation with JSON support
 */

#include "plugin/plugin_ipc.h"
#include "log.h"
#include "cJSON.h"
#include <string.h>

/* ============================================================================
 * Plugin Data Storage
 * ============================================================================ */

static wchar_t g_pluginDisplayText[128] = {0};
static BOOL g_hasPluginData = FALSE;
static DWORD g_currentPluginType = 0;
static CRITICAL_SECTION g_pluginCS;

/* ============================================================================
 * Initialization and Shutdown
 * ============================================================================ */

void PluginIPC_Init(void) {
    InitializeCriticalSection(&g_pluginCS);
    g_hasPluginData = FALSE;
    g_currentPluginType = 0;
    memset(g_pluginDisplayText, 0, sizeof(g_pluginDisplayText));
    LOG_INFO("Plugin IPC subsystem initialized");
}

void PluginIPC_Shutdown(void) {
    EnterCriticalSection(&g_pluginCS);
    g_hasPluginData = FALSE;
    g_currentPluginType = 0;
    memset(g_pluginDisplayText, 0, sizeof(g_pluginDisplayText));
    LeaveCriticalSection(&g_pluginCS);
    DeleteCriticalSection(&g_pluginCS);
    LOG_INFO("Plugin IPC subsystem shutdown");
}

/* ============================================================================
 * Message Handling
 * ============================================================================ */

/**
 * @brief Parse JSON message and extract text field
 * @param jsonStr JSON string
 * @param outText Output buffer for text
 * @param maxLen Maximum length of output buffer
 * @return TRUE if parsed successfully
 */
static BOOL ParseJSONMessage(const char* jsonStr, wchar_t* outText, size_t maxLen) {
    cJSON* root = cJSON_Parse(jsonStr);
    if (!root) {
        LOG_WARNING("Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return FALSE;
    }

    // Extract "text" field
    cJSON* textItem = cJSON_GetObjectItem(root, "text");
    if (!textItem || !cJSON_IsString(textItem)) {
        LOG_WARNING("JSON missing 'text' field or not a string");
        cJSON_Delete(root);
        return FALSE;
    }

    // Convert UTF-8 to wide string
    const char* text = cJSON_GetStringValue(textItem);
    if (text) {
        MultiByteToWideChar(CP_UTF8, 0, text, -1, outText, (int)maxLen);
        outText[maxLen - 1] = L'\0';
    }

    cJSON_Delete(root);
    return (text != NULL);
}

BOOL PluginIPC_HandleMessage(HWND hwnd, PCOPYDATASTRUCT pcds) {
    if (!pcds || !pcds->lpData || pcds->cbData == 0) {
        return FALSE;
    }

    // Verify magic number
    if ((pcds->dwData & 0xFFFF0000) != CATIME_IPC_MAGIC_BASE) {
        return FALSE;
    }

    // Validate data size
    if (pcds->cbData > PLUGIN_MAX_DATA_SIZE) {
        LOG_WARNING("Plugin message too large: %lu bytes (max %d)",
                   pcds->cbData, PLUGIN_MAX_DATA_SIZE);
        return FALSE;
    }

    // Extract plugin type
    DWORD pluginType = pcds->dwData & 0x0000FFFF;

    EnterCriticalSection(&g_pluginCS);

    // Parse JSON message
    BOOL success = ParseJSONMessage((const char*)pcds->lpData,
                                   g_pluginDisplayText, 128);

    if (success) {
        g_hasPluginData = TRUE;
        g_currentPluginType = pluginType;
        LOG_INFO("JSON message received (plugin: 0x%04X): %ls",
                pluginType, g_pluginDisplayText);
    } else {
        LOG_WARNING("Failed to parse JSON message from plugin 0x%04X", pluginType);
    }

    LeaveCriticalSection(&g_pluginCS);

    // Trigger redraw if successful
    if (success) {
        InvalidateRect(hwnd, NULL, FALSE);
    }

    return success;
}

/* ============================================================================
 * Data Access
 * ============================================================================ */

BOOL PluginIPC_GetDisplayText(wchar_t* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0) {
        return FALSE;
    }

    BOOL hasData = FALSE;

    EnterCriticalSection(&g_pluginCS);
    
    if (g_hasPluginData && wcslen(g_pluginDisplayText) > 0) {
        wcsncpy(buffer, g_pluginDisplayText, maxLen - 1);
        buffer[maxLen - 1] = L'\0';
        hasData = TRUE;
    }

    LeaveCriticalSection(&g_pluginCS);

    return hasData;
}

BOOL PluginIPC_HasData(void) {
    BOOL hasData;
    
    EnterCriticalSection(&g_pluginCS);
    hasData = g_hasPluginData;
    LeaveCriticalSection(&g_pluginCS);

    return hasData;
}

void PluginIPC_ClearData(void) {
    EnterCriticalSection(&g_pluginCS);
    
    g_hasPluginData = FALSE;
    g_currentPluginType = 0;
    memset(g_pluginDisplayText, 0, sizeof(g_pluginDisplayText));

    LeaveCriticalSection(&g_pluginCS);

    LOG_INFO("Plugin data cleared");
}

DWORD PluginIPC_GetCurrentPluginType(void) {
    DWORD type;
    
    EnterCriticalSection(&g_pluginCS);
    type = g_currentPluginType;
    LeaveCriticalSection(&g_pluginCS);

    return type;
}
