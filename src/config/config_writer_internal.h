#ifndef CONFIG_WRITER_INTERNAL_H
#define CONFIG_WRITER_INTERNAL_H

#include "config/config_writer.h"

#include <windows.h>
#include <stddef.h>

typedef struct {
    ConfigWriteItem* items;
    int capacity;
    int count;
} ConfigItemBuilder;

void ConfigWriter_InitBuilder(ConfigItemBuilder* builder,
                              ConfigWriteItem* items, int capacity);
ConfigWriteItem* ConfigWriter_ReserveItem(ConfigItemBuilder* builder,
                                          const char* section,
                                          const char* key);
BOOL ConfigWriter_AppendString(ConfigItemBuilder* builder,
                               const char* section, const char* key,
                               const char* value);
BOOL ConfigWriter_AppendInt(ConfigItemBuilder* builder,
                            const char* section, const char* key, int value);
BOOL ConfigWriter_AppendFloat(ConfigItemBuilder* builder,
                              const char* section, const char* key,
                              float value);
BOOL ConfigWriter_AppendBool(ConfigItemBuilder* builder,
                             const char* section, const char* key,
                             BOOL value);
BOOL ConfigWriter_AppendIntList(ConfigItemBuilder* builder,
                                const char* section, const char* key,
                                const int* values, int count,
                                const char* listName);
BOOL ConfigWriter_AppendListToken(char* destination,
                                  size_t destinationSize,
                                  const char* token, BOOL* first,
                                  const char* listName);

BOOL ConfigWriter_CollectGeneralDisplay(ConfigItemBuilder* builder);
BOOL ConfigWriter_CollectTimerPomodoro(ConfigItemBuilder* builder);
BOOL ConfigWriter_CollectNotification(ConfigItemBuilder* builder);
BOOL ConfigWriter_CollectHotkeysRecentColors(ConfigItemBuilder* builder);

#endif /* CONFIG_WRITER_INTERNAL_H */
