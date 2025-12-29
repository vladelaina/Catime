/**
 * @file config_plugin_security.c
 * @brief Plugin security and trust management
 */

#include "config/config_plugin_security.h"
#include "config.h"
#include "log.h"
#include <windows.h>
#include <wincrypt.h>
#include <string.h>
#include <stdio.h>
#include <shlobj.h>

/* Plugin trust section name */
#define INI_SECTION_PLUGIN_TRUST "PluginTrust"

/* Thread safety: Critical section to protect g_AppConfig.plugin_trust */
static CRITICAL_SECTION g_pluginTrustCS;
static volatile LONG g_pluginTrustCSInitialized = 0;

/**
 * @brief Initialize plugin trust critical section (thread-safe)
 * Uses InterlockedCompareExchange for atomic initialization
 */
static void EnsurePluginTrustCSInitialized(void) {
    /* Fast path: already initialized */
    if (g_pluginTrustCSInitialized == 1) {
        return;
    }
    
    /* Atomic initialization using InterlockedCompareExchange */
    if (InterlockedCompareExchange(&g_pluginTrustCSInitialized, 1, 0) == 0) {
        /* This thread won the race, initialize the critical section */
        InitializeCriticalSection(&g_pluginTrustCS);
    } else {
        /* Another thread is initializing or has initialized, wait */
        while (g_pluginTrustCSInitialized != 1) {
            Sleep(0);  /* Yield to other threads */
        }
    }
}

/**
 * @brief Calculate SHA256 hash of a file
 * @param filePath Path to file
 * @param hashHex Output buffer for hex string (must be 65 bytes)
 * @return TRUE if successful
 */
/**
 * @brief Compress path by replacing known folders with environment variables
 * @param fullPath Full path to compress
 * @param compressedPath Output buffer for compressed path
 * @param bufferSize Size of output buffer
 * @return TRUE if compression was applied
 */
static BOOL CompressPath(const char* fullPath, char* compressedPath, size_t bufferSize) {
    if (!fullPath || !compressedPath || bufferSize == 0) return FALSE;
    
    char localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
        size_t len = strlen(localAppData);
        if (_strnicmp(fullPath, localAppData, len) == 0) {
            snprintf(compressedPath, bufferSize, "%%LOCALAPPDATA%%%s", fullPath + len);
            return TRUE;
        }
    }
    
    /* No compression, copy as-is */
    strncpy(compressedPath, fullPath, bufferSize - 1);
    compressedPath[bufferSize - 1] = '\0';
    return FALSE;
}

/**
 * @brief Expand environment variables in path
 * @param compressedPath Path with environment variables
 * @param expandedPath Output buffer for expanded path
 * @param bufferSize Size of output buffer
 * @return TRUE if successful
 */
static BOOL ExpandPath(const char* compressedPath, char* expandedPath, size_t bufferSize) {
    if (!compressedPath || !expandedPath || bufferSize == 0) return FALSE;
    
    DWORD result = ExpandEnvironmentStringsA(compressedPath, expandedPath, (DWORD)bufferSize);
    if (result == 0) {
        LOG_ERROR("Failed to expand path: %s (error: %lu)", compressedPath, GetLastError());
        strncpy(expandedPath, compressedPath, bufferSize - 1);
        expandedPath[bufferSize - 1] = '\0';
        return FALSE;
    }
    if (result > bufferSize) {
        LOG_WARNING("Path expansion buffer too small (need %lu bytes): %s", result, compressedPath);
        strncpy(expandedPath, compressedPath, bufferSize - 1);
        expandedPath[bufferSize - 1] = '\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL CalculateFileSHA256(const char* filePath, char* hashHex) {
    if (!filePath || !hashHex) return FALSE;
    
    /* Open file */
    HANDLE hFile = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, 
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to open file for hashing: %s (error: %lu)", filePath, GetLastError());
        return FALSE;
    }
    
    /* Get crypto provider */
    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        LOG_ERROR("CryptAcquireContext failed (error: %lu)", GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }
    
    /* Create hash object */
    HCRYPTHASH hHash = 0;
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        LOG_ERROR("CryptCreateHash failed (error: %lu)", GetLastError());
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return FALSE;
    }
    
    /* Read and hash file */
    BYTE buffer[4096];
    DWORD bytesRead;
    BOOL success = TRUE;
    
    while (TRUE) {
        if (!ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL)) {
            LOG_ERROR("ReadFile failed (error: %lu)", GetLastError());
            success = FALSE;
            break;
        }
        
        if (bytesRead == 0) {
            /* End of file reached successfully */
            break;
        }
        
        if (!CryptHashData(hHash, buffer, bytesRead, 0)) {
            LOG_ERROR("CryptHashData failed (error: %lu)", GetLastError());
            success = FALSE;
            break;
        }
    }
    
    /* Get hash result */
    BYTE hash[32];  /* SHA256 is 32 bytes */
    DWORD hashLen = sizeof(hash);
    
    if (success && CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
        /* Verify hash length (SHA256 should be exactly 32 bytes) */
        if (hashLen != 32) {
            LOG_ERROR("Unexpected hash length: %lu (expected 32)", hashLen);
            success = FALSE;
        } else {
            /* Convert to hex string - safe because hashLen is validated */
            for (DWORD i = 0; i < 32; i++) {
                sprintf_s(hashHex + (i * 2), 3, "%02x", hash[i]);
            }
            hashHex[64] = '\0';
        }
    } else {
        LOG_ERROR("CryptGetHashParam failed (error: %lu)", GetLastError());
        success = FALSE;
    }
    
    /* Cleanup */
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    CloseHandle(hFile);
    
    return success;
}

/**
 * @brief Check if a plugin is trusted
 * @param pluginPath Path to plugin file
 * @return TRUE if trusted and hash matches
 */
BOOL IsPluginTrusted(const char* pluginPath) {
    if (!pluginPath) return FALSE;
    
    /* Calculate current hash */
    char currentHash[65];
    if (!CalculateFileSHA256(pluginPath, currentHash)) {
        return FALSE;
    }
    
    /* Thread-safe access to trust list */
    EnsurePluginTrustCSInitialized();
    EnterCriticalSection(&g_pluginTrustCS);
    
    BOOL isTrusted = FALSE;
    
    /* Check trust list (use case-insensitive comparison for Windows paths) */
    for (int i = 0; i < g_AppConfig.plugin_trust.count; i++) {
        /* Expand stored path if it contains environment variables */
        char expandedPath[MAX_PATH];
        ExpandPath(g_AppConfig.plugin_trust.entries[i].path, expandedPath, sizeof(expandedPath));
        
        if (_stricmp(expandedPath, pluginPath) == 0) {
            /* Path matches, check hash */
            if (strcmp(g_AppConfig.plugin_trust.entries[i].sha256, currentHash) == 0) {
                isTrusted = TRUE;
            } else {
                LOG_WARNING("Plugin hash mismatch: %s (file may have been modified)", pluginPath);
                isTrusted = FALSE;
            }
            break;
        }
    }
    
    LeaveCriticalSection(&g_pluginTrustCS);
    return isTrusted;
}

/**
 * @brief Add plugin to trust list
 * @param pluginPath Path to plugin file
 * @return TRUE if successful
 */
BOOL TrustPlugin(const char* pluginPath) {
    if (!pluginPath) return FALSE;
    
    /* Thread-safe access to trust list */
    EnsurePluginTrustCSInitialized();
    EnterCriticalSection(&g_pluginTrustCS);
    
    /* Check if already trusted (use case-insensitive comparison for Windows paths) */
    for (int i = 0; i < g_AppConfig.plugin_trust.count; i++) {
        /* Expand stored path if it contains environment variables */
        char expandedPath[MAX_PATH];
        ExpandPath(g_AppConfig.plugin_trust.entries[i].path, expandedPath, sizeof(expandedPath));
        
        if (_stricmp(expandedPath, pluginPath) == 0) {
            /* Update hash */
            if (!CalculateFileSHA256(pluginPath, g_AppConfig.plugin_trust.entries[i].sha256)) {
                LeaveCriticalSection(&g_pluginTrustCS);
                return FALSE;
            }
            
            /* Write to config with compressed path */
            char key[32];
            snprintf(key, sizeof(key), "PLUGIN_%d", i);
            
            char compressedPath[MAX_PATH];
            CompressPath(expandedPath, compressedPath, sizeof(compressedPath));
            
            /* Update in-memory path to compressed form */
            strncpy(g_AppConfig.plugin_trust.entries[i].path, compressedPath, sizeof(g_AppConfig.plugin_trust.entries[i].path) - 1);
            g_AppConfig.plugin_trust.entries[i].path[sizeof(g_AppConfig.plugin_trust.entries[i].path) - 1] = '\0';
            
            char value[MAX_PATH + 65 + 2];
            snprintf(value, sizeof(value), "%s|%s", 
                    compressedPath,
                    g_AppConfig.plugin_trust.entries[i].sha256);
            
            if (!UpdateConfigKeyValueAtomic(INI_SECTION_PLUGIN_TRUST, key, value)) {
                LOG_ERROR("Failed to update plugin trust config for: %s", pluginPath);
                LeaveCriticalSection(&g_pluginTrustCS);
                return FALSE;
            }
            LeaveCriticalSection(&g_pluginTrustCS);
            return TRUE;
        }
    }
    
    /* Add new entry */
    if (g_AppConfig.plugin_trust.count >= MAX_TRUSTED_PLUGINS) {
        LOG_ERROR("Maximum trusted plugins limit reached (%d)", MAX_TRUSTED_PLUGINS);
        LeaveCriticalSection(&g_pluginTrustCS);
        return FALSE;
    }
    
    int index = g_AppConfig.plugin_trust.count;
    PluginTrustEntry* entry = &g_AppConfig.plugin_trust.entries[index];
    
    /* Store path in compressed form */
    char compressedPath[MAX_PATH];
    CompressPath(pluginPath, compressedPath, sizeof(compressedPath));
    strncpy(entry->path, compressedPath, sizeof(entry->path) - 1);
    entry->path[sizeof(entry->path) - 1] = '\0';
    
    /* Calculate and store hash */
    if (!CalculateFileSHA256(pluginPath, entry->sha256)) {
        LeaveCriticalSection(&g_pluginTrustCS);
        return FALSE;
    }
    
    g_AppConfig.plugin_trust.count++;
    
    /* Write to config */
    char key[32];
    snprintf(key, sizeof(key), "PLUGIN_%d", index);
    
    char value[MAX_PATH + 65 + 2];
    snprintf(value, sizeof(value), "%s|%s", entry->path, entry->sha256);
    
    if (!UpdateConfigKeyValueAtomic(INI_SECTION_PLUGIN_TRUST, key, value)) {
        LOG_ERROR("Failed to write plugin trust config for: %s", pluginPath);
        /* Rollback: decrease count since write failed */
        g_AppConfig.plugin_trust.count--;
        LeaveCriticalSection(&g_pluginTrustCS);
        return FALSE;
    }
    
    LOG_INFO("Plugin trusted: %s", pluginPath);
    LeaveCriticalSection(&g_pluginTrustCS);
    return TRUE;
}

/**
 * @brief Remove plugin from trust list
 * @param pluginPath Path to plugin file
 * @return TRUE if successful
 */
BOOL UntrustPlugin(const char* pluginPath) {
    if (!pluginPath) return FALSE;
    
    /* Thread-safe access to trust list */
    EnsurePluginTrustCSInitialized();
    EnterCriticalSection(&g_pluginTrustCS);
    
    /* Find and remove entry (use case-insensitive comparison for Windows paths) */
    for (int i = 0; i < g_AppConfig.plugin_trust.count; i++) {
        /* Expand stored path if it contains environment variables */
        char expandedPath[MAX_PATH];
        ExpandPath(g_AppConfig.plugin_trust.entries[i].path, expandedPath, sizeof(expandedPath));
        
        if (_stricmp(expandedPath, pluginPath) == 0) {
            /* Shift remaining entries */
            for (int j = i; j < g_AppConfig.plugin_trust.count - 1; j++) {
                g_AppConfig.plugin_trust.entries[j] = g_AppConfig.plugin_trust.entries[j + 1];
            }
            g_AppConfig.plugin_trust.count--;
            
            /* Rewrite all entries to config */
            BOOL writeSuccess = TRUE;
            for (int j = 0; j < g_AppConfig.plugin_trust.count; j++) {
                char key[32];
                snprintf(key, sizeof(key), "PLUGIN_%d", j);
                
                char value[MAX_PATH + 65 + 2];
                snprintf(value, sizeof(value), "%s|%s", 
                        g_AppConfig.plugin_trust.entries[j].path,
                        g_AppConfig.plugin_trust.entries[j].sha256);
                
                if (!UpdateConfigKeyValueAtomic(INI_SECTION_PLUGIN_TRUST, key, value)) {
                    LOG_ERROR("Failed to rewrite plugin trust entry %d during untrust operation", j);
                    writeSuccess = FALSE;
                }
            }
            
            /* Remove the now-unused last key by writing empty string */
            char lastKey[32];
            snprintf(lastKey, sizeof(lastKey), "PLUGIN_%d", g_AppConfig.plugin_trust.count);
            if (!UpdateConfigKeyValueAtomic(INI_SECTION_PLUGIN_TRUST, lastKey, "")) {
                LOG_ERROR("Failed to clear last plugin trust entry during untrust operation");
                writeSuccess = FALSE;
            }
            
            if (writeSuccess) {
                LOG_INFO("Plugin untrusted: %s", pluginPath);
            } else {
                LOG_WARNING("Plugin untrusted in memory: %s (config update had errors, will be corrected on next load)", pluginPath);
            }
            LeaveCriticalSection(&g_pluginTrustCS);
            return TRUE;
        }
    }
    
    LeaveCriticalSection(&g_pluginTrustCS);
    return FALSE;
}

/**
 * @brief Cleanup plugin trust critical section
 * WARNING: Must be called only after all threads that might use it have stopped
 */
void CleanupPluginTrustCS(void) {
    if (g_pluginTrustCSInitialized == 1) {
        /* Acquire lock one last time to ensure no thread is using it */
        EnterCriticalSection(&g_pluginTrustCS);
        LeaveCriticalSection(&g_pluginTrustCS);
        
        /* Now safe to delete */
        DeleteCriticalSection(&g_pluginTrustCS);
        InterlockedExchange(&g_pluginTrustCSInitialized, 0);
    }
}

/**
 * @brief Load plugin trust state from config
 */
void LoadPluginTrustFromConfig(void) {
    /* Thread-safe access to trust list */
    EnsurePluginTrustCSInitialized();
    EnterCriticalSection(&g_pluginTrustCS);
    
    /* Clear existing trust list for security */
    memset(&g_AppConfig.plugin_trust, 0, sizeof(g_AppConfig.plugin_trust));
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    for (int i = 0; i < MAX_TRUSTED_PLUGINS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "PLUGIN_%d", i);
        
        char value[MAX_PATH + 65 + 2];
        ReadIniString(INI_SECTION_PLUGIN_TRUST, key, "", value, sizeof(value), config_path);
        
        if (strlen(value) > 0) {
            /* Parse "path|hash" format */
            char* separator = strchr(value, '|');
            if (separator && (separator - value) < MAX_PATH) {
                *separator = '\0';
                char* hash = separator + 1;
                
                if (strlen(hash) == 64) {  /* SHA256 hex is 64 chars */
                    /* Validate hash contains only hex characters */
                    BOOL validHash = TRUE;
                    for (int j = 0; j < 64; j++) {
                        char c = hash[j];
                        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                            validHash = FALSE;
                            LOG_WARNING("Invalid hash format in config for plugin (index %d)", i);
                            break;
                        }
                    }
                    
                    if (validHash && strlen(value) > 0) {  /* Also check path is not empty */
                        /* Defensive check: ensure we don't overflow (though loop limit prevents this) */
                        if (g_AppConfig.plugin_trust.count >= MAX_TRUSTED_PLUGINS) {
                            LOG_ERROR("Plugin trust list overflow during load (max: %d)", MAX_TRUSTED_PLUGINS);
                            break;
                        }
                        
                        PluginTrustEntry* entry = &g_AppConfig.plugin_trust.entries[g_AppConfig.plugin_trust.count];
                        
                        strncpy(entry->path, value, sizeof(entry->path) - 1);
                        entry->path[sizeof(entry->path) - 1] = '\0';
                        
                        strncpy(entry->sha256, hash, sizeof(entry->sha256) - 1);
                        entry->sha256[sizeof(entry->sha256) - 1] = '\0';
                        
                        g_AppConfig.plugin_trust.count++;
                    }
                }
            }
        } else {
            /* No more entries */
            break;
        }
    }
    
    LOG_INFO("Loaded %d trusted plugins from config", g_AppConfig.plugin_trust.count);
    LeaveCriticalSection(&g_pluginTrustCS);
}

/**
 * @brief Calculate SHA256 hash of a plugin file (public interface)
 * @param pluginPath Path to plugin file
 * @param hashHex Output buffer for hex string (must be 65 bytes)
 * @return TRUE if successful
 */
BOOL CalculatePluginHash(const char* pluginPath, char* hashHex) {
    return CalculateFileSHA256(pluginPath, hashHex);
}
