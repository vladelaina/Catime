/**
 * @file tray_animation_frame_dispatch.c
 * @brief Animation timer callback and update message dispatch.
 */

#include "tray_animation_core_internal.h"

void UpdateTrayIconToCurrentFrame(void) {
    UpdateTrayIconToCurrentFrameInternal();
}

void UpdateTrayIconToCurrentFrameForPreview(void) {
    UpdateTrayIconToCurrentFrameInternal();
}

/**
 * @brief Request tray update (thread-safe)
 */
void RequestTrayIconUpdate(void) {
    HWND trayHwnd = GetValidTrayAnimationWindow();
    if (!trayHwnd) return;

    if (ClaimPendingTrayUpdate()) {
        return;
    }

    if (!PostMessage(trayHwnd, WM_TRAY_UPDATE_ICON, 0, 0)) {
        ClearPendingTrayUpdate();
        MarkTrayFrameDirty();
    }
}

/**
 * @brief Timer callback (worker thread)
 */
void TrayAnimationTimerCallback(void* userData) {
    (void)userData;

    if (!BeginTrayAnimationRuntimeUse()) {
        return;
    }

    BOOL locked = IsAnimCriticalSectionReady();
    if (locked) {
        EnterCriticalSection(&g_animCriticalSection);
    }

    /* Skip logic for percent icons (updated separately) and __none__ (static). */
    if (!g_isPreviewActive && IsBuiltinAnimationName(g_animationName)) {
        if (locked) {
            LeaveCriticalSection(&g_animCriticalSection);
        }
        EndTrayAnimationRuntimeUse();
        return;
    }

    const LoadedAnimation* currentAnim = g_isPreviewActive ? &g_previewAnimation : &g_mainAnimation;
    int* currentIndex = g_isPreviewActive ? &g_previewIndex : &g_mainIndex;
    BOOL shouldRequestUpdate = FALSE;

    BOOL tickDiscontinuity = FALSE;
    double elapsedMs = AnimationPlayback_ComputeTickElapsedMs(
        &g_lastAnimationTickMs,
        GetTickCount64(),
        INTERNAL_TICK_INTERVAL_MS,
        ANIMATION_TICK_DISCONTINUITY_MS,
        &tickDiscontinuity);
    if (tickDiscontinuity) {
        AnimationPlayback_Reset(&g_playbackState);
        FrameRateController_Init(&g_frameRateCtrl, TRAY_UPDATE_INTERVAL_MS);
    }

    if (currentAnim->count > 0 && currentAnim->isAnimated) {
        double speedMultiplier = 1.0;
        UINT minimumFrameIntervalMs = 0;
        GetPlaybackSpeedSnapshot(&speedMultiplier, &minimumFrameIntervalMs);

        if (AnimationPlayback_Advance(&g_playbackState,
                                      currentAnim->delays,
                                      currentAnim->count,
                                      GetBaseFolderIntervalMs(),
                                      speedMultiplier,
                                      minimumFrameIntervalMs,
                                      elapsedMs,
                                      currentIndex)) {
            g_framePresentationSerial++;
            g_trayFrameDirty = TRUE;
        }
    }

    BOOL retryDue = AnimationUpdateBackoff_ShouldRetry(
        InterlockedCompareExchange(&g_shellUpdateBackoffActive, 0, 0) != 0,
        (DWORD)InterlockedCompareExchange(&g_lastFailedUpdateAttemptTick, 0, 0),
        GetTickCount(), UPDATE_FAILURE_BACKOFF_MS);
    if (FrameRateController_ShouldUpdateTray(&g_frameRateCtrl, elapsedMs) &&
        g_trayFrameDirty && retryDue) {
        g_trayFrameDirty = FALSE;
        shouldRequestUpdate = TRUE;
    }

    if (locked) {
        LeaveCriticalSection(&g_animCriticalSection);
    }

    if (shouldRequestUpdate) {
        RequestTrayIconUpdate();
        EndTrayAnimationRuntimeUse();
        return;
    }

    EndTrayAnimationRuntimeUse();
}

/**
 * @brief Start animation system
 */
