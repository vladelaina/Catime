/**
 * @file config_writer.c
 * @brief Configuration collection and atomic write orchestration
 */
#include "config/config_writer.h"
#include "config_writer_internal.h"

#include "config.h"
#include "log.h"
#include "tray/tray_animation_core.h"
#include "utils/string_safe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BOOL CollectCurrentConfig(ConfigWriteItem* items, int itemCapacity,
                          int* count) {
    ConfigItemBuilder builder;

    if (!items || itemCapacity <= 0 || !count) return FALSE;
    *count = 0;
    ConfigWriter_InitBuilder(&builder, items, itemCapacity);
    if (!ConfigWriter_CollectGeneralDisplay(&builder) ||
        !ConfigWriter_CollectTimerPomodoro(&builder) ||
        !ConfigWriter_CollectNotification(&builder) ||
        !ConfigWriter_CollectHotkeysRecentColors(&builder)) {
        return FALSE;
    }
    *count = builder.count;
    return TRUE;
}

BOOL WriteConfigItems(const char* configPath,
                      const ConfigWriteItem* items, int count) {
    IniKeyValue stackUpdates[CONFIG_WRITE_ITEM_CAPACITY];
    IniKeyValue* updates = stackUpdates;
    BOOL result;

    if (!configPath || !items || count <= 0) return FALSE;
    if (count > CONFIG_WRITE_ITEM_CAPACITY) {
        updates = (IniKeyValue*)calloc((size_t)count, sizeof(*updates));
        if (!updates) return FALSE;
    }
    for (int i = 0; i < count; ++i) {
        updates[i].section = items[i].section;
        updates[i].key = items[i].key;
        updates[i].value = items[i].value;
    }
    result = WriteIniMultipleAtomic(configPath, updates, (size_t)count);
    if (updates != stackUpdates) free(updates);
    return result;
}

static BOOL IsBuiltInAnimation(const char* name) {
    static const char* const names[] = {
        "__logo__", "__cpu__", "__mem__", "__battery__",
        "__capslock__", "__none__"
    };
    for (size_t i = 0; i < _countof(names); ++i) {
        if (_stricmp(name, names[i]) == 0) return TRUE;
    }
    return FALSE;
}

static BOOL AppendAnimationPathIfMissing(const char* configPath,
                                         ConfigWriteItem* items,
                                         int* count) {
    char existingPath[MAX_PATH] = {0};
    BOOL complete = ReadIniStringExact("Animation", "ANIMATION_PATH", "",
                                       existingPath, sizeof(existingPath),
                                       configPath);
    const char* animation;
    ConfigWriteItem* item;
    int written;

    if (complete && existingPath[0] != '\0') return TRUE;
    if (!complete) {
        LOG_WARNING("Replacing oversized ANIMATION_PATH during full config write");
    }
    if (*count >= CONFIG_WRITE_ITEM_CAPACITY) return FALSE;
    animation = GetCurrentAnimationName();
    if (!animation || animation[0] == '\0') return TRUE;

    item = &items[*count];
    safe_strncpy(item->section, "Animation", sizeof(item->section));
    safe_strncpy(item->key, "ANIMATION_PATH", sizeof(item->key));
    written = IsBuiltInAnimation(animation)
        ? snprintf(item->value, sizeof(item->value), "%s", animation)
        : snprintf(item->value, sizeof(item->value),
                   "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s",
                   animation);
    if (written < 0 || written >= (int)sizeof(item->value)) return FALSE;
    ++(*count);
    return TRUE;
}

BOOL WriteConfig(const char* configPath) {
    ConfigWriteItem* items;
    int count = 0;
    BOOL result;

    if (!configPath) return FALSE;
    items = (ConfigWriteItem*)calloc(CONFIG_WRITE_ITEM_CAPACITY,
                                      sizeof(*items));
    if (!items) return FALSE;
    if (!CollectCurrentConfig(items, CONFIG_WRITE_ITEM_CAPACITY, &count) ||
        !AppendAnimationPathIfMissing(configPath, items, &count) ||
        !CollectAnimationSpeedConfigItems(
            items, CONFIG_WRITE_ITEM_CAPACITY, &count)) {
        LOG_ERROR("Failed to collect complete configuration");
        free(items);
        return FALSE;
    }

    result = WriteConfigItems(configPath, items, count);
    if (!result) {
        LOG_ERROR("Failed to write complete config: %s", configPath);
    }
    free(items);
    return result;
}

BOOL WriteConfigSection(const char* configPath, const char* section) {
    ConfigWriteItem* items;
    IniKeyValue updates[CONFIG_WRITE_ITEM_CAPACITY];
    int itemCount = 0;
    size_t updateCount = 0;
    BOOL result = TRUE;

    if (!configPath || !section) return FALSE;
    items = (ConfigWriteItem*)calloc(CONFIG_WRITE_ITEM_CAPACITY,
                                      sizeof(*items));
    if (!items) return FALSE;
    if (!CollectCurrentConfig(items, CONFIG_WRITE_ITEM_CAPACITY,
                              &itemCount)) {
        free(items);
        return FALSE;
    }

    for (int i = 0; i < itemCount; ++i) {
        if (strcmp(items[i].section, section) != 0) continue;
        updates[updateCount].section = items[i].section;
        updates[updateCount].key = items[i].key;
        updates[updateCount].value = items[i].value;
        ++updateCount;
    }
    if (updateCount > 0) {
        result = WriteIniMultipleAtomic(configPath, updates, updateCount);
        if (!result) {
            LOG_ERROR("Failed to write config section '%s': %s",
                      section, configPath);
        }
    }
    free(items);
    return result;
}
