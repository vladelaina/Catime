/**
 * @file config_plugin_security_store.c
 * @brief Plugin trust INI serialization and atomic persistence
 */
#include "config_plugin_security_internal.h"

#include "log.h"
#include "utils/string_safe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    IniKeyValue updates[MAX_TRUSTED_PLUGINS + 1];
    char keys[MAX_TRUSTED_PLUGINS + 1][32];
    char values[MAX_TRUSTED_PLUGINS + 1][PLUGIN_TRUST_VALUE_BUFFER_SIZE];
} PluginTrustWriteBuffer;

static BOOL FormatEntryValue(const PluginTrustEntry* entry,
                             char* value, size_t valueSize) {
    int written;

    if (!entry || !value || valueSize == 0) return FALSE;
    written = snprintf(value, valueSize, "%s|%s",
                       entry->path, entry->sha256);
    return written >= 0 && (size_t)written < valueSize;
}

BOOL PluginTrustStore_WriteEntry(int index, const PluginTrustEntry* entry) {
    char key[32];
    char value[PLUGIN_TRUST_VALUE_BUFFER_SIZE];

    if (index < 0 || index >= MAX_TRUSTED_PLUGINS ||
        !FormatEntryValue(entry, value, sizeof(value))) {
        return FALSE;
    }
    snprintf(key, sizeof(key), "PLUGIN_%d", index);
    return UpdateConfigKeyValueAtomic(INI_SECTION_PLUGIN_TRUST, key, value);
}

BOOL PluginTrustStore_Rewrite(const PluginTrustState* trust) {
    PluginTrustWriteBuffer* buffer;
    char configPath[MAX_PATH] = {0};
    size_t updateCount = 0;
    int count;
    BOOL success;

    if (!trust) return FALSE;
    buffer = (PluginTrustWriteBuffer*)calloc(1, sizeof(*buffer));
    if (!buffer) {
        LOG_ERROR("Failed to allocate plugin trust persistence buffer");
        return FALSE;
    }

    count = PluginTrust_ClampCount(trust->count);
    for (int i = 0; i < count; ++i) {
        snprintf(buffer->keys[updateCount],
                 sizeof(buffer->keys[updateCount]), "PLUGIN_%d", i);
        if (!FormatEntryValue(&trust->entries[i],
                              buffer->values[updateCount],
                              sizeof(buffer->values[updateCount]))) {
            free(buffer);
            return FALSE;
        }
        buffer->updates[updateCount].section = INI_SECTION_PLUGIN_TRUST;
        buffer->updates[updateCount].key = buffer->keys[updateCount];
        buffer->updates[updateCount].value = buffer->values[updateCount];
        ++updateCount;
    }

    snprintf(buffer->keys[updateCount], sizeof(buffer->keys[updateCount]),
             "PLUGIN_%d", count);
    buffer->updates[updateCount].section = INI_SECTION_PLUGIN_TRUST;
    buffer->updates[updateCount].key = buffer->keys[updateCount];
    buffer->updates[updateCount].value = buffer->values[updateCount];
    ++updateCount;

    GetConfigPath(configPath, sizeof(configPath));
    success = configPath[0] != '\0' &&
              WriteIniMultipleAtomic(configPath, buffer->updates,
                                     updateCount);
    free(buffer);
    return success;
}

static BOOL ParseEntry(char* value, PluginTrustEntry* entry, int index) {
    char* separator;
    const char* hash;

    separator = strchr(value, '|');
    if (!separator || separator == value ||
        (size_t)(separator - value) >= sizeof(entry->path)) {
        LOG_WARNING("Ignoring malformed plugin trust entry %d", index);
        return FALSE;
    }
    *separator = '\0';
    hash = separator + 1;
    if (!PluginTrust_IsValidHash(hash)) {
        LOG_WARNING("Ignoring plugin trust entry %d with an invalid hash",
                    index);
        return FALSE;
    }
    safe_strncpy(entry->path, value, sizeof(entry->path));
    safe_strncpy(entry->sha256, hash, sizeof(entry->sha256));
    return TRUE;
}

BOOL PluginTrustStore_Load(PluginTrustState* trust) {
    char configPath[MAX_PATH] = {0};

    if (!trust) return FALSE;
    ZeroMemory(trust, sizeof(*trust));
    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0] == '\0') return FALSE;

    for (int i = 0; i < MAX_TRUSTED_PLUGINS; ++i) {
        char key[32];
        char value[PLUGIN_TRUST_VALUE_BUFFER_SIZE] = {0};
        PluginTrustEntry entry = {0};

        snprintf(key, sizeof(key), "PLUGIN_%d", i);
        if (!ReadIniStringExact(INI_SECTION_PLUGIN_TRUST, key, "", value,
                                sizeof(value), configPath)) {
            LOG_WARNING("Ignoring oversized plugin trust entry %d", i);
            continue;
        }
        if (value[0] == '\0') break;
        if (!ParseEntry(value, &entry, i)) continue;
        trust->entries[trust->count++] = entry;
    }
    return TRUE;
}
