/**
 * @file tray_animation_update_api.c
 * @brief Dynamic refresh and update-message public APIs.
 */

#include "tray_animation_core_internal.h"

BOOL TrayAnimation_GetBuiltinRefreshNeeds(BOOL* needsSystemMonitor) {
    if (needsSystemMonitor) *needsSystemMonitor = FALSE;
    if (!BeginTrayAnimationRuntimeUse()) return FALSE;

    BOOL dynamicBuiltin = FALSE;
    char animationName[MAX_PATH] = {0};
    AnimationSourceType sourceType = ANIM_SOURCE_UNKNOWN;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        BOOL previewActive = g_isPreviewActive;
        const LoadedAnimation* current =
            previewActive ? &g_previewAnimation : &g_mainAnimation;
        sourceType = current->sourceType;
        CopyStringExactA(previewActive ? g_previewAnimationName : g_animationName,
                         animationName, sizeof(animationName));
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        BOOL previewActive = g_isPreviewActive;
        const LoadedAnimation* current =
            previewActive ? &g_previewAnimation : &g_mainAnimation;
        sourceType = current->sourceType;
        CopyStringExactA(previewActive ? g_previewAnimationName : g_animationName,
                         animationName, sizeof(animationName));
    }

    dynamicBuiltin = sourceType == ANIM_SOURCE_PERCENT ||
                     sourceType == ANIM_SOURCE_CAPSLOCK;
    if (dynamicBuiltin && needsSystemMonitor) {
        *needsSystemMonitor =
            _stricmp(animationName, "__cpu__") == 0 ||
            _stricmp(animationName, "__mem__") == 0;
    }

    EndTrayAnimationRuntimeUse();
    return dynamicBuiltin;
}

/**
 * @brief Handle WM_TRAY_UPDATE_ICON message
 */
BOOL TrayAnimation_HandleUpdateMessage(HWND hwnd) {
    if (!BeginTrayAnimationRuntimeUse()) return FALSE;

    BOOL hasPending = FALSE;

    if (!IsValidTrayAnimationWindow(hwnd) || hwnd != g_trayHwnd) {
        goto done;
    }

    EnsureTrayAnimationTimerState();

    hasPending = HasPendingTrayUpdate();

    if (hasPending) {
        UpdateTrayIconToCurrentFrameInternal();
    }

done:
    EndTrayAnimationRuntimeUse();
    return TRUE;
}

void TrayAnimation_RefreshCurrentIcon(void) {
    if (!BeginTrayAnimationRuntimeUse()) return;
    ResetBuiltinIconUpdateCache();
    UpdateTrayIconToCurrentFrame();
    EndTrayAnimationRuntimeUse();
}

BOOL TrayAnimation_HasDeferredIconUpdate(void) {
    if (!BeginTrayAnimationRuntimeUse()) return FALSE;

    BOOL deferred = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        deferred = g_trayFrameDirty || g_pendingTrayUpdate;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        deferred = g_trayFrameDirty || g_pendingTrayUpdate;
    }

    EndTrayAnimationRuntimeUse();
    return deferred;
}
