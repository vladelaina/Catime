/**
 * @file tray_animation_startup.c
 * @brief Startup preload, hot-applied config, and interval APIs.
 */

#include "tray_animation_core_internal.h"

void PreloadAnimationFromConfig(void) {
    if (IsPreviewWorkerRetiringAfterCleanup()) {
        WriteLog(LOG_LEVEL_WARNING,
                 "PreloadAnimationFromConfig skipped because preview worker is still retiring");
        return;
    }

    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    ReadAnimationNameFromConfig(g_animationName, sizeof(g_animationName), config_path);

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    LoadedAnimation_Free(&g_mainAnimation);
    LoadedAnimation_Init(&g_mainAnimation);
    g_mainAnimationPreloaded = FALSE;
    g_preloadedIconCx = 0;
    g_preloadedIconCy = 0;
    BOOL loaded = LoadAnimationByNameWithTemporaryPool(g_animationName, &g_mainAnimation, cx, cy);
    if (!loaded &&
        _stricmp(g_animationName, "__logo__") != 0) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Failed to preload tray animation '%s'; using logo temporarily without changing the configured animation",
                 g_animationName);
        LoadedAnimation_Free(&g_mainAnimation);
        strncpy(g_animationName, "__logo__", sizeof(g_animationName) - 1);
        g_animationName[sizeof(g_animationName) - 1] = '\0';
        loaded = LoadAnimationByNameWithTemporaryPool(g_animationName, &g_mainAnimation, cx, cy);
    }
    if (loaded) {
        g_mainAnimationPreloaded = TRUE;
        g_preloadedIconCx = cx;
        g_preloadedIconCy = cy;
    }
}

/**
 * @brief Get initial animation icon
 */
HICON GetInitialAnimationHicon(void) {
    if (_stricmp(g_animationName, "__cpu__") == 0 || _stricmp(g_animationName, "__mem__") == 0 ||
        _stricmp(g_animationName, "__battery__") == 0) {
        return NULL;
    }

    /* Return transparent icon for __none__ */
    if (_stricmp(g_animationName, "__none__") == 0) {
        return GetTransparentTrayIcon();
    }

    if (g_mainAnimation.count > 0) {
        return g_mainAnimation.icons[0];
    }

    if (_stricmp(g_animationName, "__logo__") == 0) {
        return LoadIconW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_CATIME));
    }

    return NULL;
}

/**
 * @brief Apply animation path without persistence
 */
void ApplyAnimationPathValueNoPersist(const char* value) {
    if (!value || !*value) return;
    if (!BeginTrayAnimationRuntimeUse()) return;
    BOOL applied = FALSE;

    const char* prefix = "%LOCALAPPDATA%\\Catime\\resources\\animations\\";
    char name[MAX_PATH] = {0};
    BOOL copiedName = FALSE;

    if (IsBuiltinAnimationName(value)) {
        copiedName = CopyStringExactA(value, name, sizeof(name));
    } else if (_strnicmp(value, prefix, (int)strlen(prefix)) == 0) {
        const char* rel = value + strlen(prefix);
        copiedName = CopyStringExactA(rel, name, sizeof(name));
    } else {
        copiedName = CopyStringExactA(value, name, sizeof(name));
    }

    if (!copiedName) {
        LOG_WARNING("Ignoring animation config path because it is too long: %s", value);
        goto done;
    }
    if (name[0] == '\0') goto done;
    if (_stricmp(g_animationName, name) == 0) goto done;


    LoadedAnimation newMain;
    LoadedAnimation oldMain;
    LoadedAnimation oldPreview;
    LoadedAnimation_Init(&newMain);
    LoadedAnimation_Init(&oldMain);
    LoadedAnimation_Init(&oldPreview);

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (!LoadAnimationByNameWithTemporaryPool(name, &newMain, cx, cy)) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Ignoring hot-reloaded tray animation '%s' because it could not be loaded",
                 name);
        LoadedAnimation_Free(&newMain);
        goto done;
    }

    if (_stricmp(g_animationName, name) == 0) {
        LoadedAnimation_Free(&newMain);
        goto done;
    }

    BOOL canceledPreviewLoad = FALSE;
    AcquireSRWLockExclusive(&g_previewWorkerLock);
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    CopyStringExactA(name, g_animationName, sizeof(g_animationName));
    SwapLoadedAnimation(&oldMain, &g_mainAnimation);
    SwapLoadedAnimation(&g_mainAnimation, &newMain);
    LoadedAnimation_Init(&newMain);
    g_mainIndex = 0;
    ResetFramePlaybackState();

    if (g_isPreviewActive || g_pendingPreviewName[0] != '\0') {
        g_isPreviewActive = FALSE;
        g_previewAnimationFromPath = FALSE;
        g_previewAnimationName[0] = '\0';
        g_pendingPreviewFromPath = FALSE;
        g_pendingPreviewName[0] = '\0';
        InterlockedIncrement(&g_previewRequestSerial);
        canceledPreviewLoad = TRUE;
        SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
        LoadedAnimation_Init(&g_previewAnimation);
    }

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

    ResetBuiltinIconUpdateCache();
    EnsureTrayAnimationTimerState();
    if (g_mainAnimation.count > 0) {
        UpdateTrayIconToCurrentFrame();
    }
    applied = TRUE;

done:
    EndTrayAnimationRuntimeUse();
    if (applied) {
        RefreshTrayBackgroundWorkState();
    }
}

/**
 * @brief Set base interval
 */
void TrayAnimation_SetBaseIntervalMs(UINT ms) {
    LONG interval = (LONG)ClampAnimationIntervalMs(ms);
    if (InterlockedCompareExchange(&g_baseFolderInterval, interval, interval) == interval) {
        return;
    }
    InterlockedExchange(&g_baseFolderInterval, interval);
    InvalidateSpeedScaleCache();
}

/**
 * @brief Set minimum interval
 */
void TrayAnimation_SetMinIntervalMs(UINT ms) {
    LONG interval = (LONG)ClampAnimationMinIntervalMs(ms);
    if (InterlockedCompareExchange(&g_userMinIntervalMs, interval, interval) == interval) {
        return;
    }
    InterlockedExchange(&g_userMinIntervalMs, interval);
    InvalidateSpeedScaleCache();
}

/**
 * @brief Invalidate cached timer speed scaling
 */
void TrayAnimation_RecomputeTimerDelay(void) {
    InvalidateSpeedScaleCache();
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        ResetFramePlaybackState();
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        ResetFramePlaybackState();
    }
    RefreshTrayBackgroundWorkState();
}

/**
 * @brief Clear current animation name to force reload
 */
void TrayAnimation_ClearCurrentName(void) {
    if (!BeginTrayAnimationRuntimeUse()) return;

    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    g_animationName[0] = '\0';
    if (IsAnimCriticalSectionReady()) {
        LeaveCriticalSection(&g_animCriticalSection);
    }

    EndTrayAnimationRuntimeUse();
}

/**
 * @brief Update percent icon if needed
 */
