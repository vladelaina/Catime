/**
 * @file tray_animation_timer_state.c
 * @brief Timer activation decisions for the current animation.
 */

#include "tray_animation_core_internal.h"

BOOL ShouldRunTrayAnimationTimer(void) {
    const LoadedAnimation* currentAnim = g_isPreviewActive ? &g_previewAnimation : &g_mainAnimation;
    return currentAnim->isAnimated && currentAnim->count > 1;
}

void EnsureTrayAnimationTimerState(void) {
    HWND trayHwnd = GetValidTrayAnimationWindow();
    if (!trayHwnd) {
        if (IsAnimationTimerActive()) {
            if (!CleanupAnimationTimer()) {
                LOG_WARNING("Animation timer cleanup timed out; retaining runtime resources");
            }
        }
        g_trayFrameDirty = FALSE;
        ClearPendingTrayUpdate();
        return;
    }

    BOOL shouldRun = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        shouldRun = ShouldRunTrayAnimationTimer();
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        shouldRun = ShouldRunTrayAnimationTimer();
    }

    if (shouldRun) {
        if (!IsAnimationTimerActive()) {
            FrameRateController_Init(&g_frameRateCtrl, TRAY_UPDATE_INTERVAL_MS);
            if (!InitializeAnimationTimer(trayHwnd, INTERNAL_TICK_INTERVAL_MS,
                                          TrayAnimationTimerCallback, NULL)) {
                LOG_WARNING("Failed to start tray animation timer (interval=%u)",
                            (unsigned)INTERNAL_TICK_INTERVAL_MS);
                ClearPendingTrayUpdate();
            }
        }
    } else {
        g_trayFrameDirty = FALSE;
        if (IsAnimationTimerActive()) {
            if (!CleanupAnimationTimer()) {
                LOG_WARNING("Animation timer cleanup timed out; retaining runtime resources");
            }
        }
    }
}
