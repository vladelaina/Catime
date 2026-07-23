/**
 * @file config_animation_writer.c
 * @brief Serialization of animation speed and percent-icon configuration
 */
#include "config_animation_internal.h"
#include "config_writer_internal.h"

#include "config/config_writer.h"
#include "log.h"
#include "tray/tray_animation_percent.h"

#include <stdio.h>
#include <stdlib.h>

static BOOL AppendNumber(ConfigItemBuilder* builder, const char* key,
                         const char* format, double value) {
    char text[32];
    int written = snprintf(text, sizeof(text), format, value);
    if (written < 0 || written >= (int)sizeof(text)) return FALSE;
    return ConfigWriter_AppendString(builder, "Animation", key, text);
}

static BOOL AppendIntervals(ConfigItemBuilder* builder,
                            const AnimationSpeedSnapshot* snapshot) {
    return ConfigWriter_AppendInt(builder, "Animation",
                                  "ANIMATION_FOLDER_INTERVAL_MS",
                                  snapshot->folderIntervalMs) &&
           ConfigWriter_AppendInt(builder, "Animation",
                                  "ANIMATION_MIN_INTERVAL_MS",
                                  snapshot->minIntervalMs);
}

static BOOL AppendSpeedMap(ConfigItemBuilder* builder,
                           const AnimationSpeedSnapshot* snapshot) {
    for (int i = 0; i < snapshot->pointCount; ++i) {
        char key[64];
        char value[32];
        int keyLength = snprintf(key, sizeof(key),
                                 "ANIMATION_SPEED_MAP_%d",
                                 snapshot->points[i].percent);
        int valueLength = snprintf(value, sizeof(value), "%g",
                                   snapshot->points[i].scalePercent);
        if (keyLength < 0 || keyLength >= (int)sizeof(key) ||
            valueLength < 0 || valueLength >= (int)sizeof(value) ||
            !ConfigWriter_AppendString(builder, "Animation", key, value)) {
            return FALSE;
        }
    }
    return TRUE;
}

static void FormatColor(COLORREF color, const char* automaticValue,
                        char output[16]) {
    if (color == TRANSPARENT_BG_AUTO) {
        snprintf(output, 16, "%s", automaticValue);
    } else {
        snprintf(output, 16, "#%02X%02X%02X",
                 GetRValue(color), GetGValue(color), GetBValue(color));
    }
}

static BOOL AppendPercentColors(ConfigItemBuilder* builder) {
    COLORREF textColor = GetPercentIconTextColor();
    COLORREF backgroundColor = GetPercentIconBgColor();
    char textValue[16];
    char backgroundValue[16];

    if (backgroundColor == TRANSPARENT_BG_AUTO) {
        snprintf(textValue, sizeof(textValue), "auto");
    } else {
        FormatColor(textColor, "auto", textValue);
    }
    FormatColor(backgroundColor, "transparent", backgroundValue);
    return ConfigWriter_AppendString(builder, "Animation",
                                     "PERCENT_ICON_TEXT_COLOR",
                                     textValue) &&
           ConfigWriter_AppendString(builder, "Animation",
                                     "PERCENT_ICON_BG_COLOR",
                                     backgroundValue);
}

BOOL CollectAnimationSpeedConfigItems(ConfigWriteItem* items,
                                      int itemCapacity, int* count) {
    AnimationSpeedSnapshot snapshot;
    ConfigItemBuilder builder;

    if (!items || !count || itemCapacity <= 0 || *count < 0 ||
        *count > itemCapacity) {
        return FALSE;
    }
    AnimationConfig_GetSnapshot(&snapshot);
    ConfigWriter_InitBuilder(&builder, items, itemCapacity);
    builder.count = *count;

    if (!ConfigWriter_AppendString(
            &builder, "Animation", "ANIMATION_SPEED_METRIC",
            AnimationConfig_MetricToString(snapshot.metric)) ||
        !AppendNumber(&builder, "ANIMATION_FIXED_SPEED_PERCENT", "%g",
                      snapshot.fixedScalePercent) ||
        !ConfigWriter_AppendInt(
            &builder, "Animation", "ANIMATION_SPEED_DEFAULT",
            (int)(snapshot.defaultScalePercent + 0.5)) ||
        !AppendSpeedMap(&builder, &snapshot) ||
        !AppendIntervals(&builder, &snapshot) ||
        !AppendPercentColors(&builder)) {
        return FALSE;
    }
    *count = builder.count;
    return TRUE;
}

BOOL WriteAnimationSpeedToConfig(const char* configPath) {
    const int capacity = ANIMATION_SPEED_POINT_CAPACITY + 8;
    ConfigWriteItem* items;
    int count = 0;
    BOOL result;

    if (!configPath) return FALSE;
    items = (ConfigWriteItem*)calloc((size_t)capacity, sizeof(*items));
    if (!items) return FALSE;
    if (!CollectAnimationSpeedConfigItems(items, capacity, &count)) {
        free(items);
        return FALSE;
    }
    result = WriteConfigItems(configPath, items, count);
    if (!result) {
        LOG_ERROR("Failed to write animation speed config: %s", configPath);
    }
    free(items);
    return result;
}
