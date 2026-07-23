/**
 * @file config_defaults_migration.c
 * @brief Rebuild-and-restore configuration migration workflow.
 */

#include "config_defaults_internal.h"

void MigrateConfig(const char* config_path) {
    if (!config_path) return;

    /* Step 1: Read ALL config entries from old file (automatic discovery) */
    ConfigEntry* oldConfig = ConfigDefaults_ReadAllConfigEntries(config_path);
    if (!oldConfig) {
        /* If reading fails, just create default config */
        InvalidateIniCache();
        (void)CreateDefaultConfig(config_path);
        return;
    }

    /* Step 2: Detect and convert legacy default PERCENT_ICON colors */
    ConfigEntry* textColorEntry = NULL;
    ConfigEntry* bgColorEntry = NULL;
    ConfigEntry* current = oldConfig;
    while (current) {
        if (strcmp(current->section, "Animation") == 0) {
            if (strcmp(current->key, "PERCENT_ICON_TEXT_COLOR") == 0) {
                textColorEntry = current;
            } else if (strcmp(current->key, "PERCENT_ICON_BG_COLOR") == 0) {
                bgColorEntry = current;
            }
        }
        current = current->next;
    }

    /* Convert old hardcoded defaults to new "auto"/"transparent" keywords */
    if (textColorEntry && bgColorEntry) {
        /* Old buggy default: white text (#FFFFFF), black bg (#000000) */
        BOOL isOldBuggyDefault =
            (strcasecmp(textColorEntry->value, "#FFFFFF") == 0 || strcasecmp(textColorEntry->value, "#ffffff") == 0) &&
            (strcasecmp(bgColorEntry->value, "#000000") == 0 || strcasecmp(bgColorEntry->value, "#000") == 0);

        /* Old fixed default: black text (#000000), white bg (#FFFFFF) */
        BOOL isOldFixedDefault =
            (strcasecmp(textColorEntry->value, "#000000") == 0 || strcasecmp(textColorEntry->value, "#000") == 0) &&
            (strcasecmp(bgColorEntry->value, "#FFFFFF") == 0 || strcasecmp(bgColorEntry->value, "#ffffff") == 0);

        /* Convert to new defaults */
        if (isOldBuggyDefault || isOldFixedDefault) {
            safe_strncpy(textColorEntry->value, "auto", sizeof(textColorEntry->value));
            safe_strncpy(bgColorEntry->value, "transparent", sizeof(bgColorEntry->value));
        }
    }

    /* Step 3: Invalidate stale cache before replacing with fresh defaults. */
    InvalidateIniCache();

    /* Step 4: Create fresh default config atomically. */
    if (!CreateDefaultConfig(config_path)) {
        ConfigDefaults_FreeConfigEntryList(oldConfig);
        return;
    }

    /* Step 5: Restore user values that exist in metadata or supported dynamic keys */
    int restoreCount = 0;
    current = oldConfig;
    while (current) {
        if (strcmp(current->key, "CONFIG_VERSION") != 0 &&
            ConfigDefaults_IsConfigItemMigratable(current->section, current->key)) {
            restoreCount++;
        }
        current = current->next;
    }

    if (restoreCount > 0) {
        IniKeyValue* updates = (IniKeyValue*)calloc((size_t)restoreCount, sizeof(IniKeyValue));
        if (updates) {
            size_t updateCount = 0;
            current = oldConfig;
            while (current) {
                /* Skip CONFIG_VERSION - must be updated to current version */
                if (strcmp(current->key, "CONFIG_VERSION") != 0 &&
                    ConfigDefaults_IsConfigItemMigratable(current->section, current->key)) {
                    updates[updateCount].section = current->section;
                    updates[updateCount].key = current->key;
                    updates[updateCount].value = current->value;
                    updateCount++;
                }
                current = current->next;
            }

            if (!WriteIniMultipleAtomic(config_path, updates, updateCount)) {
                LOG_ERROR("Failed to restore %zu config values during migration", updateCount);
            }
            free(updates);
        } else {
            int failedWrites = 0;
            current = oldConfig;
            while (current) {
                if (strcmp(current->key, "CONFIG_VERSION") != 0 &&
                    ConfigDefaults_IsConfigItemMigratable(current->section, current->key)) {
                    if (!WriteIniString(current->section, current->key,
                                        current->value, config_path)) {
                        failedWrites++;
                    }
                }
                current = current->next;
            }
            if (failedWrites > 0) {
                LOG_ERROR("Failed to restore %d config values during migration", failedWrites);
            }
        }
    }

    /* Clean up */
    ConfigDefaults_FreeConfigEntryList(oldConfig);
}
