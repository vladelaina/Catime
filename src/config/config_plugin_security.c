/**
 * @file config_plugin_security.c
 * @brief Thread-safe plugin trust queries and mutations
 */
#include "config/config_plugin_security.h"
#include "config_plugin_security_internal.h"

#include "log.h"
#include "utils/string_safe.h"

#include <string.h>

static int FindTrustEntry(const PluginTrustState* trust,
                          const char* pluginPath) {
    int count;

    if (!trust || !pluginPath) return -1;
    count = PluginTrust_ClampCount(trust->count);
    for (int i = 0; i < count; ++i) {
        char expandedPath[MAX_PATH] = {0};
        PluginTrust_ExpandPath(trust->entries[i].path, expandedPath,
                               sizeof(expandedPath));
        if (PluginTrust_PathsEqual(expandedPath, pluginPath)) return i;
    }
    return -1;
}

BOOL IsPluginTrusted(const char* pluginPath) {
    char expectedHash[65] = {0};
    char currentHash[65] = {0};
    int index;

    if (!pluginPath) return FALSE;
    PluginTrustLock_EnterState();
    index = FindTrustEntry(&g_AppConfig.plugin_trust, pluginPath);
    if (index >= 0) {
        safe_strncpy(expectedHash,
                     g_AppConfig.plugin_trust.entries[index].sha256,
                     sizeof(expectedHash));
    }
    PluginTrustLock_LeaveState();

    if (index < 0 ||
        !PluginTrust_CalculateHash(pluginPath, currentHash)) {
        return FALSE;
    }
    if (strcmp(expectedHash, currentHash) == 0) return TRUE;
    LOG_WARNING("Plugin hash mismatch: %s (file may have been modified)",
                pluginPath);
    return FALSE;
}

BOOL TrustPlugin(const char* pluginPath) {
    char currentHash[65] = {0};

    if (!pluginPath ||
        !PluginTrust_CalculateHash(pluginPath, currentHash)) {
        return FALSE;
    }
    return TrustPluginWithVerifiedHash(pluginPath, currentHash);
}

static BOOL PrepareTrustedEntry(PluginTrustState* trust,
                                const char* pluginPath,
                                const char* verifiedHash,
                                int* entryIndex) {
    PluginTrustEntry* entry;
    char storedPath[MAX_PATH] = {0};
    int index = FindTrustEntry(trust, pluginPath);

    if (index < 0) {
        if (trust->count >= MAX_TRUSTED_PLUGINS) {
            LOG_ERROR("Maximum trusted plugins limit reached (%d)",
                      MAX_TRUSTED_PLUGINS);
            return FALSE;
        }
        index = trust->count++;
    }
    if (!PluginTrust_EncodePath(pluginPath, storedPath,
                                sizeof(storedPath))) {
        return FALSE;
    }

    entry = &trust->entries[index];
    safe_strncpy(entry->path, storedPath, sizeof(entry->path));
    safe_strncpy(entry->sha256, verifiedHash, sizeof(entry->sha256));
    *entryIndex = index;
    return TRUE;
}

BOOL TrustPluginWithVerifiedHash(const char* pluginPath,
                                 const char* verifiedHash) {
    PluginTrustState updatedTrust;
    int entryIndex = -1;
    BOOL success = FALSE;

    if (!pluginPath || !PluginTrust_IsValidHash(verifiedHash)) return FALSE;
    PluginTrustLock_EnterMutation();
    PluginTrustLock_EnterState();
    updatedTrust = g_AppConfig.plugin_trust;
    updatedTrust.count = PluginTrust_ClampCount(updatedTrust.count);
    success = PrepareTrustedEntry(&updatedTrust, pluginPath, verifiedHash,
                                  &entryIndex);
    PluginTrustLock_LeaveState();

    if (success) {
        success = PluginTrustStore_WriteEntry(
            entryIndex, &updatedTrust.entries[entryIndex]);
        if (!success) {
            LOG_ERROR("Failed to persist plugin trust for: %s", pluginPath);
        }
    }
    if (success) {
        PluginTrustLock_EnterState();
        g_AppConfig.plugin_trust = updatedTrust;
        PluginTrustLock_LeaveState();
    }
    PluginTrustLock_LeaveMutation();
    return success;
}

static void RemoveTrustEntry(PluginTrustState* trust, int index) {
    int count = PluginTrust_ClampCount(trust->count);

    for (int i = index; i < count - 1; ++i) {
        trust->entries[i] = trust->entries[i + 1];
    }
    if (count > 0) {
        ZeroMemory(&trust->entries[count - 1],
                   sizeof(trust->entries[count - 1]));
        trust->count = count - 1;
    }
}

BOOL UntrustPlugin(const char* pluginPath) {
    PluginTrustState updatedTrust;
    int index;
    BOOL success = FALSE;

    if (!pluginPath) return FALSE;
    PluginTrustLock_EnterMutation();
    PluginTrustLock_EnterState();
    updatedTrust = g_AppConfig.plugin_trust;
    updatedTrust.count = PluginTrust_ClampCount(updatedTrust.count);
    index = FindTrustEntry(&updatedTrust, pluginPath);
    if (index >= 0) RemoveTrustEntry(&updatedTrust, index);
    PluginTrustLock_LeaveState();

    if (index >= 0) {
        success = PluginTrustStore_Rewrite(&updatedTrust);
        if (!success) {
            LOG_ERROR("Failed to rewrite plugin trust config during removal");
        }
    }
    if (success) {
        PluginTrustLock_EnterState();
        g_AppConfig.plugin_trust = updatedTrust;
        PluginTrustLock_LeaveState();
    }
    PluginTrustLock_LeaveMutation();
    return success;
}

void LoadPluginTrustFromConfig(void) {
    PluginTrustState loadedTrust = {0};

    PluginTrustLock_EnterMutation();
    PluginTrustLock_EnterState();
    ZeroMemory(&g_AppConfig.plugin_trust,
               sizeof(g_AppConfig.plugin_trust));
    PluginTrustLock_LeaveState();

    if (PluginTrustStore_Load(&loadedTrust)) {
        PluginTrustLock_EnterState();
        g_AppConfig.plugin_trust = loadedTrust;
        PluginTrustLock_LeaveState();
    }
    PluginTrustLock_LeaveMutation();
}
