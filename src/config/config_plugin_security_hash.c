/**
 * @file config_plugin_security_hash.c
 * @brief Bounded SHA-256 hashing for plugin trust verification
 */
#include "config/config_plugin_security.h"
#include "config_plugin_security_internal.h"

#include "log.h"
#include "utils/string_convert.h"

#include <wincrypt.h>
#include <stdio.h>
#include <stdlib.h>

#define PLUGIN_HASH_READ_BUFFER_SIZE (64 * 1024)
#define PLUGIN_HASH_MAX_FILE_BYTES (64ull * 1024ull * 1024ull)

static BOOL ReadFileIntoHash(HANDLE file, HCRYPTHASH hash, BYTE* buffer) {
    for (;;) {
        DWORD bytesRead = 0;
        if (!ReadFile(file, buffer, PLUGIN_HASH_READ_BUFFER_SIZE,
                      &bytesRead, NULL)) {
            LOG_ERROR("Failed to read plugin for hashing (error=%lu)",
                      GetLastError());
            return FALSE;
        }
        if (bytesRead == 0) return TRUE;
        if (!CryptHashData(hash, buffer, bytesRead, 0)) {
            LOG_ERROR("Failed to update plugin hash (error=%lu)",
                      GetLastError());
            return FALSE;
        }
    }
}

static BOOL WriteHashHex(HCRYPTHASH hash, char* hashHex) {
    BYTE digest[32];
    DWORD digestSize = sizeof(digest);

    if (!CryptGetHashParam(hash, HP_HASHVAL, digest, &digestSize, 0)) {
        LOG_ERROR("Failed to finish plugin hash (error=%lu)", GetLastError());
        return FALSE;
    }
    if (digestSize != sizeof(digest)) {
        LOG_ERROR("Unexpected plugin hash length: %lu", digestSize);
        return FALSE;
    }
    for (DWORD i = 0; i < digestSize; ++i) {
        sprintf_s(hashHex + (i * 2), 3, "%02x", digest[i]);
    }
    hashHex[64] = '\0';
    return TRUE;
}

BOOL PluginTrust_CalculateHash(const char* filePath, char* hashHex) {
    wchar_t filePathWide[MAX_PATH];
    HANDLE file = INVALID_HANDLE_VALUE;
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    BYTE* buffer = NULL;
    LARGE_INTEGER fileSize;
    BOOL success = FALSE;

    if (!filePath || !hashHex) return FALSE;
    hashHex[0] = '\0';
    if (!Utf8ToWide(filePath, filePathWide, _countof(filePathWide))) {
        LOG_ERROR("Failed to convert plugin path for hashing: %s", filePath);
        return FALSE;
    }

    file = CreateFileW(filePathWide, GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_DELETE, NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                       NULL);
    if (file == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to open plugin for hashing: %s (error=%lu)",
                  filePath, GetLastError());
        goto cleanup;
    }
    if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart < 0 ||
        (ULONGLONG)fileSize.QuadPart > PLUGIN_HASH_MAX_FILE_BYTES) {
        LOG_ERROR("Plugin is unreadable or exceeds the %llu byte hash limit: %s",
                  (ULONGLONG)PLUGIN_HASH_MAX_FILE_BYTES, filePath);
        goto cleanup;
    }
    if (!CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_AES,
                             CRYPT_VERIFYCONTEXT)) {
        LOG_ERROR("Failed to acquire plugin hash provider (error=%lu)",
                  GetLastError());
        goto cleanup;
    }
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        LOG_ERROR("Failed to create plugin hash (error=%lu)", GetLastError());
        goto cleanup;
    }

    buffer = (BYTE*)malloc(PLUGIN_HASH_READ_BUFFER_SIZE);
    if (!buffer) {
        LOG_ERROR("Failed to allocate plugin hash buffer");
        goto cleanup;
    }
    success = ReadFileIntoHash(file, hash, buffer) &&
              WriteHashHex(hash, hashHex);

cleanup:
    free(buffer);
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    return success;
}

BOOL CalculatePluginHash(const char* pluginPath, char* hashHex) {
    return PluginTrust_CalculateHash(pluginPath, hashHex);
}
