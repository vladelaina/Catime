        SystemMonitor_GetMemoryUsage(&mem);
        percent = mem;
    }

    BOOL applyScaling = TRUE;
    if (metric == ANIMATION_SPEED_TIMER) {
        if (CLOCK_SHOW_CURRENT_TIME || CLOCK_COUNT_UP || CLOCK_TOTAL_TIME <= 0 || percent >= 100.0) {
            applyScaling = FALSE;
        }
    }

    double scalePercent = applyScaling ? GetAnimationSpeedScaleForPercent(percent) :
                                        GetAnimationSpeedScaleForPercent(0.0);
    return (scalePercent > 0.0) ? scalePercent : 100.0;
}

static BOOL IsSpeedScaleCacheFresh(DWORD now, LONG invalidationSerial) {
    return g_speedScaleCache.valid &&
           g_speedScaleCache.invalidationSerial == invalidationSerial &&
           (DWORD)(now - g_speedScaleCache.lastRefreshTick) < SPEED_SCALE_CACHE_TTL_MS;
}

static void RefreshSpeedScaleCache(DWORD now, LONG invalidationSerial) {
    AnimationSpeedMetric metric = GetAnimationSpeedMetric();

    g_speedScaleCache.scalePercent = ComputeAnimationSpeedScalePercent(metric);
    g_speedScaleCache.minIntervalMs = GetUserMinIntervalMs();
    g_speedScaleCache.lastRefreshTick = now;
    g_speedScaleCache.invalidationSerial = invalidationSerial;
    g_speedScaleCache.valid = TRUE;
}

static void GetPlaybackSpeedSnapshot(double* speedMultiplier,
                                     UINT* minimumFrameIntervalMs) {
    if (!speedMultiplier || !minimumFrameIntervalMs) return;
    DWORD now = GetTickCount();
    LONG invalidationSerial = InterlockedCompareExchange(&g_speedScaleCacheInvalidation, 0, 0);
    if (!IsSpeedScaleCacheFresh(now, invalidationSerial)) {
        RefreshSpeedScaleCache(now, invalidationSerial);
    }

    *speedMultiplier = g_speedScaleCache.scalePercent / 100.0;
    if (!DoubleIsFiniteStrict(*speedMultiplier) || *speedMultiplier < 0.1) {
        *speedMultiplier = 0.1;
    }
    *minimumFrameIntervalMs = g_speedScaleCache.minIntervalMs;
}

static void UpdateTrayIconToCurrentFrameInternal(void) {
    HWND trayHwnd = GetValidTrayAnimationWindow();
    if (!trayHwnd) {
        ClearPendingTrayUpdate();
        return;
    }

    ClearPendingTrayUpdate();
    ClearTrayFrameDirty();

    BOOL previewActive = FALSE;
    AnimationSourceType sourceType = ANIM_SOURCE_UNKNOWN;
    char targetName[MAX_PATH] = {0};
    HICON hIcon = NULL;
    BOOL locked = IsAnimCriticalSectionReady();
    BOOL shouldRecordBuiltinIcon = FALSE;
    int builtinIconValue = -1;
    COLORREF builtinTextColor = RGB(0, 0, 0);
    COLORREF builtinBgColor = TRANSPARENT_BG_AUTO;
    int builtinIconCx = 0;
    int builtinIconCy = 0;
    BOOL isAnimatedFrame = FALSE;
    int shellFrameIndex = -1;
    int shellFrameCount = 0;
    ULONGLONG playbackGeneration = 0;
    ULONGLONG presentationSerial = 0;

    if (locked) {
        EnterCriticalSection(&g_animCriticalSection);
    }

    previewActive = g_isPreviewActive;
    LoadedAnimation* currentAnim = previewActive ? &g_previewAnimation : &g_mainAnimation;
    int* currentIndex = previewActive ? &g_previewIndex : &g_mainIndex;
    const char* currentName = previewActive ? g_previewAnimationName : g_animationName;
    sourceType = currentAnim->sourceType;
    strncpy(targetName, currentName, sizeof(targetName) - 1);
    targetName[sizeof(targetName) - 1] = '\0';

    /* Handle transparent/none icon */
    if (_stricmp(targetName, "__none__") == 0) {
        HICON transparentIcon = GetTransparentTrayIcon();
        hIcon = transparentIcon ? CopyIcon(transparentIcon) : NULL;
        if (locked) LeaveCriticalSection(&g_animCriticalSection);
        goto applyIcon;
    }

    /* Handle percent icons - both normal and preview mode */
    if (sourceType == ANIM_SOURCE_PERCENT) {
        if (locked) LeaveCriticalSection(&g_animCriticalSection);

        int p = 0;

        const BuiltinAnimDef* def = GetBuiltinAnimDef(targetName);
        if (def && def->getValue) {
            p = def->getValue();
        }

        if (p < 0) p = 0;
        if (p > 100) p = 100;

        if (ShouldPreserveCachedPercentIconValue(targetName, p)) {
            return;
        }

        if (!previewActive &&
            GetPercentIconColorSnapshot(&builtinTextColor, &builtinBgColor)) {
            GetGeneratedTrayIconSizeSnapshot(&builtinIconCx, &builtinIconCy);
            builtinIconValue = p;
            shouldRecordBuiltinIcon = TRUE;
        }

        hIcon = CreatePercentIcon16(p);
        if (!hIcon) {
            WriteLog(LOG_LEVEL_ERROR, "Failed to create percent icon for %d%%", p);
        }
        goto applyIcon;
    }

    /* Handle Caps Lock indicator */
    if (sourceType == ANIM_SOURCE_CAPSLOCK) {
        if (locked) LeaveCriticalSection(&g_animCriticalSection);

        BOOL capsOn = IsCapsLockOn();
        if (!previewActive &&
            GetPercentIconColorSnapshot(&builtinTextColor, &builtinBgColor)) {
            GetGeneratedTrayIconSizeSnapshot(&builtinIconCx, &builtinIconCy);
            builtinIconValue = capsOn ? 1 : 0;
            shouldRecordBuiltinIcon = TRUE;
        }

        hIcon = CreateCapsLockIcon(capsOn);
        goto applyIcon;
    }

    if (currentAnim->count <= 0) {
        if (previewActive) {
            g_isPreviewActive = FALSE;
            if (locked) LeaveCriticalSection(&g_animCriticalSection);
            return;
        }
        if (locked) LeaveCriticalSection(&g_animCriticalSection);
        if (RecordFailedUpdate()) {
            HandleRepeatedTrayUpdateFailure();
        }
        return;
    }

    if (*currentIndex < 0 || *currentIndex >= currentAnim->count) *currentIndex = 0;

    int displayIndex = *currentIndex;
    if (currentAnim->isAnimated && currentAnim->count > 1) {
        playbackGeneration = g_playbackGeneration;
        presentationSerial = g_framePresentationSerial;
        if (g_lastShellAnimatedFrameValid &&
            g_lastShellFrameCount == currentAnim->count &&
            g_lastShellPresentationSerial != presentationSerial &&
            g_lastShellFrameIndex == displayIndex &&
            _stricmp(g_lastShellAnimationName, targetName) == 0) {
            displayIndex = (displayIndex + 1) % currentAnim->count;
        }
        isAnimatedFrame = TRUE;
        shellFrameIndex = displayIndex;
        shellFrameCount = currentAnim->count;
    }

    HICON currentIcon = currentAnim->icons[displayIndex];
    if (!currentIcon && displayIndex != *currentIndex) {
        displayIndex = *currentIndex;
        shellFrameIndex = displayIndex;
        currentIcon = currentAnim->icons[displayIndex];
    }
    if (!currentIcon) {
        if (previewActive) {
            g_isPreviewActive = FALSE;
            if (locked) LeaveCriticalSection(&g_animCriticalSection);
            return;
        }
        if (locked) LeaveCriticalSection(&g_animCriticalSection);
        if (RecordFailedUpdate()) {
            HandleRepeatedTrayUpdateFailure();
        }
        return;
    }

    /* Shell calls can block behind Explorer. Copy under the animation lock,
     * then release it before entering the Shell so timer cleanup stays bounded. */
    hIcon = CopyIcon(currentIcon);
    if (locked) LeaveCriticalSection(&g_animCriticalSection);

applyIcon:
    if (!hIcon) {
        if (!previewActive) {
            if (!isAnimatedFrame) {
                MarkTrayFrameDirty();
                RefreshTrayBackgroundWorkState();
            }
            if (RecordFailedUpdate()) {
                HandleRepeatedTrayUpdateFailure();
            }
        }
        return;
    }

    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = trayHwnd;
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON;
    nid.hIcon = hIcon;

    BOOL success = Shell_NotifyIconW(NIM_MODIFY, &nid);
    if (success) {
        TrayIconLifetime_Retain(hIcon);
    } else {
        DestroyIcon(hIcon);
    }

    if (success) {
        BOOL trackingLockReady = IsAnimCriticalSectionReady();
        if (trackingLockReady) EnterCriticalSection(&g_animCriticalSection);
        if (isAnimatedFrame && g_playbackGeneration == playbackGeneration) {
            g_lastShellAnimatedFrameValid = TRUE;
            g_lastShellFrameIndex = shellFrameIndex;
            g_lastShellFrameCount = shellFrameCount;
            g_lastShellPresentationSerial = presentationSerial;
            CopyStringExactA(targetName, g_lastShellAnimationName,
                             sizeof(g_lastShellAnimationName));
        } else {
            g_lastShellAnimatedFrameValid = FALSE;
            g_lastShellAnimationName[0] = '\0';
        }
        if (trackingLockReady) LeaveCriticalSection(&g_animCriticalSection);

        RecordSuccessfulUpdate();
        if (shouldRecordBuiltinIcon) {
            RecordBuiltinIconUpdateCache(targetName, builtinIconValue,
                                         builtinTextColor, builtinBgColor,
                                         builtinIconCx, builtinIconCy);
        }
    } else {
        if (previewActive) {
            LogPreviewShellFailureThrottled();
        } else {
            if (!isAnimatedFrame) {
                MarkTrayFrameDirty();
