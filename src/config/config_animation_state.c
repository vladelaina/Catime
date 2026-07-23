/**
 * @file config_animation_state.c
 * @brief Thread-safe runtime animation speed state
 */
#include "config_animation_internal.h"

#include "tray/tray_animation_core.h"
#include "utils/finite_double.h"

#include <math.h>
#include <string.h>

static AnimationSpeedSnapshot g_animationSpeed = {
    .metric = ANIMATION_SPEED_MEMORY,
    .pointCount = 0,
    .defaultScalePercent = 100.0,
    .fixedScalePercent = ANIMATION_FIXED_SPEED_DEFAULT_MULTIPLIER * 100.0,
    .folderIntervalMs = TRAY_ANIMATION_DEFAULT_INTERVAL_MS,
    .minIntervalMs = 0,
};
static SRWLOCK g_animationSpeedLock = SRWLOCK_INIT;

void AnimationConfig_GetSnapshot(AnimationSpeedSnapshot* snapshot) {
    if (!snapshot) return;
    AcquireSRWLockShared(&g_animationSpeedLock);
    *snapshot = g_animationSpeed;
    ReleaseSRWLockShared(&g_animationSpeedLock);
    if (snapshot->pointCount < 0) snapshot->pointCount = 0;
    if (snapshot->pointCount > ANIMATION_SPEED_POINT_CAPACITY) {
        snapshot->pointCount = ANIMATION_SPEED_POINT_CAPACITY;
    }
}

static BOOL PointsMatch(const AnimationSpeedSnapshot* first,
                        const AnimationSpeedSnapshot* second) {
    if (first->pointCount != second->pointCount) return FALSE;
    for (int i = 0; i < first->pointCount; ++i) {
        if (first->points[i].percent != second->points[i].percent ||
            fabs(first->points[i].scalePercent -
                 second->points[i].scalePercent) > 0.000001) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL SnapshotsMatch(const AnimationSpeedSnapshot* first,
                           const AnimationSpeedSnapshot* second) {
    return first->metric == second->metric &&
           fabs(first->defaultScalePercent -
                second->defaultScalePercent) <= 0.000001 &&
           fabs(first->fixedScalePercent -
                second->fixedScalePercent) <= 0.000001 &&
           first->folderIntervalMs == second->folderIntervalMs &&
           first->minIntervalMs == second->minIntervalMs &&
           PointsMatch(first, second);
}

void AnimationConfig_ReplaceSnapshot(const AnimationSpeedSnapshot* snapshot) {
    if (!snapshot) return;
    AcquireSRWLockExclusive(&g_animationSpeedLock);
    if (!SnapshotsMatch(&g_animationSpeed, snapshot)) {
        g_animationSpeed = *snapshot;
    }
    ReleaseSRWLockExclusive(&g_animationSpeedLock);
}

void AnimationConfig_SetMetric(AnimationSpeedMetric metric) {
    AcquireSRWLockExclusive(&g_animationSpeedLock);
    g_animationSpeed.metric = AnimationConfig_NormalizeMetric(metric);
    ReleaseSRWLockExclusive(&g_animationSpeedLock);
}

void AnimationConfig_SetFixedMetric(double fixedScalePercent) {
    AcquireSRWLockExclusive(&g_animationSpeedLock);
    g_animationSpeed.metric = ANIMATION_SPEED_FIXED;
    g_animationSpeed.fixedScalePercent = fixedScalePercent;
    ReleaseSRWLockExclusive(&g_animationSpeedLock);
}

AnimationSpeedMetric GetAnimationSpeedMetric(void) {
    AnimationSpeedSnapshot snapshot;
    AnimationConfig_GetSnapshot(&snapshot);
    return snapshot.metric;
}

double GetAnimationFixedSpeedMultiplier(void) {
    AnimationSpeedSnapshot snapshot;
    AnimationConfig_GetSnapshot(&snapshot);
    return snapshot.fixedScalePercent / 100.0;
}

void SetAnimationSpeedRuntimeState(AnimationSpeedMetric metric,
                                   double fixedMultiplier) {
    double minimum = ANIMATION_FIXED_SPEED_MIN_MULTIPLIER;
    double maximum = ANIMATION_FIXED_SPEED_MAX_MULTIPLIER;

    if (DoubleIsNaNStrict(fixedMultiplier)) {
        fixedMultiplier = ANIMATION_FIXED_SPEED_DEFAULT_MULTIPLIER;
    } else if (!DoubleIsFiniteStrict(fixedMultiplier) ||
               fixedMultiplier > maximum) {
        fixedMultiplier = maximum;
    } else if (fixedMultiplier < minimum) {
        fixedMultiplier = minimum;
    }

    AcquireSRWLockExclusive(&g_animationSpeedLock);
    g_animationSpeed.metric = AnimationConfig_NormalizeMetric(metric);
    g_animationSpeed.fixedScalePercent = fixedMultiplier * 100.0;
    ReleaseSRWLockExclusive(&g_animationSpeedLock);
}

double GetAnimationSpeedScaleForPercent(double percent) {
    AnimationSpeedSnapshot snapshot;
    AnimationConfig_GetSnapshot(&snapshot);
    return AnimationConfig_Interpolate(percent, snapshot.points,
                                       snapshot.pointCount,
                                       snapshot.defaultScalePercent);
}
