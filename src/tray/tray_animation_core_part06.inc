        EnterCriticalSection(&g_animCriticalSection);
        g_pendingPreviewFromPath = FALSE;
        g_pendingPreviewName[0] = '\0';
        g_previewAnimationFromPath = FALSE;
        g_previewAnimationName[0] = '\0';
        g_isPreviewActive = FALSE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_pendingPreviewFromPath = FALSE;
        g_pendingPreviewName[0] = '\0';
        g_previewAnimationFromPath = FALSE;
        g_previewAnimationName[0] = '\0';
        g_isPreviewActive = FALSE;
    }
    InterlockedIncrement(&g_previewRequestSerial);

    BOOL previewWorkerStopped = ShutdownPreviewWorker();
    if (!previewWorkerStopped) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Preview worker is still retiring; animation resources will be retained until process exit");
    }

    BOOL resourcesCanBeFreed = previewWorkerStopped;
    if (resourcesCanBeFreed) {
        LoadedAnimation_Free(&g_mainAnimation);
        LoadedAnimation_Free(&g_previewAnimation);
        g_mainAnimationPreloaded = FALSE;
        g_preloadedIconCx = 0;
        g_preloadedIconCy = 0;
        CleanupPercentIconCache();
        CleanupTransparentTrayIcon();
    }

    if (resourcesCanBeFreed && g_memoryPool) {
        MemoryPool_Destroy(g_memoryPool);
        g_memoryPool = NULL;
    }

    BOOL criticalSectionReady = WaitForLongToDiffer(
        &g_criticalSectionInitialized,
        ANIM_CS_INITIALIZING,
        ANIMATION_STATE_DRAIN_TIMEOUT_MS);
    if (!criticalSectionReady) {
        LOG_WARNING("Animation critical section shutdown wait timed out");
    }

    if (resourcesCanBeFreed && criticalSectionReady &&
        InterlockedCompareExchange(&g_criticalSectionInitialized, 0, 0) == ANIM_CS_INITIALIZED) {
        DeleteCriticalSection(&g_animCriticalSection);
        InterlockedExchange(&g_criticalSectionInitialized, ANIM_CS_UNINITIALIZED);
    }

    g_consecutiveUpdateFailures = 0;
    g_lastSuccessfulUpdateTime = 0;
    g_updateFailureReported = FALSE;
    g_lastPreviewShellFailureLogTick = 0;
    InterlockedExchange(&g_shellUpdateBackoffActive, 0);
    InterlockedExchange(&g_lastFailedUpdateAttemptTick, 0);
    g_pendingTrayUpdate = FALSE;
    g_trayFrameDirty = FALSE;
    ResetBuiltinIconUpdateCache();
    g_trayHwnd = NULL;
    RefreshTrayBackgroundWorkState();
}

/**
 * @brief Get current animation name
 */
const char* GetCurrentAnimationName(void) {
    return g_animationName;
}

static BOOL SetCurrentAnimationNameInternal(const char* name, BOOL persistConfig) {
    if (!name || !*name) return FALSE;
    if (!BeginTrayAnimationRuntimeUse()) return FALSE;

    BOOL result = FALSE;
    char requestedName[MAX_PATH] = {0};
    if (!CopyStringExactA(name, requestedName, sizeof(requestedName))) {
        LOG_WARNING("Ignoring animation name because it is too long: %s", name);
        goto done;
    }

    BOOL sameAnimationNoPreview = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        sameAnimationNoPreview = !g_isPreviewActive &&
                                 _stricmp(g_animationName, requestedName) == 0;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        sameAnimationNoPreview = !g_isPreviewActive &&
                                 _stricmp(g_animationName, requestedName) == 0;
    }

    /* Prevent redundant reloads if name is same and no preview is active */
    if (sameAnimationNoPreview) {
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

    LoadedAnimation newMain;
    LoadedAnimation oldMain;
    LoadedAnimation oldPreview;
    LoadedAnimation_Init(&newMain);
    LoadedAnimation_Init(&oldMain);
    LoadedAnimation_Init(&oldPreview);

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (!LoadAnimationByNameWithTemporaryPool(requestedName, &newMain, cx, cy)) {
        LoadedAnimation_Free(&newMain);
        goto done;
    }

    if (persistConfig && !WriteAnimationNameToConfigIfChanged(requestedName)) {
        LoadedAnimation_Free(&newMain);
        goto done;
    }

    BOOL canceledPreviewLoad = FALSE;
    AcquireSRWLockExclusive(&g_previewWorkerLock);
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
    }

    SwapLoadedAnimation(&oldMain, &g_mainAnimation);
    SwapLoadedAnimation(&g_mainAnimation, &newMain);
    LoadedAnimation_Init(&newMain);
    g_mainIndex = 0;
    ResetFramePlaybackState();

    if (g_isPreviewActive || g_pendingPreviewName[0] != '\0') {
        SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
        LoadedAnimation_Init(&g_previewAnimation);
        g_isPreviewActive = FALSE;
        g_previewAnimationFromPath = FALSE;
        g_previewAnimationName[0] = '\0';
        g_pendingPreviewFromPath = FALSE;
        g_pendingPreviewName[0] = '\0';
        InterlockedIncrement(&g_previewRequestSerial);
        canceledPreviewLoad = TRUE;
    }

    CopyStringExactA(requestedName, g_animationName, sizeof(g_animationName));

    if (IsAnimCriticalSectionReady()) {
        LeaveCriticalSection(&g_animCriticalSection);
    }
    if (canceledPreviewLoad) {
        SignalPreviewDecodeCancelLocked();
        WakePreviewWorkerLocked();
    }
    ReleaseSRWLockExclusive(&g_previewWorkerLock);

    LoadedAnimation_Free(&oldMain);
    LoadedAnimation_Free(&oldPreview);
