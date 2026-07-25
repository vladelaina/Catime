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
static void UpdatePercentIconIfNeededInternal(BOOL hasMetricsSnapshot,
                                              float cpuPercent,
                                              float memPercent) {
    if (!BeginTrayAnimationRuntimeUse()) return;

    HWND trayHwnd = GetValidTrayAnimationWindow();
    if (!trayHwnd) goto done;
    /* Replacing the icon while Explorer owns a hover tooltip can make the
     * notification area briefly tear down and recreate that tooltip.  A
     * popup menu does not own that tooltip, so dynamic built-in icons may
     * continue refreshing while either tray menu is open. */
    if (IsTrayTooltipActive()) goto done;

    char animationName[MAX_PATH] = {0};
    BOOL previewActive = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        previewActive = g_isPreviewActive;
        CopyStringExactA(previewActive ? g_previewAnimationName : g_animationName,
                         animationName, sizeof(animationName));
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        previewActive = g_isPreviewActive;
        CopyStringExactA(previewActive ? g_previewAnimationName : g_animationName,
                         animationName, sizeof(animationName));
    }

    if (!animationName[0]) goto done;

    const BuiltinAnimDef* def = GetBuiltinAnimDef(animationName);
    if (!def) goto done;

    HICON hIcon = NULL;
    int value = -1;
    COLORREF textColor = RGB(0, 0, 0);
    COLORREF bgColor = TRANSPARENT_BG_AUTO;
    int iconCx = 0;
    int iconCy = 0;
    if (!GetPercentIconColorSnapshot(&textColor, &bgColor)) goto done;
    GetGeneratedTrayIconSizeSnapshot(&iconCx, &iconCy);

    /* Handle percent type (CPU, Memory, Battery) */
    if (def->type == ANIM_SOURCE_PERCENT) {
        int p = 0;
        if (hasMetricsSnapshot && _stricmp(animationName, "__cpu__") == 0) {
            p = (int)(cpuPercent + 0.5f);
        } else if (hasMetricsSnapshot && _stricmp(animationName, "__mem__") == 0) {
            p = (int)(memPercent + 0.5f);
        } else if (def->getValue) {
            p = def->getValue();
        }
        if (p < 0) p = 0;
        if (p > 100) p = 100;
        value = p;
        if (ShouldPreserveCachedPercentIconValue(animationName, value)) {
            goto done;
        }
        if (IsBuiltinIconUpdateCacheCurrent(animationName, value,
                                            textColor, bgColor,
                                            iconCx, iconCy)) {
            goto done;
        }
        hIcon = CreatePercentIcon16(p);
    }
    /* Handle Caps Lock indicator */
    else if (def->type == ANIM_SOURCE_CAPSLOCK) {
        value = IsCapsLockOn() ? 1 : 0;
        if (IsBuiltinIconUpdateCacheCurrent(animationName, value,
                                            textColor, bgColor,
                                            iconCx, iconCy)) {
            goto done;
        }
        hIcon = CreateCapsLockIcon(value != 0);
    }
    else {
        goto done;
    }

    if (!hIcon) goto done;

    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = trayHwnd;
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON;
    nid.hIcon = hIcon;
    if (Shell_NotifyIconW(NIM_MODIFY, &nid)) {
        TrayIconLifetime_Retain(hIcon);
        hIcon = NULL;
        RecordBuiltinIconUpdateCache(animationName, value,
                                     textColor, bgColor,
                                     iconCx, iconCy);
    }

    if (hIcon) DestroyIcon(hIcon);

done:
    EndTrayAnimationRuntimeUse();
}

void TrayAnimation_UpdatePercentIconIfNeeded(void) {
    UpdatePercentIconIfNeededInternal(FALSE, 0.0f, 0.0f);
}

void TrayAnimation_UpdatePercentIconWithMetrics(float cpuPercent, float memPercent) {
    UpdatePercentIconIfNeededInternal(TRUE, cpuPercent, memPercent);
}

BOOL TrayAnimation_GetBuiltinRefreshNeeds(BOOL* needsSystemMonitor) {
    if (needsSystemMonitor) *needsSystemMonitor = FALSE;
    if (!BeginTrayAnimationRuntimeUse()) return FALSE;

    BOOL dynamicBuiltin = FALSE;
    char animationName[MAX_PATH] = {0};
    AnimationSourceType sourceType = ANIM_SOURCE_UNKNOWN;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        BOOL previewActive = g_isPreviewActive;
        LoadedAnimation* current = previewActive ? &g_previewAnimation : &g_mainAnimation;
        sourceType = current->sourceType;
        CopyStringExactA(previewActive ? g_previewAnimationName : g_animationName,
                         animationName, sizeof(animationName));
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        BOOL previewActive = g_isPreviewActive;
        LoadedAnimation* current = previewActive ? &g_previewAnimation : &g_mainAnimation;
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
