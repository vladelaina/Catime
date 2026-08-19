/**
 * @file tray_animation_lifecycle.c
 * @brief Animation runtime start and stop lifecycle.
 */

#include "tray_animation_core_internal.h"

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
    g_pendingPreviewCommit = FALSE;
    g_pendingPreviewPersist = FALSE;
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
        EnterCriticalSection(&g_animCriticalSection);
        g_pendingPreviewFromPath = FALSE;
        g_pendingPreviewCommit = FALSE;
        g_pendingPreviewPersist = FALSE;
        g_pendingPreviewName[0] = '\0';
        g_previewAnimationFromPath = FALSE;
        g_previewAnimationName[0] = '\0';
        g_isPreviewActive = FALSE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_pendingPreviewFromPath = FALSE;
        g_pendingPreviewCommit = FALSE;
        g_pendingPreviewPersist = FALSE;
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
