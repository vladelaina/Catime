/**
 * @file tray_animation_playback.h
 * @brief Bounded, time-based animation frame advancement
 */

#ifndef TRAY_ANIMATION_PLAYBACK_H
#define TRAY_ANIMATION_PLAYBACK_H

#include <windows.h>

typedef struct {
    double elapsedMs;
    int logicalFrameIndex;
    int lastPresentedFrameIndex;
    int frameCount;
    BOOL initialized;
} AnimationPlaybackState;

#define ANIMATION_PLAYBACK_MAX_FRAMES 512

/** Reset accumulated time within the current frame. */
void AnimationPlayback_Reset(AnimationPlaybackState* state);

/**
 * Compute a monotonic tick delta and reject discontinuities such as sleep.
 * Returns fallbackElapsedMs on the first tick and 0 for a discontinuity.
 */
double AnimationPlayback_ComputeTickElapsedMs(ULONGLONG* lastTickMs,
                                              ULONGLONG nowMs,
                                              UINT fallbackElapsedMs,
                                              UINT discontinuityThresholdMs,
                                              BOOL* discontinuity);

/**
 * Advance an animation using real elapsed time.
 *
 * Full animation cycles are removed with fmod before walking frames, so the
 * function examines at most frameCount frames regardless of elapsedRealMs.
 * Logical phase is kept separately from the presented frame to prevent exact
 * sampling aliases from making a fast animation appear frozen.
 */
BOOL AnimationPlayback_Advance(AnimationPlaybackState* state,
                               const UINT* frameDelaysMs,
                               int frameCount,
                               UINT fallbackDelayMs,
                               double speedMultiplier,
                               UINT minimumFrameIntervalMs,
                               double elapsedRealMs,
                               int* frameIndex);

#endif /* TRAY_ANIMATION_PLAYBACK_H */
