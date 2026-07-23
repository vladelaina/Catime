/**
 * @file config_writer_builder.c
 * @brief Bounds-checked construction of configuration write items
 */
#include "config_writer_internal.h"

#include "log.h"
#include "utils/string_safe.h"

#include <stdio.h>
#include <string.h>

void ConfigWriter_InitBuilder(ConfigItemBuilder* builder,
                              ConfigWriteItem* items, int capacity) {
    if (!builder) return;
    builder->items = items;
    builder->capacity = capacity;
    builder->count = 0;
}

ConfigWriteItem* ConfigWriter_ReserveItem(ConfigItemBuilder* builder,
                                          const char* section,
                                          const char* key) {
    ConfigWriteItem* item;

    if (!builder || !builder->items || builder->count < 0 ||
        builder->count >= builder->capacity) {
        LOG_ERROR("Config item capacity exceeded while collecting [%s] %s",
                  section ? section : "", key ? key : "");
        return NULL;
    }
    item = &builder->items[builder->count++];
    ZeroMemory(item, sizeof(*item));
    safe_strncpy(item->section, section ? section : "",
                 sizeof(item->section));
    safe_strncpy(item->key, key ? key : "", sizeof(item->key));
    return item;
}

BOOL ConfigWriter_AppendString(ConfigItemBuilder* builder,
                               const char* section, const char* key,
                               const char* value) {
    ConfigWriteItem* item = ConfigWriter_ReserveItem(builder, section, key);
    if (!item) return FALSE;
    safe_strncpy(item->value, value ? value : "", sizeof(item->value));
    return TRUE;
}

BOOL ConfigWriter_AppendInt(ConfigItemBuilder* builder,
                            const char* section, const char* key, int value) {
    ConfigWriteItem* item = ConfigWriter_ReserveItem(builder, section, key);
    if (!item) return FALSE;
    return snprintf(item->value, sizeof(item->value), "%d", value) >= 0;
}

BOOL ConfigWriter_AppendFloat(ConfigItemBuilder* builder,
                              const char* section, const char* key,
                              float value) {
    ConfigWriteItem* item = ConfigWriter_ReserveItem(builder, section, key);
    if (!item) return FALSE;
    return snprintf(item->value, sizeof(item->value), "%.9g", value) >= 0;
}

BOOL ConfigWriter_AppendBool(ConfigItemBuilder* builder,
                             const char* section, const char* key,
                             BOOL value) {
    return ConfigWriter_AppendString(builder, section, key,
                                     value ? "TRUE" : "FALSE");
}

BOOL ConfigWriter_AppendListToken(char* destination,
                                  size_t destinationSize,
                                  const char* token, BOOL* first,
                                  const char* listName) {
    size_t destinationLength;
    size_t separatorLength;
    size_t tokenLength;
    size_t remaining;

    if (!destination || destinationSize == 0 || !token || !first) {
        return FALSE;
    }
    destinationLength = strnlen(destination, destinationSize);
    if (destinationLength >= destinationSize) return FALSE;
    separatorLength = *first ? 0 : 1;
    tokenLength = strlen(token);
    remaining = destinationSize - destinationLength - 1;
    if (separatorLength > remaining ||
        tokenLength > remaining - separatorLength) {
        LOG_WARNING("%s is too long; remaining entries were omitted",
                    listName ? listName : "Config list");
        return FALSE;
    }

    if (separatorLength) destination[destinationLength++] = ',';
    memcpy(destination + destinationLength, token, tokenLength);
    destination[destinationLength + tokenLength] = '\0';
    *first = FALSE;
    return TRUE;
}

BOOL ConfigWriter_AppendIntList(ConfigItemBuilder* builder,
                                const char* section, const char* key,
                                const int* values, int count,
                                const char* listName) {
    ConfigWriteItem* item = ConfigWriter_ReserveItem(builder, section, key);
    BOOL first = TRUE;

    if (!item || (!values && count > 0) || count < 0) return FALSE;
    for (int i = 0; i < count; ++i) {
        char value[32];
        if (snprintf(value, sizeof(value), "%d", values[i]) < 0 ||
            !ConfigWriter_AppendListToken(item->value,
                                          sizeof(item->value), value,
                                          &first, listName)) {
            return FALSE;
        }
    }
    return TRUE;
}
