/**
 * @file config_animation_common.c
 * @brief Animation speed normalization, mapping, and interpolation
 */
#include "config_animation_internal.h"

#include "tray/tray_animation_core.h"
#include "utils/finite_double.h"

#include <string.h>

AnimationSpeedMetric AnimationConfig_NormalizeMetric(
    AnimationSpeedMetric metric) {
    switch (metric) {
        case ANIMATION_SPEED_ORIGINAL:
        case ANIMATION_SPEED_MEMORY:
        case ANIMATION_SPEED_CPU:
        case ANIMATION_SPEED_TIMER:
        case ANIMATION_SPEED_FIXED:
            return metric;
        default:
            return ANIMATION_SPEED_MEMORY;
    }
}

AnimationSpeedMetric AnimationConfig_MetricFromString(const char* value) {
    if (value && _stricmp(value, "ORIGINAL") == 0) {
        return ANIMATION_SPEED_ORIGINAL;
    }
    if (value && _stricmp(value, "CPU") == 0) return ANIMATION_SPEED_CPU;
    if (value && (_stricmp(value, "TIMER") == 0 ||
                  _stricmp(value, "COUNTDOWN") == 0)) {
        return ANIMATION_SPEED_TIMER;
    }
    if (value && _stricmp(value, "FIXED") == 0) {
        return ANIMATION_SPEED_FIXED;
    }
    return ANIMATION_SPEED_MEMORY;
}

const char* AnimationConfig_MetricToString(AnimationSpeedMetric metric) {
    switch (AnimationConfig_NormalizeMetric(metric)) {
        case ANIMATION_SPEED_ORIGINAL: return "ORIGINAL";
        case ANIMATION_SPEED_CPU: return "CPU";
        case ANIMATION_SPEED_TIMER: return "TIMER";
        case ANIMATION_SPEED_FIXED: return "FIXED";
        case ANIMATION_SPEED_MEMORY:
        default: return "MEMORY";
    }
}

double AnimationConfig_NormalizeFixedPercent(double scalePercent) {
    const double minimum = ANIMATION_FIXED_SPEED_MIN_MULTIPLIER * 100.0;
    const double maximum = ANIMATION_FIXED_SPEED_MAX_MULTIPLIER * 100.0;
    const double fallback = ANIMATION_FIXED_SPEED_DEFAULT_MULTIPLIER * 100.0;

    if (!DoubleIsFiniteStrict(scalePercent) || scalePercent < minimum ||
        scalePercent > maximum) {
        return fallback;
    }
    return scalePercent;
}

double AnimationConfig_NormalizeScalePercent(double scalePercent,
                                              double fallbackPercent) {
    if (!DoubleIsFiniteStrict(scalePercent) ||
        scalePercent < ANIMATION_SPEED_SCALE_MIN_PERCENT) {
        return fallbackPercent;
    }
    if (scalePercent > ANIMATION_SPEED_SCALE_MAX_PERCENT) {
        return ANIMATION_SPEED_SCALE_MAX_PERCENT;
    }
    return scalePercent;
}

int AnimationConfig_ClampFolderInterval(int intervalMs) {
    if (intervalMs <= 0) return (int)TRAY_ANIMATION_DEFAULT_INTERVAL_MS;
    if (intervalMs < (int)TRAY_ANIMATION_MIN_INTERVAL_MS) {
        return (int)TRAY_ANIMATION_MIN_INTERVAL_MS;
    }
    if (intervalMs > (int)TRAY_ANIMATION_MAX_INTERVAL_MS) {
        return (int)TRAY_ANIMATION_MAX_INTERVAL_MS;
    }
    return intervalMs;
}

int AnimationConfig_ClampMinInterval(int intervalMs) {
    return intervalMs <= 0
        ? 0
        : AnimationConfig_ClampFolderInterval(intervalMs);
}

double AnimationConfig_Interpolate(double percent,
                                   const AnimationSpeedPoint* points,
                                   int pointCount,
                                   double defaultScalePercent) {
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;
    if (!points || pointCount <= 0) return defaultScalePercent;

    if (percent <= (double)points[0].percent) {
        double endpoint = (double)points[0].percent;
        if (endpoint <= 0.0) return points[0].scalePercent;
        return defaultScalePercent +
               (points[0].scalePercent - defaultScalePercent) *
               (percent / endpoint);
    }
    for (int i = 0; i < pointCount - 1; ++i) {
        double start = (double)points[i].percent;
        double end = (double)points[i + 1].percent;
        if (percent < start || percent > end) continue;
        if (end <= start) return points[i + 1].scalePercent;
        return points[i].scalePercent +
               (points[i + 1].scalePercent - points[i].scalePercent) *
               ((percent - start) / (end - start));
    }
    return points[pointCount - 1].scalePercent;
}
