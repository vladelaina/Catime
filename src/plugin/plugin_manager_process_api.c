/**
 * @file plugin_manager_process_api.c
 * @brief Plugin launch and security-check entry points.
 */

#include "plugin_manager_internal.h"

BOOL StartPluginWithExpectedPath(int index, const wchar_t* expectedPath) {
    if (!g_pluginManagerInitialized) return FALSE;

    wchar_t pluginPath[MAX_PATH];
    wchar_t pluginDisplayName[64];
    BOOL alreadyRunning = FALSE;

    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    if (expectedPath && wcscmp(g_plugins[index].path, expectedPath) != 0) {
        LOG_WARNING("Plugin changed before start; start cancelled");
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    /* Copy plugin info before doing file hashing/trust checks outside the lock. */
    alreadyRunning = g_plugins[index].isRunning;
    wcsncpy(pluginPath, g_plugins[index].path, MAX_PATH - 1);
    pluginPath[MAX_PATH - 1] = L'\0';
    wcsncpy(pluginDisplayName, g_plugins[index].displayName, 63);
    pluginDisplayName[63] = L'\0';

    LeaveCriticalSection(&g_pluginCS);

    if (alreadyRunning) {
        return TRUE;
    }
    /* Convert to UTF-8 for security check functions */
    char pluginPathUtf8[MAX_PATH];
    if (!WideToUtf8Fixed(pluginPath, pluginPathUtf8, MAX_PATH)) {
        LOG_ERROR("Failed to convert plugin path to UTF-8: %ls", pluginPath);
        return FALSE;
    }

    /* Security check: verify plugin trust before launching */
    if (!IsPluginTrusted(pluginPathUtf8)) {
        LOG_INFO("Plugin not trusted, showing security dialog: %ls", pluginDisplayName);

        /* Show modeless security confirmation dialog */
        HWND hwnd = PluginProcess_GetNotifyWindow();
        if (IsPluginSecurityDialogOpen()) {
            ShowPluginSecurityDialog(hwnd, pluginPathUtf8, "", index, "");
            return FALSE;
        }

        /* Calculate and save hash at dialog show time for later verification */
        char pluginHash[65] = {0};
        if (!CalculatePluginHash(pluginPathUtf8, pluginHash)) {
            LOG_ERROR("Failed to calculate plugin hash for security dialog");
        }

        char displayNameUtf8[128];
        if (!WideToUtf8Fixed(pluginDisplayName, displayNameUtf8, 128)) {
            LOG_ERROR("Failed to convert plugin display name to UTF-8: %ls", pluginDisplayName);
            return FALSE;
        }
        ShowPluginSecurityDialog(hwnd, pluginPathUtf8, displayNameUtf8, index, pluginHash);

        /* Return FALSE - plugin will be started via WM_DIALOG_PLUGIN_SECURITY message handler */
        return FALSE;
    }

    /* Plugin is trusted, launch directly */
    BOOL result = LaunchPreparedPlugin(index, pluginPath);
    if (result && PluginManager_IsPluginRunning(index)) {
        UpdatePluginLastModTimeIfCurrent(index, pluginPath);
        StartHotReloadIfNeeded();
    }
    return result;
}

BOOL PluginManager_StartPlugin(int index) {
    return StartPluginWithExpectedPath(index, NULL);
}

/**
 * @brief Start plugin after security dialog confirmation
 * @param index Plugin index
 * @param trustPlugin TRUE if user chose "Trust & Run", FALSE for "Run Once"
 * @return TRUE if plugin started successfully
 */
BOOL PluginManager_StartPluginAfterSecurityCheck(int index, BOOL trustPlugin) {
    if (!g_pluginManagerInitialized) return FALSE;

    char expectedPluginPathUtf8[MAX_PATH] = {0};
    const char* pendingPluginPath = GetPendingPluginPath();
    if (pendingPluginPath) {
        strncpy(expectedPluginPathUtf8, pendingPluginPath, sizeof(expectedPluginPathUtf8) - 1);
        expectedPluginPathUtf8[sizeof(expectedPluginPathUtf8) - 1] = '\0';
    }

    EnterCriticalSection(&g_pluginCS);

    if (index < 0 || index >= g_pluginCount) {
        LOG_ERROR("Plugin index invalid after security dialog");
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    wchar_t pluginPath[MAX_PATH];
    wchar_t pluginDisplayName[64];
    wcsncpy(pluginPath, g_plugins[index].path, MAX_PATH - 1);
    pluginPath[MAX_PATH - 1] = L'\0';
    wcsncpy(pluginDisplayName, g_plugins[index].displayName, 63);
    pluginDisplayName[63] = L'\0';

    char pluginPathUtf8[MAX_PATH];
    if (!WideToUtf8Fixed(pluginPath, pluginPathUtf8, MAX_PATH)) {
        LOG_ERROR("Failed to convert plugin path to UTF-8: %ls", pluginPath);
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    if (expectedPluginPathUtf8[0] != '\0' &&
        _stricmp(expectedPluginPathUtf8, pluginPathUtf8) != 0) {
        LOG_WARNING("Plugin index changed during security dialog; launch cancelled");
        PluginProcess_SetLastError(L"File changed");
        ClearPendingPluginInfo();
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    LeaveCriticalSection(&g_pluginCS);

    /* Security: Verify plugin file hasn't changed since dialog was shown */
    char savedHash[65] = {0};
    const char* pendingHash = GetPendingPluginHash();
    if (pendingHash) {
        strncpy(savedHash, pendingHash, sizeof(savedHash) - 1);
        savedHash[sizeof(savedHash) - 1] = '\0';
    }

    char verifiedHash[65] = {0};
    if (savedHash[0] != '\0') {
        if (CalculatePluginHash(pluginPathUtf8, verifiedHash)) {
            if (strcmp(savedHash, verifiedHash) != 0) {
                LOG_ERROR("Plugin file changed during security dialog! Aborting launch for security.");
                LOG_ERROR("  Saved hash: %s", savedHash);
                LOG_ERROR("  Current hash: %s", verifiedHash);
                PluginProcess_SetLastError(L"File changed");
                ClearPendingPluginInfo();
                return FALSE;
            }
        } else {
            LOG_ERROR("Failed to calculate current plugin hash, aborting launch for security");
            PluginProcess_SetLastError(L"Hash error");
            ClearPendingPluginInfo();
            return FALSE;
        }
    } else {
        LOG_WARNING("No saved hash available for verification (proceeding anyway)");
    }

    if (trustPlugin) {
        /* User chose "Trust & Run" - reuse the security verification hash when available. */
        BOOL trustResult = verifiedHash[0] != '\0'
            ? TrustPluginWithVerifiedHash(pluginPathUtf8, verifiedHash)
            : TrustPlugin(pluginPathUtf8);
        if (!trustResult) {
            LOG_ERROR("Failed to add plugin to trust list: %ls (will still run once)", pluginDisplayName);
        }
    }

    BOOL result = LaunchPreparedPlugin(index, pluginPath);
    if (result && PluginManager_IsPluginRunning(index)) {
        UpdatePluginLastModTimeIfCurrent(index, pluginPath);
        StartHotReloadIfNeeded();
    }
    return result;
}

/**
 * @brief Internal restart function for hot-reload
 */
