/**
 * @file config_plugin_security.c
 * @brief Plugin security and trust management
 */

#include "config/config_plugin_security.h"
#include "config.h"
#include "log.h"
#include "utils/string_convert.h"
#include "utils/string_safe.h"
#include <windows.h>
#include <wincrypt.h>
#include <string.h>
#include <stdio.h>
#include <shlobj.h>
#include <stdlib.h>

/* Thread safety: Critical section to protect g_AppConfig.plugin_trust */
static CRITICAL_SECTION g_pluginTrustCS;
static CRITICAL_SECTION g_pluginTrustMutationCS;
static volatile LONG g_pluginTrustCSInitialized = 0;

#define PLUGIN_TRUST_CS_UNINITIALIZED 0
#define PLUGIN_TRUST_CS_INITIALIZING 1
#define PLUGIN_TRUST_CS_INITIALIZED 2
#define PLUGIN_HASH_READ_BUFFER_SIZE (64 * 1024)
#define PLUGIN_HASH_MAX_FILE_BYTES (64ull * 1024ull * 1024ull)
#define PLUGIN_TRUST_VALUE_BUFFER_SIZE (MAX_PATH + 65 + 2)
#define PLUGIN_TRUST_WAIT_SPIN_LIMIT 64

static int ClampPluginTrustCount(int count) {
    if (count < 0) {
        return 0;
    }
    if (count > MAX_TRUSTED_PLUGINS) {
        return MAX_TRUSTED_PLUGINS;
    }
    return count;
}

/**
 * @brief Initialize plugin trust critical section (thread-safe)
 * Uses InterlockedCompareExchange for atomic initialization
 */
static void WaitWhilePluginTrustCSInitializing(void) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(&g_pluginTrustCSInitialized, 0, 0) ==
           PLUGIN_TRUST_CS_INITIALIZING) {
        Sleep(spins++ < PLUGIN_TRUST_WAIT_SPIN_LIMIT ? 0 : 1);
    }
}

static void EnsurePluginTrustCSInitialized(void) {
    if (InterlockedCompareExchange(&g_pluginTrustCSInitialized, 0, 0) ==
        PLUGIN_TRUST_CS_INITIALIZED) {
        return;
    }

    if (InterlockedCompareExchange(&g_pluginTrustCSInitialized,
                                   PLUGIN_TRUST_CS_INITIALIZING,
                                   PLUGIN_TRUST_CS_UNINITIALIZED) == PLUGIN_TRUST_CS_UNINITIALIZED) {
        InitializeCriticalSection(&g_pluginTrustCS);
        InitializeCriticalSection(&g_pluginTrustMutationCS);
        InterlockedExchange(&g_pluginTrustCSInitialized, PLUGIN_TRUST_CS_INITIALIZED);
        return;
    }

    WaitWhilePluginTrustCSInitializing();
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
static void CopyPluginTrustPathFallback(const char* src, char* dest, size_t destSize) {
    if (!dest || destSize == 0) return;
    dest[0] = '\0';
    if (!src) return;

    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

static BOOL PluginTrustPathsEqual(const char* a, const char* b) {
    if (!a || !b) return FALSE;

    wchar_t aWide[MAX_PATH];
    wchar_t bWide[MAX_PATH];
    if (Utf8ToWide(a, aWide, MAX_PATH) &&
        Utf8ToWide(b, bWide, MAX_PATH)) {
        return _wcsicmp(aWide, bWide) == 0;
    }

    return _stricmp(a, b) == 0;
}

static BOOL IsValidPluginTrustHash(const char* hash) {
    if (!hash || strlen(hash) != 64) {
        return FALSE;
    }

    for (int i = 0; i < 64; i++) {
        char c = hash[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL CompressPath(const char* fullPath, char* compressedPath, size_t bufferSize) {
    if (!fullPath || !compressedPath || bufferSize == 0) return FALSE;

    compressedPath[0] = '\0';

    wchar_t fullPathWide[MAX_PATH];
    wchar_t localAppData[MAX_PATH];
    char localAppDataUtf8[MAX_PATH] = {0};
    if (Utf8ToWide(fullPath, fullPathWide, MAX_PATH) &&
        GetEffectiveLocalAppDataPath(localAppDataUtf8, sizeof(localAppDataUtf8)) &&
        Utf8ToWide(localAppDataUtf8, localAppData, MAX_PATH)) {
        size_t len = wcslen(localAppData);
        if (_wcsnicmp(fullPathWide, localAppData, len) == 0 &&
            (fullPathWide[len] == L'\0' || fullPathWide[len] == L'\\')) {
            wchar_t compressedWide[MAX_PATH];
            int written = _snwprintf_s(compressedWide, MAX_PATH, _TRUNCATE,
                                       L"%%LOCALAPPDATA%%%ls", fullPathWide + len);
            if (written >= 0 &&
                WideToUtf8(compressedWide, compressedPath, bufferSize)) {
                return TRUE;
            }
            LOG_WARNING("Compressed plugin trust path too long, storing full path instead: %s", fullPath);
            compressedPath[0] = '\0';
        }
    }

    /* No compression, copy as-is */
    if (strlen(fullPath) >= bufferSize) {
        LOG_ERROR("Plugin trust path too long to store: %s", fullPath);
        return FALSE;
    }

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

    if (_strnicmp(compressedPath, "%LOCALAPPDATA%", strlen("%LOCALAPPDATA%")) == 0) {
        if (ExpandEffectiveLocalAppDataPath(compressedPath, expandedPath, bufferSize)) {
            return TRUE;
        }
        CopyPluginTrustPathFallback(compressedPath, expandedPath, bufferSize);
        return FALSE;
    }

    wchar_t compressedWide[MAX_PATH];
    wchar_t expandedWide[MAX_PATH];
    if (!Utf8ToWide(compressedPath, compressedWide, MAX_PATH)) {
        CopyPluginTrustPathFallback(compressedPath, expandedPath, bufferSize);
        return FALSE;
    }

    DWORD result = ExpandEnvironmentStringsW(compressedWide, expandedWide, MAX_PATH);
    if (result == 0) {
        LOG_ERROR("Failed to expand path: %s (error: %lu)", compressedPath, GetLastError());
        CopyPluginTrustPathFallback(compressedPath, expandedPath, bufferSize);
        return FALSE;
    }
    if (result > MAX_PATH) {
        LOG_WARNING("Path expansion buffer too small (need %lu bytes): %s", result, compressedPath);
        CopyPluginTrustPathFallback(compressedPath, expandedPath, bufferSize);
        return FALSE;
    }
    if (!WideToUtf8(expandedWide, expandedPath, bufferSize)) {
        LOG_WARNING("Expanded plugin trust path is too long: %s", compressedPath);
        CopyPluginTrustPathFallback(compressedPath, expandedPath, bufferSize);
        return FALSE;
    }
    return TRUE;
}

static BOOL CalculateFileSHA256(const char* filePath, char* hashHex) {
    if (!filePath || !hashHex) return FALSE;

    wchar_t filePathWide[MAX_PATH];
    if (!Utf8ToWide(filePath, filePathWide, MAX_PATH)) {
        LOG_ERROR("Failed to convert plugin path for hashing: %s", filePath);
        return FALSE;
    }

    /* Open file */
    HANDLE hFile = CreateFileW(filePathWide, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL,
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                               NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to open file for hashing: %s (error: %lu)", filePath, GetLastError());
        return FALSE;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) ||
        fileSize.QuadPart < 0 ||
        (ULONGLONG)fileSize.QuadPart > PLUGIN_HASH_MAX_FILE_BYTES) {
        LOG_ERROR("Plugin file too large or unreadable for hashing: %s (limit %llu bytes)",
                  filePath, (ULONGLONG)PLUGIN_HASH_MAX_FILE_BYTES);
        CloseHandle(hFile);
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
    BYTE* buffer = (BYTE*)malloc(PLUGIN_HASH_READ_BUFFER_SIZE);
    if (!buffer) {
        LOG_ERROR("Failed to allocate plugin hash buffer (%u bytes)",
                  (unsigned)PLUGIN_HASH_READ_BUFFER_SIZE);
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return FALSE;
    }
    DWORD bytesRead;
    BOOL success = TRUE;

    while (TRUE) {
        if (!ReadFile(hFile, buffer, PLUGIN_HASH_READ_BUFFER_SIZE, &bytesRead, NULL)) {
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
    free(buffer);
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

    /* Thread-safe access to trust list */
    EnsurePluginTrustCSInitialized();
    EnterCriticalSection(&g_pluginTrustCS);

    BOOL hasTrustEntry = FALSE;
    char expectedHash[65] = {0};
    int trustCount = ClampPluginTrustCount(g_AppConfig.plugin_trust.count);

    /* Check trust list (use case-insensitive comparison for Windows paths) */
    for (int i = 0; i < trustCount; i++) {
        /* Expand stored path if it contains environment variables */
        char expandedPath[MAX_PATH];
        ExpandPath(g_AppConfig.plugin_trust.entries[i].path, expandedPath, sizeof(expandedPath));

        if (PluginTrustPathsEqual(expandedPath, pluginPath)) {
            strncpy(expectedHash, g_AppConfig.plugin_trust.entries[i].sha256, sizeof(expectedHash) - 1);
            expectedHash[sizeof(expectedHash) - 1] = '\0';
            hasTrustEntry = TRUE;
            break;
        }
    }

    LeaveCriticalSection(&g_pluginTrustCS);

    if (!hasTrustEntry) {
        return FALSE;
    }

    char currentHash[65];
    if (!CalculateFileSHA256(pluginPath, currentHash)) {
        return FALSE;
    }

    if (strcmp(expectedHash, currentHash) == 0) {
        return TRUE;
    }

    LOG_WARNING("Plugin hash mismatch: %s (file may have been modified)", pluginPath);
    return FALSE;
}

/**
 * @brief Add plugin to trust list
 * @param pluginPath Path to plugin file
 * @return TRUE if successful
 */
BOOL TrustPlugin(const char* pluginPath) {
    if (!pluginPath) return FALSE;

    char currentHash[65];
    if (!CalculateFileSHA256(pluginPath, currentHash)) {
        return FALSE;
    }

    return TrustPluginWithVerifiedHash(pluginPath, currentHash);
}

BOOL TrustPluginWithVerifiedHash(const char* pluginPath, const char* verifiedHash) {
    if (!pluginPath || !IsValidPluginTrustHash(verifiedHash)) {
        return FALSE;
    }

    EnsurePluginTrustCSInitialized();
    EnterCriticalSection(&g_pluginTrustMutationCS);

    PluginTrustState updatedTrust;
    char key[32] = {0};
    char value[PLUGIN_TRUST_VALUE_BUFFER_SIZE] = {0};
    BOOL foundExisting = FALSE;

    EnterCriticalSection(&g_pluginTrustCS);
    updatedTrust = g_AppConfig.plugin_trust;
    updatedTrust.count = ClampPluginTrustCount(updatedTrust.count);

    /* Check if already trusted (use case-insensitive comparison for Windows paths) */
    for (int i = 0; i < updatedTrust.count; i++) {
        /* Expand stored path if it contains environment variables */
        char expandedPath[MAX_PATH];
        ExpandPath(updatedTrust.entries[i].path, expandedPath, sizeof(expandedPath));

        if (PluginTrustPathsEqual(expandedPath, pluginPath)) {
            char compressedPath[MAX_PATH];
            CompressPath(expandedPath, compressedPath, sizeof(compressedPath));
            if (compressedPath[0] == '\0') {
                LeaveCriticalSection(&g_pluginTrustCS);
                LeaveCriticalSection(&g_pluginTrustMutationCS);
                return FALSE;
            }

            safe_strncpy(updatedTrust.entries[i].path, compressedPath,
                         sizeof(updatedTrust.entries[i].path));
            safe_strncpy(updatedTrust.entries[i].sha256, verifiedHash,
                         sizeof(updatedTrust.entries[i].sha256));

            snprintf(key, sizeof(key), "PLUGIN_%d", i);
            snprintf(value, sizeof(value), "%s|%s",
                     updatedTrust.entries[i].path,
                     updatedTrust.entries[i].sha256);
            foundExisting = TRUE;
            break;
        }
    }

    if (!foundExisting) {
        if (updatedTrust.count >= MAX_TRUSTED_PLUGINS) {
            LOG_ERROR("Maximum trusted plugins limit reached (%d)", MAX_TRUSTED_PLUGINS);
            LeaveCriticalSection(&g_pluginTrustCS);
            LeaveCriticalSection(&g_pluginTrustMutationCS);
            return FALSE;
        }

        int index = updatedTrust.count;
        PluginTrustEntry* entry = &updatedTrust.entries[index];

        char compressedPath[MAX_PATH];
        CompressPath(pluginPath, compressedPath, sizeof(compressedPath));
        if (compressedPath[0] == '\0') {
            LeaveCriticalSection(&g_pluginTrustCS);
            LeaveCriticalSection(&g_pluginTrustMutationCS);
            return FALSE;
        }
        strncpy(entry->path, compressedPath, sizeof(entry->path) - 1);
        entry->path[sizeof(entry->path) - 1] = '\0';

        strncpy(entry->sha256, verifiedHash, sizeof(entry->sha256) - 1);
        entry->sha256[sizeof(entry->sha256) - 1] = '\0';

        updatedTrust.count++;

        snprintf(key, sizeof(key), "PLUGIN_%d", index);
        snprintf(value, sizeof(value), "%s|%s", entry->path, entry->sha256);
    }

    LeaveCriticalSection(&g_pluginTrustCS);

    if (!UpdateConfigKeyValueAtomic(INI_SECTION_PLUGIN_TRUST, key, value)) {
        LOG_ERROR("Failed to write plugin trust config for: %s", pluginPath);
        LeaveCriticalSection(&g_pluginTrustMutationCS);
        return FALSE;
    }

    EnterCriticalSection(&g_pluginTrustCS);
    g_AppConfig.plugin_trust = updatedTrust;
    LeaveCriticalSection(&g_pluginTrustCS);

    LeaveCriticalSection(&g_pluginTrustMutationCS);
    return TRUE;
}

/**
 * @brief Remove plugin from trust list
 * @param pluginPath Path to plugin file
 * @return TRUE if successful
 */
BOOL UntrustPlugin(const char* pluginPath) {
    if (!pluginPath) return FALSE;

    IniKeyValue* updates = (IniKeyValue*)calloc(MAX_TRUSTED_PLUGINS + 1,
                                                sizeof(*updates));
    char (*keyStorage)[32] = (char (*)[32])calloc(MAX_TRUSTED_PLUGINS + 1,
                                                  sizeof(*keyStorage));
    char (*valueStorage)[PLUGIN_TRUST_VALUE_BUFFER_SIZE] =
        (char (*)[PLUGIN_TRUST_VALUE_BUFFER_SIZE])calloc(MAX_TRUSTED_PLUGINS + 1,
                                                         sizeof(*valueStorage));
    if (!updates || !keyStorage || !valueStorage) {
        free(valueStorage);
        free(keyStorage);
        free(updates);
        LOG_ERROR("Failed to allocate plugin trust update buffers");
        return FALSE;
    }

    EnsurePluginTrustCSInitialized();
    EnterCriticalSection(&g_pluginTrustMutationCS);
    EnterCriticalSection(&g_pluginTrustCS);

    PluginTrustState updatedTrust = g_AppConfig.plugin_trust;
    int trustCount = ClampPluginTrustCount(updatedTrust.count);

    /* Find and remove entry (use case-insensitive comparison for Windows paths) */
    for (int i = 0; i < trustCount; i++) {
        /* Expand stored path if it contains environment variables */
        char expandedPath[MAX_PATH];
        ExpandPath(updatedTrust.entries[i].path, expandedPath, sizeof(expandedPath));

        if (PluginTrustPathsEqual(expandedPath, pluginPath)) {
            /* Shift remaining entries */
            for (int j = i; j < trustCount - 1; j++) {
                updatedTrust.entries[j] = updatedTrust.entries[j + 1];
            }
            ZeroMemory(&updatedTrust.entries[trustCount - 1],
                       sizeof(updatedTrust.entries[trustCount - 1]));
            updatedTrust.count = trustCount - 1;

            size_t updateCount = 0;

            for (int j = 0; j < updatedTrust.count; j++) {
                snprintf(keyStorage[updateCount], sizeof(keyStorage[updateCount]), "PLUGIN_%d", j);
                snprintf(valueStorage[updateCount], sizeof(valueStorage[updateCount]), "%s|%s",
                         updatedTrust.entries[j].path,
                         updatedTrust.entries[j].sha256);
                updates[updateCount].section = INI_SECTION_PLUGIN_TRUST;
                updates[updateCount].key = keyStorage[updateCount];
                updates[updateCount].value = valueStorage[updateCount];
                updateCount++;
            }

            snprintf(keyStorage[updateCount], sizeof(keyStorage[updateCount]),
                     "PLUGIN_%d", updatedTrust.count);
            valueStorage[updateCount][0] = '\0';
            updates[updateCount].section = INI_SECTION_PLUGIN_TRUST;
            updates[updateCount].key = keyStorage[updateCount];
            updates[updateCount].value = valueStorage[updateCount];
            updateCount++;

            char configPath[MAX_PATH];
            GetConfigPath(configPath, sizeof(configPath));

            LeaveCriticalSection(&g_pluginTrustCS);
            BOOL writeSuccess = WriteIniMultipleAtomic(configPath, updates, updateCount);
            if (!writeSuccess) {
                LOG_ERROR("Failed to rewrite plugin trust config during untrust operation");
                LeaveCriticalSection(&g_pluginTrustMutationCS);
                free(valueStorage);
                free(keyStorage);
                free(updates);
                return FALSE;
            }

            EnterCriticalSection(&g_pluginTrustCS);
            g_AppConfig.plugin_trust = updatedTrust;
            LeaveCriticalSection(&g_pluginTrustCS);

            LeaveCriticalSection(&g_pluginTrustMutationCS);
            free(valueStorage);
            free(keyStorage);
            free(updates);
            return TRUE;
        }
    }

    LeaveCriticalSection(&g_pluginTrustCS);
    LeaveCriticalSection(&g_pluginTrustMutationCS);
    free(valueStorage);
    free(keyStorage);
    free(updates);
    return FALSE;
}

/**
 * @brief Cleanup plugin trust critical section
 * WARNING: Must be called only after all threads that might use it have stopped
 */
void CleanupPluginTrustCS(void) {
    WaitWhilePluginTrustCSInitializing();

    if (InterlockedCompareExchange(&g_pluginTrustCSInitialized, 0, 0) == PLUGIN_TRUST_CS_INITIALIZED) {
        EnterCriticalSection(&g_pluginTrustMutationCS);
        EnterCriticalSection(&g_pluginTrustCS);
        LeaveCriticalSection(&g_pluginTrustCS);
        LeaveCriticalSection(&g_pluginTrustMutationCS);
        DeleteCriticalSection(&g_pluginTrustCS);
        DeleteCriticalSection(&g_pluginTrustMutationCS);
        InterlockedExchange(&g_pluginTrustCSInitialized, PLUGIN_TRUST_CS_UNINITIALIZED);
    }
}

/**
 * @brief Load plugin trust state from config
 */
void LoadPluginTrustFromConfig(void) {
    PluginTrustState loadedTrust = {0};

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    EnsurePluginTrustCSInitialized();
    EnterCriticalSection(&g_pluginTrustMutationCS);

    /* Clear existing trust list before file I/O so stale trust is not retained. */
    EnterCriticalSection(&g_pluginTrustCS);
    memset(&g_AppConfig.plugin_trust, 0, sizeof(g_AppConfig.plugin_trust));
    LeaveCriticalSection(&g_pluginTrustCS);

    for (int i = 0; i < MAX_TRUSTED_PLUGINS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "PLUGIN_%d", i);
        
        char value[PLUGIN_TRUST_VALUE_BUFFER_SIZE];
        if (!ReadIniStringExact(INI_SECTION_PLUGIN_TRUST, key, "", value,
                                sizeof(value), config_path)) {
            LOG_WARNING("Ignoring plugin trust entry %d because the config value is too long", i);
            continue;
        }

        if (value[0] != '\0') {
            /* Parse "path|hash" format */
            char* separator = strchr(value, '|');
            if (separator && separator != value && (separator - value) < MAX_PATH) {
                *separator = '\0';
                const char* const hash = separator + 1;
                
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
                    
                    if (validHash) {
                        /* Defensive check: ensure we don't overflow (though loop limit prevents this) */
                        if (loadedTrust.count >= MAX_TRUSTED_PLUGINS) {
                            LOG_ERROR("Plugin trust list overflow during load (max: %d)", MAX_TRUSTED_PLUGINS);
                            break;
                        }

                        PluginTrustEntry* entry = &loadedTrust.entries[loadedTrust.count];

                        strncpy(entry->path, value, sizeof(entry->path) - 1);
                        entry->path[sizeof(entry->path) - 1] = '\0';

                        strncpy(entry->sha256, hash, sizeof(entry->sha256) - 1);
                        entry->sha256[sizeof(entry->sha256) - 1] = '\0';

                        loadedTrust.count++;
                    }
                }
            }
        } else {
            /* No more entries */
            break;
        }
    }

    EnterCriticalSection(&g_pluginTrustCS);
    g_AppConfig.plugin_trust = loadedTrust;
    LeaveCriticalSection(&g_pluginTrustCS);
    LeaveCriticalSection(&g_pluginTrustMutationCS);
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
