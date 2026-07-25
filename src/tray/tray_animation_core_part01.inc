}

static UINT GetBaseFolderIntervalMs(void) {
    LONG interval = InterlockedCompareExchange(&g_baseFolderInterval, 0, 0);
    return interval > 0 ? (UINT)interval : TRAY_ANIMATION_DEFAULT_INTERVAL_MS;
}

static UINT GetUserMinIntervalMs(void) {
    LONG interval = InterlockedCompareExchange(&g_userMinIntervalMs, 0, 0);
    return interval > 0 ? (UINT)interval : 0;
}

static UINT ClampAnimationIntervalMs(UINT ms) {
    if (ms == 0) return TRAY_ANIMATION_DEFAULT_INTERVAL_MS;
    if (ms < TRAY_ANIMATION_MIN_INTERVAL_MS) return TRAY_ANIMATION_MIN_INTERVAL_MS;
    if (ms > TRAY_ANIMATION_MAX_INTERVAL_MS) return TRAY_ANIMATION_MAX_INTERVAL_MS;
    return ms;
}

static UINT ClampAnimationMinIntervalMs(UINT ms) {
    if (ms == 0) return 0;
    return ClampAnimationIntervalMs(ms);
}

static BOOL BeginTrayAnimationRuntimeUse(void) {
    if (!IsTrayAnimationRuntimeActive()) {
        return FALSE;
    }

    InterlockedIncrement(&g_runtimeUsers);
    if (!IsTrayAnimationRuntimeActive()) {
        InterlockedDecrement(&g_runtimeUsers);
        return FALSE;
    }

    return TRUE;
}

static void EndTrayAnimationRuntimeUse(void) {
    InterlockedDecrement(&g_runtimeUsers);
}

static BOOL QuiesceTrayAnimationRuntime(void) {
    InterlockedExchange(&g_runtimeActive, 0);
    BOOL timerStopped = CleanupAnimationTimer();
    BOOL runtimeDrained = WaitForLongValue(&g_runtimeUsers, 0,
                                           ANIMATION_STATE_DRAIN_TIMEOUT_MS);
    if (!timerStopped) {
        LOG_WARNING("Animation timer callback drain timed out; retaining runtime resources");
    }
    if (!runtimeDrained) {
        LOG_WARNING("Animation runtime drain timed out; retaining runtime resources");
    }
    return timerStopped && runtimeDrained;
}

static HICON GetTransparentTrayIcon(void) {
    if (!g_transparentTrayIcon) {
        BYTE andMask[32];
        BYTE xorMask[32];
        memset(andMask, 0xFF, sizeof(andMask));
        memset(xorMask, 0x00, sizeof(xorMask));
        g_transparentTrayIcon = CreateIcon(NULL, 16, 16, 1, 1, andMask, xorMask);
    }

    return g_transparentTrayIcon;
}

static void CleanupTransparentTrayIcon(void) {
    if (g_transparentTrayIcon) {
        DestroyIcon(g_transparentTrayIcon);
        g_transparentTrayIcon = NULL;
    }
}

static void ClearPendingTrayUpdate(void) {
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        g_pendingTrayUpdate = FALSE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_pendingTrayUpdate = FALSE;
    }
}

/*
 * Atomically mark an icon update as pending and report whether another
 * message already owns the pending slot.  RequestTrayIconUpdate used to do
 * this as a separate read followed by a write; the UI thread could consume
 * and clear the slot between those operations, leaving the animation dirty
 * with no message left to present it.
 */
static BOOL ClaimPendingTrayUpdate(void) {
    BOOL alreadyPending = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        alreadyPending = g_pendingTrayUpdate;
        g_pendingTrayUpdate = TRUE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        alreadyPending = g_pendingTrayUpdate;
        g_pendingTrayUpdate = TRUE;
    }
    return alreadyPending;
}

static BOOL HasPendingTrayUpdate(void) {
    BOOL pending = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        pending = g_pendingTrayUpdate;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        pending = g_pendingTrayUpdate;
    }
    return pending;
}

static void MarkTrayFrameDirty(void) {
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        g_trayFrameDirty = TRUE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_trayFrameDirty = TRUE;
    }
}

static void ClearTrayFrameDirty(void) {
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        g_trayFrameDirty = FALSE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_trayFrameDirty = FALSE;
    }
}

static void ResetBuiltinIconUpdateCache(void) {
    g_lastBuiltinIconName[0] = '\0';
    g_lastBuiltinIconValue = -1;
    g_lastBuiltinIconTextColor = CLR_INVALID;
    g_lastBuiltinIconBgColor = CLR_INVALID;
    g_lastBuiltinIconCx = 0;
    g_lastBuiltinIconCy = 0;
}

static BOOL IsBuiltinIconUpdateCacheCurrent(const char* name,
                                            int value,
                                            COLORREF textColor,
                                            COLORREF bgColor,
                                            int iconCx,
                                            int iconCy) {
    return name &&
           _stricmp(g_lastBuiltinIconName, name) == 0 &&
           g_lastBuiltinIconValue == value &&
           g_lastBuiltinIconTextColor == textColor &&
           g_lastBuiltinIconBgColor == bgColor &&
           g_lastBuiltinIconCx == iconCx &&
           g_lastBuiltinIconCy == iconCy;
}

static BOOL TryGetCachedBuiltinIconValue(const char* name, int* value) {
    if (!name || !value) {
        return FALSE;
    }

    if (g_lastBuiltinIconValue >= 0 &&
        _stricmp(g_lastBuiltinIconName, name) == 0) {
        *value = g_lastBuiltinIconValue;
        return TRUE;
    }

    if (g_lastStablePercentIconValue >= 0 &&
        _stricmp(g_lastStablePercentIconName, name) == 0) {
        *value = g_lastStablePercentIconValue;
        return TRUE;
    }

    return FALSE;
}

static BOOL IsTransientZeroPronePercentIcon(const char* name) {
    return name &&
           (_stricmp(name, "__cpu__") == 0 ||
            _stricmp(name, "__mem__") == 0);
}

static BOOL ShouldPreserveCachedPercentIconValue(const char* name, int sampledValue) {
    int cachedValue = -1;
    return sampledValue == 0 &&
           IsTransientZeroPronePercentIcon(name) &&
           (!TryGetCachedBuiltinIconValue(name, &cachedValue) || cachedValue > 0);
}

static void RecordBuiltinIconUpdateCache(const char* name,
                                         int value,
                                         COLORREF textColor,
                                         COLORREF bgColor,
                                         int iconCx,
                                         int iconCy) {
    if (!name) return;

    strncpy(g_lastBuiltinIconName, name, sizeof(g_lastBuiltinIconName) - 1);
    g_lastBuiltinIconName[sizeof(g_lastBuiltinIconName) - 1] = '\0';
    g_lastBuiltinIconValue = value;
    g_lastBuiltinIconTextColor = textColor;
    g_lastBuiltinIconBgColor = bgColor;
    g_lastBuiltinIconCx = iconCx;
    g_lastBuiltinIconCy = iconCy;

    if (value > 0 && IsTransientZeroPronePercentIcon(name)) {
        strncpy(g_lastStablePercentIconName, name, sizeof(g_lastStablePercentIconName) - 1);
        g_lastStablePercentIconName[sizeof(g_lastStablePercentIconName) - 1] = '\0';
        g_lastStablePercentIconValue = value;
    }
}

static BOOL CopyStringExactA(const char* src, char* out, size_t outSize) {
    if (!out || outSize == 0) return FALSE;
    out[0] = '\0';
    if (!src) return FALSE;

    size_t srcLen = strlen(src);
    if (srcLen >= outSize) return FALSE;

    memcpy(out, src, srcLen + 1);
    return TRUE;
}

static void InvalidateSpeedScaleCache(void) {
    InterlockedIncrement(&g_speedScaleCacheInvalidation);
}

static void ResetFramePlaybackState(void) {
    AnimationPlayback_Reset(&g_playbackState);
    g_lastAnimationTickMs = 0;
    g_playbackGeneration++;
    g_framePresentationSerial = 0;
    g_lastShellPresentationSerial = 0;
    g_lastShellFrameIndex = -1;
    g_lastShellFrameCount = 0;
    g_lastShellAnimatedFrameValid = FALSE;
    g_lastShellAnimationName[0] = '\0';
    g_frameRateCtrl.trayAccumulatorMs = 0.0;
    g_trayFrameDirty = FALSE;
}

/* Error recovery:
 * - 5000ms timeout: Reasonable duration before declaring icon update as failed
 */
static UINT g_consecutiveUpdateFailures = 0;
static DWORD g_lastSuccessfulUpdateTime = 0;
static BOOL g_updateFailureReported = FALSE;
static DWORD g_lastPreviewShellFailureLogTick = 0;
static volatile LONG g_shellUpdateBackoffActive = 0;
static volatile LONG g_lastFailedUpdateAttemptTick = 0;
#define MAX_CONSECUTIVE_FAILURES 5
#define UPDATE_TIMEOUT_MS 5000
#define UPDATE_FAILURE_BACKOFF_MS 1000

static void TrayAnimationTimerCallback(void* userData);
static void NormalizeAnimConfigValue(char* s);
static BOOL SetCurrentAnimationNameInternal(const char* name, BOOL persistConfig);
static void CancelStartupAnimationRetry(void);
static void ScheduleStartupAnimationRetry(HWND hwnd, const char* animationName);
static void CALLBACK TrayAnimationStartupRetryTimerProc(HWND hwnd, UINT msg,
                                                        UINT_PTR id, DWORD time);

static BOOL IsValidTrayAnimationWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
