/**
 * @file config_defaults_migration_rules.c
 * @brief Validation rules for values retained during configuration migration.
 */

#include "config_defaults_internal.h"

BOOL ConfigDefaults_GetMigrationFileSizeUtf8(const char* configPath, ULONGLONG* outSize) {
    if (!configPath || !outSize) return FALSE;
    *outSize = 0;

    wchar_t wConfigPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wConfigPath, MAX_PATH) == 0) {
        return FALSE;
    }

    HANDLE hFile = CreateFileW(wConfigPath, GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    LARGE_INTEGER size;
    BOOL result = GetFileSizeEx(hFile, &size);
    CloseHandle(hFile);
    if (!result || size.QuadPart < 0) {
        return FALSE;
    }

    *outSize = (ULONGLONG)size.QuadPart;
    return TRUE;
}

void ConfigDefaults_FreeConfigEntryList(ConfigEntry* head) {
    while (head) {
        ConfigEntry* next = head->next;
        free(head);
        head = next;
    }
}

static BOOL IsConfigItemInMetadata(const char* section, const char* key) {
    if (!section || !key) return FALSE;

    for (int i = 0; i < CONFIG_METADATA_COUNT; i++) {
        if (strcmp(CONFIG_METADATA[i].section, section) == 0 &&
            strcmp(CONFIG_METADATA[i].key, key) == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

typedef struct {
    const char* section;
    const char* keyPrefix;
    int minIndex;
    int maxIndex;
} DynamicConfigMigrationRule;

static const DynamicConfigMigrationRule DYNAMIC_CONFIG_MIGRATION_RULES[] = {
    {INI_SECTION_RECENTFILES, "CLOCK_RECENT_FILE_", 1, MAX_RECENT_FILES},
    {INI_SECTION_PLUGIN_TRUST, "PLUGIN_", 0, MAX_TRUSTED_PLUGINS - 1},
};

static BOOL IsIndexedDynamicConfigItem(const char* section, const char* key,
                                       const DynamicConfigMigrationRule* rule) {
    if (!section || !key || !rule || strcmp(section, rule->section) != 0) {
        return FALSE;
    }

    size_t prefixLen = strlen(rule->keyPrefix);
    if (strncmp(key, rule->keyPrefix, prefixLen) != 0 || key[prefixLen] == '\0') {
        return FALSE;
    }

    int index = 0;
    for (const char* p = key + prefixLen; *p; ++p) {
        if (!isdigit((unsigned char)*p)) {
            return FALSE;
        }
        index = index * 10 + (*p - '0');
        if (index > rule->maxIndex) {
            return FALSE;
        }
    }

    if (index < rule->minIndex || index > rule->maxIndex) {
        return FALSE;
    }

    char expectedKey[64];
    snprintf(expectedKey, sizeof(expectedKey), "%s%d", rule->keyPrefix, index);
    return strcmp(key, expectedKey) == 0;
}

static BOOL IsDynamicConfigItemMigratable(const char* section, const char* key) {
    for (size_t i = 0; i < _countof(DYNAMIC_CONFIG_MIGRATION_RULES); ++i) {
        if (IsIndexedDynamicConfigItem(section, key, &DYNAMIC_CONFIG_MIGRATION_RULES[i])) {
            return TRUE;
        }
    }

    return FALSE;
}

BOOL ConfigDefaults_IsConfigItemMigratable(const char* section, const char* key) {
    return IsConfigItemInMetadata(section, key) ||
           IsDynamicConfigItemMigratable(section, key);
}

/* Helper to detect if a string is valid UTF-8 */
BOOL ConfigDefaults_IsUtf8String(const char* value) {
    const unsigned char* bytes = (const unsigned char*)value;
    while (*bytes) {
        if ((*bytes & 0x80) == 0) { /* ASCII */
            bytes++;
        } else if ((*bytes & 0xE0) == 0xC0) { /* 2-byte sequence */
            if ((bytes[1] & 0xC0) != 0x80) return FALSE;
            bytes += 2;
        } else if ((*bytes & 0xF0) == 0xE0) { /* 3-byte sequence */
            if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80) return FALSE;
            bytes += 3;
        } else if ((*bytes & 0xF8) == 0xF0) { /* 4-byte sequence */
            if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80 || (bytes[3] & 0xC0) != 0x80) return FALSE;
            bytes += 4;
        } else {
            return FALSE;
        }
    }
    return TRUE;
}
