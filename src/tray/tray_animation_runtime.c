/**
 * @file tray_animation_runtime.c
 * @brief Runtime synchronization, decode-pool, and pending-update helpers.
 */

#include "tray_animation_core_internal.h"

void AnimationBackoffSleep(DWORD* spins) {
    Sleep((spins && (*spins)++ < ANIM_WAIT_SPIN_LIMIT) ? 0 : 1);
}

BOOL WaitForLongToDiffer(volatile LONG* value, LONG expected, DWORD timeoutMs) {
    DWORD spins = 0;
    ULONGLONG startedAt = GetTickCount64();
    while (InterlockedCompareExchange(value, 0, 0) == expected) {
        if (GetTickCount64() - startedAt >= timeoutMs) return FALSE;
        AnimationBackoffSleep(&spins);
    }
    return TRUE;
}

BOOL WaitForLongValue(volatile LONG* value, LONG expected, DWORD timeoutMs) {
    DWORD spins = 0;
    ULONGLONG startedAt = GetTickCount64();
    while (InterlockedCompareExchange(value, 0, 0) != expected) {
        if (GetTickCount64() - startedAt >= timeoutMs) return FALSE;
        AnimationBackoffSleep(&spins);
    }
    return TRUE;
}

BOOL IsAnimCriticalSectionReady(void) {
    return InterlockedCompareExchange(&g_criticalSectionInitialized, 0, 0) == ANIM_CS_INITIALIZED;
}

BOOL AnimationNeedsDecodePool(const char* name) {
    AnimationSourceType type = DetectAnimationSourceType(name);
    return type == ANIM_SOURCE_GIF || type == ANIM_SOURCE_WEBP;
}

MemoryPool* GetTemporaryDecodePoolForAnimation(const char* name) {
    if (!AnimationNeedsDecodePool(name)) {
        return NULL;
    }

    if (!g_memoryPool) {
        g_memoryPool = MemoryPool_Create(MEMORY_POOL_SIZE);
    }
    return g_memoryPool;
}

void ReleaseTemporaryDecodePool(void) {
    if (g_memoryPool) {
        MemoryPool_Destroy(g_memoryPool);
        g_memoryPool = NULL;
    }
}

BOOL LoadAnimationByNameWithTemporaryPool(const char* name,
                                                 LoadedAnimation* anim,
                                                 int iconWidth,
                                                 int iconHeight) {
    MemoryPool* pool = GetTemporaryDecodePoolForAnimation(name);
    BOOL loaded = LoadAnimationByName(name, anim, pool, iconWidth, iconHeight);
    ReleaseTemporaryDecodePool();
    return loaded;
}

BOOL IsTrayAnimationRuntimeActive(void) {
    return InterlockedCompareExchange(&g_runtimeActive, 0, 0) != 0;
}

BOOL TrayAnimation_IsRunning(void) {
    return IsTrayAnimationRuntimeActive();
}

UINT GetBaseFolderIntervalMs(void) {
    LONG interval = InterlockedCompareExchange(&g_baseFolderInterval, 0, 0);
    return interval > 0 ? (UINT)interval : TRAY_ANIMATION_DEFAULT_INTERVAL_MS;
}

UINT GetUserMinIntervalMs(void) {
    LONG interval = InterlockedCompareExchange(&g_userMinIntervalMs, 0, 0);
    return interval > 0 ? (UINT)interval : 0;
}

UINT ClampAnimationIntervalMs(UINT ms) {
    if (ms == 0) return TRAY_ANIMATION_DEFAULT_INTERVAL_MS;
    if (ms < TRAY_ANIMATION_MIN_INTERVAL_MS) return TRAY_ANIMATION_MIN_INTERVAL_MS;
    if (ms > TRAY_ANIMATION_MAX_INTERVAL_MS) return TRAY_ANIMATION_MAX_INTERVAL_MS;
    return ms;
}

UINT ClampAnimationMinIntervalMs(UINT ms) {
    if (ms == 0) return 0;
    return ClampAnimationIntervalMs(ms);
}

BOOL BeginTrayAnimationRuntimeUse(void) {
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

void EndTrayAnimationRuntimeUse(void) {
    InterlockedDecrement(&g_runtimeUsers);
}

BOOL QuiesceTrayAnimationRuntime(void) {
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

HICON GetTransparentTrayIcon(void) {
    if (!g_transparentTrayIcon) {
        BYTE andMask[32];
        BYTE xorMask[32];
        memset(andMask, 0xFF, sizeof(andMask));
        memset(xorMask, 0x00, sizeof(xorMask));
        g_transparentTrayIcon = CreateIcon(NULL, 16, 16, 1, 1, andMask, xorMask);
    }

    return g_transparentTrayIcon;
}

void CleanupTransparentTrayIcon(void) {
    if (g_transparentTrayIcon) {
        DestroyIcon(g_transparentTrayIcon);
        g_transparentTrayIcon = NULL;
    }
}

void ClearPendingTrayUpdate(void) {
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
BOOL ClaimPendingTrayUpdate(void) {
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

BOOL HasPendingTrayUpdate(void) {
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

void MarkTrayFrameDirty(void) {
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        g_trayFrameDirty = TRUE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_trayFrameDirty = TRUE;
    }
}

void ClearTrayFrameDirty(void) {
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        g_trayFrameDirty = FALSE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_trayFrameDirty = FALSE;
    }
}
