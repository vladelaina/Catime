#include "tray/tray_animation_playback.h"

#include <math.h>
#include <stdio.h>

static int g_failures = 0;

static void ExpectTrue(const char* name, BOOL value) {
    if (!value) {
        fprintf(stderr, "%s: expected true\n", name);
        g_failures++;
    }
}

static void ExpectFalse(const char* name, BOOL value) {
    if (value) {
        fprintf(stderr, "%s: expected false\n", name);
        g_failures++;
    }
}

static void ExpectInt(const char* name, int actual, int expected) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
        g_failures++;
    }
}

static double ReferenceEffectiveDelay(UINT delayMs, UINT fallbackDelayMs,
                                      double speedMultiplier,
                                      UINT minimumFrameIntervalMs) {
    UINT baseDelayMs = delayMs ? delayMs : fallbackDelayMs;
    if (baseDelayMs == 0) baseDelayMs = 1;
    double effectiveMs = (double)baseDelayMs / speedMultiplier;
    if (minimumFrameIntervalMs > 0 &&
        effectiveMs < (double)minimumFrameIntervalMs) {
        effectiveMs = (double)minimumFrameIntervalMs;
    }
    return effectiveMs < 0.001 ? 0.001 : effectiveMs;
}

static void TestThirtyTimesDoesNotLockToOneFrame(void) {
    const UINT delays[8] = {160, 160, 160, 160, 160, 160, 160, 160};
    AnimationPlaybackState state = {0};
    int index = 0;
    BOOL seen[8] = {FALSE};

    for (int sample = 0; sample < 8; ++sample) {
        ExpectTrue("30x frame advancement",
                   AnimationPlayback_Advance(&state, delays, 8, 150,
                                             30.0, 0, 160.0, &index));
        seen[index] = TRUE;
    }

    int uniqueFrames = 0;
    for (int i = 0; i < 8; ++i) {
        if (seen[i]) uniqueFrames++;
    }
    if (uniqueFrames <= 1) {
        fprintf(stderr, "30x lockstep: expected multiple displayed frames, got %d\n",
                uniqueFrames);
        g_failures++;
    }
}

static void TestThirtyTimesAtTrayCadence(void) {
    const UINT delays[8] = {160, 160, 160, 160, 160, 160, 160, 160};
    AnimationPlaybackState state = {0};
    int index = 0;
    int previousIndex = index;
    int changes = 0;

    for (int sample = 0; sample < 20; ++sample) {
        (void)AnimationPlayback_Advance(&state, delays, 8, 150,
                                        30.0, 0, 50.0, &index);
        if (index != previousIndex) changes++;
        previousIndex = index;
    }

    if (changes < 15) {
        fprintf(stderr, "30x tray cadence: expected visible motion, got %d changes\n",
                changes);
        g_failures++;
    }
}

static void TestExactCycleSamplingDoesNotFreeze(void) {
    const UINT delays[8] = {160, 160, 160, 160, 160, 160, 160, 160};
    AnimationPlaybackState state = {0};
    int index = 0;
    int previousIndex = index;

    /* At 25.6x the complete 8-frame cycle is exactly 50ms. */
    for (int sample = 0; sample < 20; ++sample) {
        ExpectTrue("exact cycle presentation changes",
                   AnimationPlayback_Advance(&state, delays, 8, 150,
                                             25.6, 0, 50.0, &index));
        if (index == previousIndex) {
            fprintf(stderr, "exact-cycle sampling presented the same frame\n");
            g_failures++;
            return;
        }
        ExpectInt("exact cycle logical phase", state.logicalFrameIndex, 0);
        previousIndex = index;
    }
}

static void TestVariableFrameDelays(void) {
    const UINT delays[3] = {100, 200, 300};
    AnimationPlaybackState state = {0};
    int index = 0;

    ExpectTrue("variable delays advance",
               AnimationPlayback_Advance(&state, delays, 3, 150,
                                         2.0, 0, 150.0, &index));
    ExpectInt("variable delays index", index, 2);
}

static void TestMinimumInterval(void) {
    const UINT delays[2] = {20, 20};
    AnimationPlaybackState state = {0};
    int index = 0;

    ExpectFalse("minimum interval holds frame",
                AnimationPlayback_Advance(&state, delays, 2, 150,
                                          30.0, 20, 19.0, &index));
    ExpectInt("minimum interval held index", index, 0);
    ExpectTrue("minimum interval advances",
               AnimationPlayback_Advance(&state, delays, 2, 150,
                                         30.0, 20, 1.0, &index));
    ExpectInt("minimum interval next index", index, 1);
}

static void TestHugeElapsedTimeIsBounded(void) {
    UINT delays[ANIMATION_PLAYBACK_MAX_FRAMES];
    for (int i = 0; i < ANIMATION_PLAYBACK_MAX_FRAMES; ++i) delays[i] = 20;

    AnimationPlaybackState state = {0};
    int index = 0;
    (void)AnimationPlayback_Advance(&state, delays,
                                    ANIMATION_PLAYBACK_MAX_FRAMES,
                                    150, 30.0, 0, 1.0e15, &index);
    if (index < 0 || index >= ANIMATION_PLAYBACK_MAX_FRAMES ||
        !isfinite(state.elapsedMs)) {
        fprintf(stderr, "huge elapsed time produced invalid state\n");
        g_failures++;
    }

    ExpectFalse("frame count guard",
                AnimationPlayback_Advance(&state, delays,
                                          ANIMATION_PLAYBACK_MAX_FRAMES + 1,
                                          150, 30.0, 0, 50.0, &index));
}

static void TestInvalidInputsPreserveState(void) {
    const UINT delays[2] = {100, 100};
    AnimationPlaybackState state = {12.5};
    int index = 1;

    ExpectFalse("NaN elapsed rejected",
                AnimationPlayback_Advance(&state, delays, 2, 150,
                                          30.0, 0, NAN, &index));
    ExpectInt("NaN elapsed preserves index", index, 1);
    ExpectFalse("negative elapsed rejected",
                AnimationPlayback_Advance(&state, delays, 2, 150,
                                          30.0, 0, -1.0, &index));
    ExpectInt("negative elapsed preserves index", index, 1);
}

static void TestOptimizedAdvanceMatchesReference(void) {
    const UINT delays[7] = {0, 1, 7, 20, 83, 160, 997};
    const double speeds[] = {0.1, 0.5, 1.0, 2.0, 10.0, 30.0};
    const UINT minimumIntervals[] = {0, 10, 50};
    unsigned int randomState = 0x5a17c9e3u;

    for (size_t speedIndex = 0;
         speedIndex < sizeof(speeds) / sizeof(speeds[0]);
         ++speedIndex) {
        for (size_t minIndex = 0;
             minIndex < sizeof(minimumIntervals) / sizeof(minimumIntervals[0]);
             ++minIndex) {
            AnimationPlaybackState optimized = {0};
            double referenceElapsedMs = 0.0;
            int optimizedIndex = 0;
            int referenceIndex = 0;

            for (int sample = 0; sample < 2000; ++sample) {
                randomState = randomState * 1664525u + 1013904223u;
                double elapsedMs = 1.0 + (double)(randomState % 200u);

                (void)AnimationPlayback_Advance(
                    &optimized, delays, 7, 150, speeds[speedIndex],
                    minimumIntervals[minIndex], elapsedMs, &optimizedIndex);

                referenceElapsedMs += elapsedMs;
                int referenceSteps = 0;
                for (;;) {
                    double currentDelayMs = ReferenceEffectiveDelay(
                        delays[referenceIndex], 150, speeds[speedIndex],
                        minimumIntervals[minIndex]);
                    if (referenceElapsedMs + 0.0000001 < currentDelayMs) break;
                    referenceElapsedMs -= currentDelayMs;
                    if (referenceElapsedMs < 0.0) referenceElapsedMs = 0.0;
                    referenceIndex = (referenceIndex + 1) % 7;
                    if (++referenceSteps > 100000) {
                        fprintf(stderr, "reference playback exceeded step guard\n");
                        g_failures++;
                        return;
                    }
                }

                if (optimized.logicalFrameIndex != referenceIndex ||
                    fabs(optimized.elapsedMs - referenceElapsedMs) > 0.00001) {
                    fprintf(stderr,
                            "optimized playback diverged at speed %.3g, min %u, sample %d\n",
                            speeds[speedIndex], minimumIntervals[minIndex], sample);
                    g_failures++;
                    return;
                }
            }
        }
    }
}

static void TestTickDiscontinuity(void) {
    ULONGLONG lastTick = 0;
    BOOL discontinuity = FALSE;

    double elapsed = AnimationPlayback_ComputeTickElapsedMs(
        &lastTick, 1000, 50, 1000, &discontinuity);
    ExpectInt("first tick fallback", (int)elapsed, 50);
    ExpectFalse("first tick continuity", discontinuity);

    elapsed = AnimationPlayback_ComputeTickElapsedMs(
        &lastTick, 1050, 50, 1000, &discontinuity);
    ExpectInt("normal tick elapsed", (int)elapsed, 50);
    ExpectFalse("normal tick continuity", discontinuity);

    elapsed = AnimationPlayback_ComputeTickElapsedMs(
        &lastTick, 5000, 50, 1000, &discontinuity);
    ExpectInt("sleep tick ignored", (int)elapsed, 0);
    ExpectTrue("sleep discontinuity", discontinuity);

    elapsed = AnimationPlayback_ComputeTickElapsedMs(
        &lastTick, 10, 50, 1000, &discontinuity);
    ExpectInt("backward tick ignored", (int)elapsed, 0);
    ExpectTrue("backward discontinuity", discontinuity);
}

int main(void) {
    TestThirtyTimesDoesNotLockToOneFrame();
    TestThirtyTimesAtTrayCadence();
    TestExactCycleSamplingDoesNotFreeze();
    TestVariableFrameDelays();
    TestMinimumInterval();
    TestHugeElapsedTimeIsBounded();
    TestInvalidInputsPreserveState();
    TestOptimizedAdvanceMatchesReference();
    TestTickDiscontinuity();

    if (g_failures != 0) {
        fprintf(stderr, "%d tray animation playback test(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
