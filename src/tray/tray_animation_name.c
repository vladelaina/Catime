/**
 * @file tray_animation_name.c
 * @brief Current-animation selection and startup retry handling.
 */

#include "tray_animation_core_internal.h"
#include "tray_animation_selection.h"

BOOL SetCurrentAnimationNameInternal(const char* name, BOOL persistConfig) {
    if (!name || !*name) return FALSE;
    if (!BeginTrayAnimationRuntimeUse()) return FALSE;

    BOOL result = FALSE;
    char requestedName[MAX_PATH] = {0};
    if (!CopyStringExactA(name, requestedName, sizeof(requestedName))) {
        LOG_WARNING("Ignoring animation name because it is too long: %s", name);
        goto done;
    }

    BOOL canReuseCurrentAnimation = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        canReuseCurrentAnimation = TrayAnimationSelection_CanReuseCurrent(
            g_animationName, requestedName, g_isPreviewActive,
            g_pendingPreviewName[0] != '\0');
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        canReuseCurrentAnimation = TrayAnimationSelection_CanReuseCurrent(
            g_animationName, requestedName, g_isPreviewActive,
            g_pendingPreviewName[0] != '\0');
    }

    /* A queued preview must still pass through the commit path so its worker
     * request is canceled before it can become active after this selection. */
    if (canReuseCurrentAnimation) {
        result = persistConfig ? WriteAnimationNameToConfigIfChanged(requestedName) : TRUE;
        goto done;
    }

    /* Validate animation exists */
    if (!IsValidAnimationSource(requestedName)) {
        goto done;
    }

    /* Seamless preview promotion */
    {
        LoadedAnimation oldMain;
        LoadedAnimation_Init(&oldMain);
        BOOL promotedPreview = FALSE;
        char oldAnimationName[MAX_PATH] = {0};
        int oldMainIndex = 0;
        int promotedPreviewIndex = 0;

        AcquireSRWLockExclusive(&g_previewWorkerLock);
        if (IsAnimCriticalSectionReady()) {
            EnterCriticalSection(&g_animCriticalSection);
        }

        BOOL canPromotePreview =
            g_isPreviewActive &&
            !g_previewAnimationFromPath &&
            g_previewAnimationName[0] != '\0' &&
            _stricmp(g_previewAnimationName, requestedName) == 0 &&
            (g_previewAnimation.count > 0 ||
             g_previewAnimation.sourceType == ANIM_SOURCE_PERCENT ||
             g_previewAnimation.sourceType == ANIM_SOURCE_CAPSLOCK);

        if (canPromotePreview) {
            CopyStringExactA(g_animationName, oldAnimationName, sizeof(oldAnimationName));
            oldMainIndex = g_mainIndex;
            promotedPreviewIndex = g_previewIndex;
            SwapLoadedAnimation(&oldMain, &g_mainAnimation);
            SwapLoadedAnimation(&g_mainAnimation, &g_previewAnimation);
            LoadedAnimation_Init(&g_previewAnimation);
            g_mainIndex = promotedPreviewIndex;

            /* Clear preview */
            g_previewIndex = 0;
            g_isPreviewActive = FALSE;
            g_previewAnimationFromPath = FALSE;
            g_previewAnimationName[0] = '\0';
            g_pendingPreviewFromPath = FALSE;
            g_pendingPreviewCommit = FALSE;
            g_pendingPreviewPersist = FALSE;
            g_pendingPreviewName[0] = '\0';
            InterlockedIncrement(&g_previewRequestSerial);
            CopyStringExactA(requestedName, g_animationName, sizeof(g_animationName));
            promotedPreview = TRUE;
        }

        if (IsAnimCriticalSectionReady()) {
            LeaveCriticalSection(&g_animCriticalSection);
        }
        if (promotedPreview) {
            SignalPreviewDecodeCancelLocked();
            WakePreviewWorkerLocked();
        }
        ReleaseSRWLockExclusive(&g_previewWorkerLock);

        if (promotedPreview) {
            if (persistConfig && !WriteAnimationNameToConfigIfChanged(requestedName)) {
                LoadedAnimation restoredPreview;
                LoadedAnimation_Init(&restoredPreview);

                AcquireSRWLockExclusive(&g_previewWorkerLock);
                if (IsAnimCriticalSectionReady()) {
                    EnterCriticalSection(&g_animCriticalSection);
                }

                SwapLoadedAnimation(&restoredPreview, &g_previewAnimation);
                SwapLoadedAnimation(&g_previewAnimation, &g_mainAnimation);
                SwapLoadedAnimation(&g_mainAnimation, &oldMain);
                CopyStringExactA(oldAnimationName, g_animationName,
                                 sizeof(g_animationName));
                g_mainIndex = oldMainIndex;
                g_previewIndex = promotedPreviewIndex;
                g_isPreviewActive = TRUE;
                g_previewAnimationFromPath = FALSE;
                CopyStringExactA(requestedName, g_previewAnimationName,
                                 sizeof(g_previewAnimationName));

                if (IsAnimCriticalSectionReady()) {
                    LeaveCriticalSection(&g_animCriticalSection);
                }
                ReleaseSRWLockExclusive(&g_previewWorkerLock);

                LoadedAnimation_Free(&restoredPreview);
                ResetBuiltinIconUpdateCache();
                EnsureTrayAnimationTimerState();
                UpdateTrayIconToCurrentFrame();
                result = FALSE;
                goto done;
            }

            ResetBuiltinIconUpdateCache();
            EnsureTrayAnimationTimerState();
            UpdateTrayIconToCurrentFrame();

            LoadedAnimation_Free(&oldMain);
            result = TRUE;
            goto done;
        }
    }

    result = QueueAnimationCommitRequest(requestedName, persistConfig);

done:
    EndTrayAnimationRuntimeUse();
    if (result) {
        RefreshTrayBackgroundWorkState();
    }
    return result;
}

/**
 * @brief Set current animation
 */
BOOL SetCurrentAnimationName(const char* name) {
    return SetCurrentAnimationNameInternal(name, TRUE);
}

void CancelStartupAnimationRetry(void) {
    if (g_startupRetryHwnd) {
        KillTimer(g_startupRetryHwnd, STARTUP_ANIMATION_RETRY_TIMER_ID);
    }
    g_startupRetryHwnd = NULL;
    g_startupRetryAnimationName[0] = '\0';
    g_startupRetryAttemptCount = 0;
}

void ScheduleStartupAnimationRetry(HWND hwnd, const char* animationName) {
    if (!IsValidTrayAnimationWindow(hwnd) || !animationName || !*animationName ||
        _stricmp(animationName, "__logo__") == 0 ||
        !CopyStringExactA(animationName, g_startupRetryAnimationName,
                          sizeof(g_startupRetryAnimationName))) {
        return;
    }

    g_startupRetryAttemptCount = 0;
    if (SetTimer(hwnd, STARTUP_ANIMATION_RETRY_TIMER_ID,
                 STARTUP_ANIMATION_RETRY_INTERVAL_MS,
                 TrayAnimationStartupRetryTimerProc) == 0) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Failed to schedule startup tray animation retry (error=%lu)",
                 GetLastError());
        g_startupRetryAnimationName[0] = '\0';
        return;
    }

    g_startupRetryHwnd = hwnd;
    WriteLog(LOG_LEVEL_INFO,
             "Scheduled retry for startup tray animation '%s'", animationName);
}

void CALLBACK TrayAnimationStartupRetryTimerProc(HWND hwnd, UINT msg,
                                                        UINT_PTR id, DWORD time) {
    (void)msg;
    (void)time;

    if (id != STARTUP_ANIMATION_RETRY_TIMER_ID) return;
    if (!IsValidTrayAnimationWindow(hwnd) || hwnd != g_startupRetryHwnd ||
        !IsTrayAnimationRuntimeActive()) {
        CancelStartupAnimationRetry();
        return;
    }

    char configPath[MAX_PATH] = {0};
    char configuredAnimation[MAX_PATH] = "__logo__";
    GetConfigPath(configPath, sizeof(configPath));
    ReadAnimationNameFromConfig(configuredAnimation,
                                sizeof(configuredAnimation), configPath);

    if (_stricmp(configuredAnimation, g_startupRetryAnimationName) != 0 ||
        _stricmp(g_animationName, configuredAnimation) == 0 ||
        _stricmp(g_animationName, "__logo__") != 0) {
        CancelStartupAnimationRetry();
        return;
    }

    g_startupRetryAttemptCount++;
    if (SetCurrentAnimationNameInternal(g_startupRetryAnimationName, FALSE)) {
        WriteLog(LOG_LEVEL_INFO,
                 "Recovered startup tray animation '%s' on retry %d",
                 configuredAnimation, g_startupRetryAttemptCount);
        CancelStartupAnimationRetry();
        return;
    }

    if (g_startupRetryAttemptCount >= STARTUP_ANIMATION_RETRY_MAX_ATTEMPTS) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Startup tray animation '%s' remained unavailable after %d retries; keeping its configuration for the next launch",
                 configuredAnimation, g_startupRetryAttemptCount);
        CancelStartupAnimationRetry();
    }
}
