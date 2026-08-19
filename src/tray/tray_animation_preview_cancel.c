/**
 * @file tray_animation_preview_cancel.c
 * @brief Preview cancellation and main-animation restoration
 */

#include "tray_animation_core_internal.h"

void CancelAnimationPreview(void) {
    LoadedAnimation oldPreview;
    LoadedAnimation_Init(&oldPreview);
    BOOL restoredMainAnimation = FALSE;

    if (!BeginTrayAnimationRuntimeUse()) return;

    AcquireSRWLockExclusive(&g_previewWorkerLock);
    InterlockedIncrement(&g_previewRequestSerial);

    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
    }

    BOOL hasPreviewState =
        g_isPreviewActive || g_pendingPreviewName[0] != '\0';
    if (!hasPreviewState) {
        if (IsAnimCriticalSectionReady()) {
            LeaveCriticalSection(&g_animCriticalSection);
        }
        ReleaseSRWLockExclusive(&g_previewWorkerLock);
        goto done;
    }

    g_isPreviewActive = FALSE;
    g_previewAnimationFromPath = FALSE;
    g_previewAnimationName[0] = '\0';
    g_pendingPreviewFromPath = FALSE;
    g_pendingPreviewCommit = FALSE;
    g_pendingPreviewPersist = FALSE;
    g_pendingPreviewName[0] = '\0';
    SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
    LoadedAnimation_Init(&g_previewAnimation);
    ResetFramePlaybackState();

    if (IsAnimCriticalSectionReady()) {
        LeaveCriticalSection(&g_animCriticalSection);
    }

    SignalPreviewDecodeCancelLocked();
    WakePreviewWorkerLocked();
    ReleaseSRWLockExclusive(&g_previewWorkerLock);

    EnsureTrayAnimationTimerState();
    UpdateTrayIconToCurrentFrameForPreview();
    restoredMainAnimation = TRUE;

done:
    LoadedAnimation_Free(&oldPreview);
    EndTrayAnimationRuntimeUse();
    if (restoredMainAnimation) {
        RefreshTrayBackgroundWorkState();
    }
}
