/**
 * @file config_animation_api.h
 * @brief Animation-speed configuration API.
 */

#ifndef CATIME_CONFIG_ANIMATION_API_H
#define CATIME_CONFIG_ANIMATION_API_H

#include "config/config_types.h"

AnimationSpeedMetric GetAnimationSpeedMetric(void);
BOOL WriteConfigAnimationSpeedMetric(AnimationSpeedMetric metric);
double GetAnimationFixedSpeedMultiplier(void);
BOOL WriteConfigAnimationFixedSpeed(double multiplier);
void SetAnimationSpeedRuntimeState(AnimationSpeedMetric metric,
                                   double fixedMultiplier);
double GetAnimationSpeedScaleForPercent(double percent);
void ReloadAnimationSpeedFromConfig(void);
BOOL WriteAnimationSpeedToConfig(const char* configPath);

#endif /* CATIME_CONFIG_ANIMATION_API_H */
