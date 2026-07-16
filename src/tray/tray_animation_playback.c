/**
 * @file tray_animation_playback.c
 * @brief Bounded, time-based animation frame advancement
 */

#include "tray/tray_animation_playback.h"
#include "utils/finite_double.h"

#include <math.h>

#define PLAYBACK_MIN_EFFECTIVE_DELAY_MS 0.001

static double EffectiveFrameDelayMs(UINT configuredDelayMs,
                                    UINT fallbackDelayMs,
                                    double speedMultiplier,
                                    UINT minimumFrameIntervalMs) {
    UINT baseDelayMs = configuredDelayMs ? configuredDelayMs : fallbackDelayMs;
    if (baseDelayMs == 0) baseDelayMs = 1;
    if (!DoubleIsFiniteStrict(speedMultiplier) || speedMultiplier <= 0.0) {
        speedMultiplier = 1.0;
    }

    double delayMs = (double)baseDelayMs / speedMultiplier;
    if (minimumFrameIntervalMs > 0 && delayMs < (double)minimumFrameIntervalMs) {
        delayMs = (double)minimumFrameIntervalMs;
    }
    if (delayMs < PLAYBACK_MIN_EFFECTIVE_DELAY_MS) {
        delayMs = PLAYBACK_MIN_EFFECTIVE_DELAY_MS;
    }
    return delayMs;
}

void AnimationPlayback_Reset(AnimationPlaybackState* state) {
    if (!state) return;
    state->elapsedMs = 0.0;
    state->logicalFrameIndex = 0;
    state->lastPresentedFrameIndex = -1;
    state->frameCount = 0;
    state->initialized = FALSE;
}

double AnimationPlayback_ComputeTickElapsedMs(ULONGLONG* lastTickMs,
                                              ULONGLONG nowMs,
                                              UINT fallbackElapsedMs,
                                              UINT discontinuityThresholdMs,
                                              BOOL* discontinuity) {
    if (discontinuity) *discontinuity = FALSE;
    if (!lastTickMs) return 0.0;

    ULONGLONG previousTickMs = *lastTickMs;
    *lastTickMs = nowMs;
    if (previousTickMs == 0) return (double)fallbackElapsedMs;

    if (nowMs < previousTickMs ||
        nowMs - previousTickMs > (ULONGLONG)discontinuityThresholdMs) {
        if (discontinuity) *discontinuity = TRUE;
        return 0.0;
    }
    return (double)(nowMs - previousTickMs);
}

BOOL AnimationPlayback_Advance(AnimationPlaybackState* state,
                               const UINT* frameDelaysMs,
                               int frameCount,
                               UINT fallbackDelayMs,
                               double speedMultiplier,
                               UINT minimumFrameIntervalMs,
                               double elapsedRealMs,
                               int* frameIndex) {
    if (!state || !frameDelaysMs || !frameIndex || frameCount <= 1 ||
        frameCount > ANIMATION_PLAYBACK_MAX_FRAMES ||
        !DoubleIsFiniteStrict(elapsedRealMs) || elapsedRealMs <= 0.0) {
        return FALSE;
    }

    int presentedIndex = *frameIndex;
    if (presentedIndex < 0 || presentedIndex >= frameCount) presentedIndex = 0;

    if (!state->initialized || state->frameCount != frameCount) {
        state->logicalFrameIndex = presentedIndex;
        state->lastPresentedFrameIndex = presentedIndex;
        state->frameCount = frameCount;
        state->initialized = TRUE;
    }
    int index = state->logicalFrameIndex;
    if (index < 0 || index >= frameCount) index = 0;

    double cycleDurationMs = 0.0;
    for (int i = 0; i < frameCount; ++i) {
        cycleDurationMs += EffectiveFrameDelayMs(frameDelaysMs[i],
                                                 fallbackDelayMs,
                                                 speedMultiplier,
                                                 minimumFrameIntervalMs);
    }
    if (!DoubleIsFiniteStrict(cycleDurationMs) || cycleDurationMs <= 0.0) {
        AnimationPlayback_Reset(state);
        *frameIndex = index;
        return FALSE;
    }

    state->elapsedMs += elapsedRealMs;
    if (!DoubleIsFiniteStrict(state->elapsedMs) || state->elapsedMs < 0.0) {
        AnimationPlayback_Reset(state);
        *frameIndex = index;
        return FALSE;
    }

    BOOL phaseAdvanced = FALSE;
    if (state->elapsedMs >= cycleDurationMs) {
        state->elapsedMs = fmod(state->elapsedMs, cycleDurationMs);
        phaseAdvanced = TRUE;
    }

    for (int steps = 0; steps < frameCount; ++steps) {
        double currentDelayMs = EffectiveFrameDelayMs(frameDelaysMs[index],
                                                      fallbackDelayMs,
                                                      speedMultiplier,
                                                      minimumFrameIntervalMs);
        if (state->elapsedMs + 0.0000001 < currentDelayMs) break;

        state->elapsedMs -= currentDelayMs;
        if (state->elapsedMs < 0.0) state->elapsedMs = 0.0;
        index = (index + 1) % frameCount;
        phaseAdvanced = TRUE;
    }

    state->logicalFrameIndex = index;

    /* If sampling lands on the same logical frame after real phase movement,
     * alternate with its neighbor. This preserves logical timing while avoiding
     * a stroboscopic freeze at exact cycle/update-frequency ratios. */
    presentedIndex = index;
    if (phaseAdvanced && presentedIndex == state->lastPresentedFrameIndex) {
        presentedIndex = (presentedIndex + 1) % frameCount;
    }

    BOOL presentationChanged =
        presentedIndex != state->lastPresentedFrameIndex;
    state->lastPresentedFrameIndex = presentedIndex;
    *frameIndex = presentedIndex;
    return presentationChanged;
}
