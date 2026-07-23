#ifndef CONFIG_ANIMATION_INTERNAL_H
#define CONFIG_ANIMATION_INTERNAL_H

#include "config.h"

#include <windows.h>

#define ANIMATION_SPEED_POINT_CAPACITY 128
#define ANIMATION_SPEED_SCALE_MIN_PERCENT 1.0
#define ANIMATION_SPEED_SCALE_MAX_PERCENT 1000.0

typedef struct {
    int percent;
    double scalePercent;
} AnimationSpeedPoint;

typedef struct {
    AnimationSpeedMetric metric;
    AnimationSpeedPoint points[ANIMATION_SPEED_POINT_CAPACITY];
    int pointCount;
    double defaultScalePercent;
    double fixedScalePercent;
    int folderIntervalMs;
    int minIntervalMs;
} AnimationSpeedSnapshot;

AnimationSpeedMetric AnimationConfig_NormalizeMetric(
    AnimationSpeedMetric metric);
AnimationSpeedMetric AnimationConfig_MetricFromString(const char* value);
const char* AnimationConfig_MetricToString(AnimationSpeedMetric metric);
double AnimationConfig_NormalizeFixedPercent(double scalePercent);
double AnimationConfig_NormalizeScalePercent(double scalePercent,
                                              double fallbackPercent);
int AnimationConfig_ClampFolderInterval(int intervalMs);
int AnimationConfig_ClampMinInterval(int intervalMs);
double AnimationConfig_Interpolate(double percent,
                                   const AnimationSpeedPoint* points,
                                   int pointCount,
                                   double defaultScalePercent);

void AnimationConfig_GetSnapshot(AnimationSpeedSnapshot* snapshot);
void AnimationConfig_ReplaceSnapshot(const AnimationSpeedSnapshot* snapshot);
void AnimationConfig_SetMetric(AnimationSpeedMetric metric);
void AnimationConfig_SetFixedMetric(double fixedScalePercent);
BOOL AnimationConfig_LoadSnapshot(const char* configPath,
                                  AnimationSpeedSnapshot* snapshot);

#endif /* CONFIG_ANIMATION_INTERNAL_H */
