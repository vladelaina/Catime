/**
 * @file tray_animation_speed_input.h
 * @brief User-input parsing for fixed tray animation speed
 */

#ifndef TRAY_ANIMATION_SPEED_INPUT_H
#define TRAY_ANIMATION_SPEED_INPUT_H

#include <windows.h>

/** Parse a fixed-speed value and silently clamp values above the maximum. */
BOOL TryParseFixedAnimationSpeed(const wchar_t* input, double* multiplier);

#endif /* TRAY_ANIMATION_SPEED_INPUT_H */
