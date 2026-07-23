/**
 * @file config_animation_io.c
 * @brief Persistence and reload of animation speed state
 */
#include "config_animation_internal.h"

#include "config/config_writer.h"
#include "tray/tray_animation_core.h"
#include "utils/finite_double.h"

#include <stdio.h>
#include <string.h>

BOOL WriteConfigAnimationSpeedMetric(AnimationSpeedMetric metric) {
    char configPath[MAX_PATH] = {0};
    char currentValue[32] = {0};
    const char* metricValue;
    BOOL configMatches;

    metric = AnimationConfig_NormalizeMetric(metric);
    metricValue = AnimationConfig_MetricToString(metric);
    GetConfigPath(configPath, sizeof(configPath));
    ReadIniString("Animation", "ANIMATION_SPEED_METRIC", "__missing__",
                  currentValue, sizeof(currentValue), configPath);
    configMatches = _stricmp(currentValue, metricValue) == 0;
    if (GetAnimationSpeedMetric() == metric && configMatches) return TRUE;
    if (!configMatches &&
        !WriteIniString("Animation", "ANIMATION_SPEED_METRIC",
                        metricValue, configPath)) {
        return FALSE;
    }
    AnimationConfig_SetMetric(metric);
    return TRUE;
}

BOOL WriteConfigAnimationFixedSpeed(double multiplier) {
    char configPath[MAX_PATH] = {0};
    ConfigWriteItem items[2] = {0};
    double scalePercent;

    if (DoubleIsNaNStrict(multiplier) ||
        multiplier < ANIMATION_FIXED_SPEED_MIN_MULTIPLIER) {
        return FALSE;
    }
    if (!DoubleIsFiniteStrict(multiplier) ||
        multiplier > ANIMATION_FIXED_SPEED_MAX_MULTIPLIER) {
        multiplier = ANIMATION_FIXED_SPEED_MAX_MULTIPLIER;
    }
    scalePercent = multiplier * 100.0;
    GetConfigPath(configPath, sizeof(configPath));

    snprintf(items[0].section, sizeof(items[0].section), "Animation");
    snprintf(items[0].key, sizeof(items[0].key),
             "ANIMATION_FIXED_SPEED_PERCENT");
    snprintf(items[0].value, sizeof(items[0].value), "%.10g",
             scalePercent);
    snprintf(items[1].section, sizeof(items[1].section), "Animation");
    snprintf(items[1].key, sizeof(items[1].key),
             "ANIMATION_SPEED_METRIC");
    snprintf(items[1].value, sizeof(items[1].value), "FIXED");
    if (!WriteConfigItems(configPath, items, _countof(items))) return FALSE;
    AnimationConfig_SetFixedMetric(scalePercent);
    return TRUE;
}

void ReloadAnimationSpeedFromConfig(void) {
    char configPath[MAX_PATH] = {0};
    AnimationSpeedSnapshot snapshot;

    GetConfigPath(configPath, sizeof(configPath));
    if (!AnimationConfig_LoadSnapshot(configPath, &snapshot)) return;
    AnimationConfig_ReplaceSnapshot(&snapshot);
    TrayAnimation_SetBaseIntervalMs((UINT)snapshot.folderIntervalMs);
    TrayAnimation_SetMinIntervalMs((UINT)snapshot.minIntervalMs);
}
