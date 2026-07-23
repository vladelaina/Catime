                RefreshTrayBackgroundWorkState();
            }
            if (RecordFailedUpdate()) {
                HandleRepeatedTrayUpdateFailure();
            }
        }
    }
}

static void UpdateTrayIconToCurrentFrame(void) {
    UpdateTrayIconToCurrentFrameInternal(FALSE);
}

static void UpdateTrayIconToCurrentFrameForPreview(void) {
    UpdateTrayIconToCurrentFrameInternal(TRUE);
}

static BOOL IsAnimationPreviewActiveSnapshot(void) {
    BOOL active = FALSE;
    BOOL locked = IsAnimCriticalSectionReady();
    if (locked) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    active = g_isPreviewActive;
    if (locked) {
        LeaveCriticalSection(&g_animCriticalSection);
    }
    return active;
}

/**
 * @brief Request tray update (thread-safe)
 */
static void RequestTrayIconUpdate(void) {
    HWND trayHwnd = GetValidTrayAnimationWindow();
    if (!trayHwnd) return;

    BOOL alreadyPending = FALSE;
    alreadyPending = HasPendingTrayUpdate();
    SetPendingTrayUpdate();

    if (alreadyPending) {
        return;
    }

    if (!PostMessage(trayHwnd, WM_TRAY_UPDATE_ICON, 0, 0)) {
        ClearPendingTrayUpdate();
    }
}

/**
 * @brief Timer callback (worker thread)
 */
static void TrayAnimationTimerCallback(void* userData) {
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

    BOOL presentationPaused =
        IsTrayTooltipActive() ||
        (IsTrayInteractionSuspended() && !g_isPreviewActive);
    BOOL retryDue = AnimationUpdateBackoff_ShouldRetry(
        InterlockedCompareExchange(&g_shellUpdateBackoffActive, 0, 0) != 0,
        (DWORD)InterlockedCompareExchange(&g_lastFailedUpdateAttemptTick, 0, 0),
        GetTickCount(), UPDATE_FAILURE_BACKOFF_MS);
    if (FrameRateController_ShouldUpdateTray(&g_frameRateCtrl, elapsedMs) &&
        g_trayFrameDirty && !presentationPaused && retryDue) {
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
void StartTrayAnimation(HWND hwnd, UINT intervalMs) {
    if (!QuiesceTrayAnimationRuntime()) {
        LOG_WARNING("Animation start request deferred until the old runtime retires");
        RefreshTrayBackgroundWorkState();
        return;
    }
    CancelStartupAnimationRetry();

    if (IsPreviewWorkerRetiringAfterCleanup()) {
        WriteLog(LOG_LEVEL_WARNING,
                 "StartTrayAnimation deferred because preview worker is still retiring");
        return;
    }

    if (!IsValidTrayAnimationWindow(hwnd)) {
        g_trayHwnd = NULL;
        g_pendingTrayUpdate = FALSE;
        g_trayFrameDirty = FALSE;
        return;
    }

    g_trayHwnd = hwnd;
    UINT baseIntervalMs = ClampAnimationIntervalMs(intervalMs);
    InterlockedExchange(&g_baseFolderInterval, (LONG)baseIntervalMs);
    g_pendingTrayUpdate = FALSE;
    g_trayFrameDirty = FALSE;

    /* Read folder interval from config */
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    int folderMs = ReadIniInt("Animation", "ANIMATION_FOLDER_INTERVAL_MS", (int)baseIntervalMs, config_path);
    if (folderMs > 0) {
        InterlockedExchange(&g_baseFolderInterval, (LONG)ClampAnimationIntervalMs((UINT)folderMs));
    }

    /* Initialize resources - use atomic operation for thread safety */
    if (InterlockedCompareExchange(&g_criticalSectionInitialized,
                                   ANIM_CS_INITIALIZING,
                                   ANIM_CS_UNINITIALIZED) == ANIM_CS_UNINITIALIZED) {
        InitializeCriticalSection(&g_animCriticalSection);
        InterlockedExchange(&g_criticalSectionInitialized, ANIM_CS_INITIALIZED);
    } else {
        if (!WaitForLongToDiffer(&g_criticalSectionInitialized,
                                 ANIM_CS_INITIALIZING,
                                 ANIMATION_STATE_DRAIN_TIMEOUT_MS)) {
            LOG_WARNING("Animation critical section initialization timed out");
            return;
        }
    }

    char configAnimationName[MAX_PATH];
    strncpy(configAnimationName, g_animationName, sizeof(configAnimationName) - 1);
    configAnimationName[sizeof(configAnimationName) - 1] = '\0';
    ReadAnimationNameFromConfig(configAnimationName, sizeof(configAnimationName), config_path);

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    BOOL reusePreloadedMain =
        g_mainAnimationPreloaded &&
        g_preloadedIconCx == cx &&
        g_preloadedIconCy == cy &&
        _stricmp(configAnimationName, g_animationName) == 0;

    if (!reusePreloadedMain) {
        LoadedAnimation_Free(&g_mainAnimation);
    }
    LoadedAnimation_Free(&g_previewAnimation);
    if (!reusePreloadedMain) {
        LoadedAnimation_Init(&g_mainAnimation);
    }
    LoadedAnimation_Init(&g_previewAnimation);
    g_pendingPreviewFromPath = FALSE;
    g_pendingPreviewName[0] = '\0';
    g_previewAnimationFromPath = FALSE;
    g_previewAnimationName[0] = '\0';
    g_mainIndex = 0;
    g_previewIndex = 0;
    g_isPreviewActive = FALSE;
    InterlockedExchange(&g_previewRequestSerial, 0);

    FrameRateController_Init(&g_frameRateCtrl, TRAY_UPDATE_INTERVAL_MS);
    ResetFramePlaybackState();

    strncpy(g_animationName, configAnimationName, sizeof(g_animationName) - 1);
    g_animationName[sizeof(g_animationName) - 1] = '\0';

    /* Load frames unless InitTrayIcon already preloaded the same animation. */
    BOOL startupAnimationLoadFailed = FALSE;
    if (!reusePreloadedMain &&
        !LoadAnimationByNameWithTemporaryPool(g_animationName, &g_mainAnimation, cx, cy) &&
        _stricmp(g_animationName, "__logo__") != 0) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Failed to load tray animation '%s' during startup; using logo temporarily and preserving the configured animation",
                 g_animationName);
        startupAnimationLoadFailed = TRUE;
        LoadedAnimation_Free(&g_mainAnimation);
        strncpy(g_animationName, "__logo__", sizeof(g_animationName) - 1);
        g_animationName[sizeof(g_animationName) - 1] = '\0';
        LoadAnimationByNameWithTemporaryPool(g_animationName, &g_mainAnimation, cx, cy);
    }
    g_mainAnimationPreloaded = FALSE;
    g_preloadedIconCx = 0;
    g_preloadedIconCy = 0;

    InterlockedExchange(&g_runtimeActive, 1);
    ResetBuiltinIconUpdateCache();

    if (startupAnimationLoadFailed) {
        ScheduleStartupAnimationRetry(hwnd, configAnimationName);
    }

    if (g_mainAnimation.count > 0) {
        UpdateTrayIconToCurrentFrame();
    }

    EnsureTrayAnimationTimerState();
    RefreshTrayBackgroundWorkState();
}

/**
 * @brief Stop animation system
 */
void StopTrayAnimation(HWND hwnd) {
    (void)hwnd;

    if (!QuiesceTrayAnimationRuntime()) {
        CancelStartupAnimationRetry();
        RefreshTrayBackgroundWorkState();
        return;
    }
    CancelStartupAnimationRetry();

    if (IsAnimCriticalSectionReady()) {
