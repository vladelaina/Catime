/**
 * @file config_defaults_internal.h
 * @brief Shared implementation details for default creation and migration.
 */

#ifndef CATIME_CONFIG_DEFAULTS_INTERNAL_H
#define CATIME_CONFIG_DEFAULTS_INTERNAL_H

#include <windows.h>
#include <winnls.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#include "config/config_defaults.h"
#include "config/config_loader.h"
#include "language.h"
#include "log.h"
#include "text_effect.h"
#include "utils/path_utils.h"
#include "utils/string_safe.h"
#include "../../resource/resource.h"

typedef struct ConfigEntry {
    char section[64];
    char key[64];
    char value[2048];
    struct ConfigEntry* next;
} ConfigEntry;

#define CONFIG_MIGRATION_MAX_PARSE_LINES   8192
#define CONFIG_MIGRATION_MAX_PARSE_ENTRIES 2048
#define CONFIG_MIGRATION_MAX_FILE_BYTES    (1024ull * 1024ull)

extern const ConfigItemMeta CONFIG_METADATA[];
extern const int CONFIG_METADATA_COUNT;

BOOL ConfigDefaults_GetMigrationFileSizeUtf8(const char* configPath,
                                               ULONGLONG* outSize);
void ConfigDefaults_FreeConfigEntryList(ConfigEntry* head);
BOOL ConfigDefaults_IsConfigItemMigratable(const char* section,
                                            const char* key);
BOOL ConfigDefaults_IsUtf8String(const char* value);
ConfigEntry* ConfigDefaults_ReadAllConfigEntries(const char* configPath);

#endif /* CATIME_CONFIG_DEFAULTS_INTERNAL_H */
