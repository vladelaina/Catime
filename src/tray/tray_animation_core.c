/**
 * @file tray_animation_core.c
 * @brief Animation lifecycle coordination
 */

#include "tray/tray_animation_core.h"
#include "tray/tray_animation_decoder.h"
#include "tray/tray_animation_loader.h"
#include "tray/tray_animation_playback.h"
#include "tray/tray_animation_timer.h"
#include "tray/tray_animation_percent.h"
#include "tray/tray_animation_menu.h"
#include "tray/tray_icon_lifetime.h"
#include "utils/memory_pool.h"
#include "config.h"
#include "system_monitor.h"
#include "timer/timer.h"
#include "tray/tray.h"
#include "log.h"
#include "utils/finite_double.h"
#include "../resource/resource.h"
#include <shellapi.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

/* A single 50ms cadence matches the practical Shell tray refresh ceiling. */
#define INTERNAL_TICK_INTERVAL_MS 50
#define TRAY_UPDATE_INTERVAL_MS 50
#define ANIMATION_TICK_DISCONTINUITY_MS 1000
#define ANIMATION_STATE_DRAIN_TIMEOUT_MS 2000
#define PREVIEW_REQUEST_DEBOUNCE_MS 25
#define PREVIEW_WORKER_SHUTDOWN_WAIT_MS 2000
#define PREVIEW_WORKER_START_RETRY_COOLDOWN_MS 2000
#define SPEED_SCALE_CACHE_TTL_MS 200
#define WM_TRAY_UPDATE_ICON (WM_USER + 100)
#define MEMORY_POOL_SIZE (256 * 1024)
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define STARTUP_ANIMATION_RETRY_TIMER_ID 42429u
#define STARTUP_ANIMATION_RETRY_INTERVAL_MS 1500u
#define STARTUP_ANIMATION_RETRY_MAX_ATTEMPTS 5

/* Global state */
static char g_animationName[MAX_PATH] = "__logo__";
static char g_previewAnimationName[MAX_PATH] = "";
static BOOL g_previewAnimationFromPath = FALSE;
BOOL g_isPreviewActive = FALSE;
static HWND g_trayHwnd = NULL;
static volatile LONG g_baseFolderInterval = TRAY_ANIMATION_DEFAULT_INTERVAL_MS;
static volatile LONG g_userMinIntervalMs = 0;

/* Main animation */
static LoadedAnimation g_mainAnimation;
static int g_mainIndex = 0;

typedef struct {
    BOOL valid;
    DWORD lastRefreshTick;
    LONG invalidationSerial;
    double scalePercent;
    UINT minIntervalMs;
} SpeedScaleCache;

/* Preview animation */
static LoadedAnimation g_previewAnimation;
static int g_previewIndex = 0;

/* Async loading */
static HANDLE g_previewWorkerThread = NULL;
static HANDLE g_previewRequestEvent = NULL;
static HANDLE g_previewStopEvent = NULL;
static HANDLE g_previewCancelEvent = NULL;
static volatile LONG g_previewRequestSerial = 0;
static char g_pendingPreviewName[MAX_PATH] = "";
static BOOL g_pendingPreviewFromPath = FALSE;
static BOOL g_previewWorkerRetiring = FALSE;
static DWORD g_previewWorkerStartFailureCooldownUntil = 0;
static SRWLOCK g_previewWorkerLock = SRWLOCK_INIT;

/* Resources */
static MemoryPool* g_memoryPool = NULL;
static FrameRateController g_frameRateCtrl;
static AnimationPlaybackState g_playbackState;
static ULONGLONG g_lastAnimationTickMs = 0;
static ULONGLONG g_playbackGeneration = 0;
static ULONGLONG g_framePresentationSerial = 0;
static ULONGLONG g_lastShellPresentationSerial = 0;
static int g_lastShellFrameIndex = -1;
static int g_lastShellFrameCount = 0;
static BOOL g_lastShellAnimatedFrameValid = FALSE;
static char g_lastShellAnimationName[MAX_PATH] = {0};
static HICON g_transparentTrayIcon = NULL;
static BOOL g_mainAnimationPreloaded = FALSE;
static int g_preloadedIconCx = 0;
static int g_preloadedIconCy = 0;
static char g_lastBuiltinIconName[MAX_PATH] = "";
static int g_lastBuiltinIconValue = -1;
static COLORREF g_lastBuiltinIconTextColor = CLR_INVALID;
static COLORREF g_lastBuiltinIconBgColor = CLR_INVALID;
static int g_lastBuiltinIconCx = 0;
static int g_lastBuiltinIconCy = 0;
static char g_lastStablePercentIconName[MAX_PATH] = "";
static int g_lastStablePercentIconValue = -1;
static char g_startupRetryAnimationName[MAX_PATH] = "";
static HWND g_startupRetryHwnd = NULL;
static int g_startupRetryAttemptCount = 0;

/* Thread safety */
static CRITICAL_SECTION g_animCriticalSection;
static volatile LONG g_criticalSectionInitialized = 0;
static volatile LONG g_runtimeActive = 0;
static volatile LONG g_runtimeUsers = 0;
static BOOL g_pendingTrayUpdate = FALSE;
static BOOL g_trayFrameDirty = FALSE;
static volatile LONG g_speedScaleCacheInvalidation = 0;
static SpeedScaleCache g_speedScaleCache = {0};

#define ANIM_CS_UNINITIALIZED 0
#define ANIM_CS_INITIALIZING 1
#define ANIM_CS_INITIALIZED 2
#define ANIM_WAIT_SPIN_LIMIT 64

static void AnimationBackoffSleep(DWORD* spins) {
    Sleep((spins && (*spins)++ < ANIM_WAIT_SPIN_LIMIT) ? 0 : 1);
}

static BOOL WaitForLongToDiffer(volatile LONG* value, LONG expected, DWORD timeoutMs) {
    DWORD spins = 0;
    ULONGLONG startedAt = GetTickCount64();
    while (InterlockedCompareExchange(value, 0, 0) == expected) {
        if (GetTickCount64() - startedAt >= timeoutMs) return FALSE;
        AnimationBackoffSleep(&spins);
    }
    return TRUE;
}

static BOOL WaitForLongValue(volatile LONG* value, LONG expected, DWORD timeoutMs) {
    DWORD spins = 0;
    ULONGLONG startedAt = GetTickCount64();
    while (InterlockedCompareExchange(value, 0, 0) != expected) {
        if (GetTickCount64() - startedAt >= timeoutMs) return FALSE;
        AnimationBackoffSleep(&spins);
    }
    return TRUE;
}

static BOOL IsAnimCriticalSectionReady(void) {
    return InterlockedCompareExchange(&g_criticalSectionInitialized, 0, 0) == ANIM_CS_INITIALIZED;
}

static BOOL AnimationNeedsDecodePool(const char* name) {
    AnimationSourceType type = DetectAnimationSourceType(name);
    return type == ANIM_SOURCE_GIF || type == ANIM_SOURCE_WEBP;
}

static MemoryPool* GetTemporaryDecodePoolForAnimation(const char* name) {
    if (!AnimationNeedsDecodePool(name)) {
        return NULL;
    }

    if (!g_memoryPool) {
        g_memoryPool = MemoryPool_Create(MEMORY_POOL_SIZE);
    }
    return g_memoryPool;
}

static void ReleaseTemporaryDecodePool(void) {
    if (g_memoryPool) {
        MemoryPool_Destroy(g_memoryPool);
        g_memoryPool = NULL;
    }
}

static BOOL LoadAnimationByNameWithTemporaryPool(const char* name,
                                                 LoadedAnimation* anim,
                                                 int iconWidth,
                                                 int iconHeight) {
    MemoryPool* pool = GetTemporaryDecodePoolForAnimation(name);
    BOOL loaded = LoadAnimationByName(name, anim, pool, iconWidth, iconHeight);
    ReleaseTemporaryDecodePool();
    return loaded;
}

static BOOL IsTrayAnimationRuntimeActive(void) {
    return InterlockedCompareExchange(&g_runtimeActive, 0, 0) != 0;
}

BOOL TrayAnimation_IsRunning(void) {
    return IsTrayAnimationRuntimeActive();
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

static void SetPendingTrayUpdate(void) {
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        g_pendingTrayUpdate = TRUE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_pendingTrayUpdate = TRUE;
    }
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
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static HWND GetValidTrayAnimationWindow(void) {
    HWND hwnd = g_trayHwnd;
    return IsValidTrayAnimationWindow(hwnd) && IsTrayIconActive(hwnd) ? hwnd : NULL;
}

static BOOL BuildAnimationConfigPath(const char* name, char* animPath, size_t animPathSize) {
    if (!name || !animPath || animPathSize == 0) return FALSE;

    int pathLen = 0;
    if (IsBuiltinAnimationName(name)) {
        pathLen = snprintf(animPath, animPathSize, "%s", name);
    } else {
        if (!IsSafeAnimationRelativePath(name)) return FALSE;

        pathLen = snprintf(animPath, animPathSize,
                           "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", name);
    }

    if (pathLen < 0 || (size_t)pathLen >= animPathSize) {
        animPath[0] = '\0';
        LOG_WARNING("Animation config path too long: %s", name);
        return FALSE;
    }

    return TRUE;
}

static BOOL WriteAnimationConfigPathIfChanged(const char* configPath, const char* animPath) {
    if (!configPath || !animPath) return FALSE;

    char currentPath[MAX_PATH] = {0};
    BOOL hasCompleteCurrentPath = ReadIniStringExact(
        "Animation", "ANIMATION_PATH", "", currentPath, sizeof(currentPath), configPath);
    if (hasCompleteCurrentPath && strcmp(currentPath, animPath) == 0) {
        return TRUE;
    }
    if (!hasCompleteCurrentPath) {
        LOG_WARNING("Replacing ANIMATION_PATH because the existing config value is too long");
    }

    return WriteIniString("Animation", "ANIMATION_PATH", animPath, configPath);
}

static BOOL WriteAnimationNameToConfigIfChanged(const char* name) {
    char configPath[MAX_PATH] = {0};
    char animPath[MAX_PATH] = {0};

    GetConfigPath(configPath, sizeof(configPath));
    if (!BuildAnimationConfigPath(name, animPath, sizeof(animPath))) {
        return FALSE;
    }

    return WriteAnimationConfigPathIfChanged(configPath, animPath);
}

static void ReadAnimationNameFromConfig(char* name, size_t nameSize, const char* configPath) {
    if (!name || nameSize == 0) return;

    char nameBuf[MAX_PATH] = {0};
    if (!ReadIniStringExact("Animation", "ANIMATION_PATH", "__logo__",
                            nameBuf, sizeof(nameBuf), configPath)) {
        LOG_WARNING("Ignoring ANIMATION_PATH because the config value is too long");
        return;
    }
    NormalizeAnimConfigValue(nameBuf);

    if (nameBuf[0] == '\0') {
        return;
    }

    const char* prefix = ANIMATIONS_PATH_PREFIX;
    if (IsBuiltinAnimationName(nameBuf)) {
        CopyStringExactA(nameBuf, name, nameSize);
    } else if (_strnicmp(nameBuf, prefix, (int)strlen(prefix)) == 0) {
        const char* rel = nameBuf + strlen(prefix);
        if (IsSafeAnimationRelativePath(rel)) {
            CopyStringExactA(rel, name, nameSize);
        }
    } else if (IsSafeAnimationRelativePath(nameBuf)) {
        CopyStringExactA(nameBuf, name, nameSize);
    }
}
static DWORD WINAPI PreviewWorkerThread(LPVOID param);

static BOOL ShouldRunTrayAnimationTimer(void) {
    const LoadedAnimation* currentAnim = g_isPreviewActive ? &g_previewAnimation : &g_mainAnimation;
    return currentAnim->isAnimated && currentAnim->count > 1;
}

static void EnsureTrayAnimationTimerState(void) {
    HWND trayHwnd = GetValidTrayAnimationWindow();
    if (!trayHwnd) {
        if (IsAnimationTimerActive()) {
            if (!CleanupAnimationTimer()) {
                LOG_WARNING("Animation timer cleanup timed out; retaining runtime resources");
            }
        }
        g_trayFrameDirty = FALSE;
        ClearPendingTrayUpdate();
        return;
    }

    BOOL shouldRun = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        shouldRun = ShouldRunTrayAnimationTimer();
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        shouldRun = ShouldRunTrayAnimationTimer();
    }

    if (shouldRun) {
        if (!IsAnimationTimerActive()) {
            FrameRateController_Init(&g_frameRateCtrl, TRAY_UPDATE_INTERVAL_MS);
            if (!InitializeAnimationTimer(trayHwnd, INTERNAL_TICK_INTERVAL_MS,
                                          TrayAnimationTimerCallback, NULL)) {
                LOG_WARNING("Failed to start tray animation timer (interval=%u)",
                            (unsigned)INTERNAL_TICK_INTERVAL_MS);
                ClearPendingTrayUpdate();
            }
        }
    } else {
        g_trayFrameDirty = FALSE;
        if (IsAnimationTimerActive()) {
            if (!CleanupAnimationTimer()) {
                LOG_WARNING("Animation timer cleanup timed out; retaining runtime resources");
            }
        }
    }
}

static void SwapLoadedAnimation(LoadedAnimation* target, const LoadedAnimation* source) {
    if (!target || !source) return;
    *target = *source;
}

static void CleanupCompletedPreviewWorkerLocked(void) {
    if (!g_previewWorkerThread) {
        return;
    }

    DWORD waitResult = WaitForSingleObject(g_previewWorkerThread, 0);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_FAILED) {
            WriteLog(LOG_LEVEL_WARNING, "Preview worker status check failed (error=%lu)", GetLastError());
        }
        return;
    }

    CloseHandle(g_previewWorkerThread);
    g_previewWorkerThread = NULL;
    g_previewWorkerRetiring = FALSE;

    if (g_previewRequestEvent) {
        CloseHandle(g_previewRequestEvent);
        g_previewRequestEvent = NULL;
    }

    if (g_previewStopEvent) {
        CloseHandle(g_previewStopEvent);
        g_previewStopEvent = NULL;
    }

    if (g_previewCancelEvent) {
        CloseHandle(g_previewCancelEvent);
        g_previewCancelEvent = NULL;
    }
}

static void CleanupRetiredPreviewWorkerOnExit(void) {
    AcquireSRWLockExclusive(&g_previewWorkerLock);
    if (g_previewWorkerRetiring) {
        if (g_previewWorkerThread) {
            CloseHandle(g_previewWorkerThread);
            g_previewWorkerThread = NULL;
        }
        g_previewWorkerRetiring = FALSE;

        if (g_previewRequestEvent) {
            CloseHandle(g_previewRequestEvent);
            g_previewRequestEvent = NULL;
        }

        if (g_previewStopEvent) {
            CloseHandle(g_previewStopEvent);
            g_previewStopEvent = NULL;
        }

        if (g_previewCancelEvent) {
            CloseHandle(g_previewCancelEvent);
            g_previewCancelEvent = NULL;
        }
    }
    ReleaseSRWLockExclusive(&g_previewWorkerLock);
}

static void SignalPreviewDecodeCancelLocked(void) {
    if (g_previewCancelEvent) {
        SetEvent(g_previewCancelEvent);
    }
}

static void WakePreviewWorkerLocked(void) {
    if (g_previewRequestEvent) {
        SetEvent(g_previewRequestEvent);
    }
}

static BOOL IsPreviewWorkerStartFailureCoolingDown(DWORD now) {
    return g_previewWorkerStartFailureCooldownUntil != 0 &&
           (LONG)(g_previewWorkerStartFailureCooldownUntil - now) > 0;
}

static void MarkPreviewWorkerStartFailure(DWORD now) {
    DWORD cooldownUntil = now + PREVIEW_WORKER_START_RETRY_COOLDOWN_MS;
    g_previewWorkerStartFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
}

static BOOL EnsurePreviewWorkerStartedLocked(void) {
    CleanupCompletedPreviewWorkerLocked();

    if (g_previewWorkerThread) {
        if (g_previewWorkerRetiring) {
            return FALSE;
        }
        return TRUE;
    }

    DWORD now = GetTickCount();
    if (IsPreviewWorkerStartFailureCoolingDown(now)) {
        return FALSE;
    }

    if (!g_previewRequestEvent) {
        g_previewRequestEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!g_previewRequestEvent) {
            WriteLog(LOG_LEVEL_ERROR, "Failed to create preview request event");
            MarkPreviewWorkerStartFailure(now);
            return FALSE;
        }
    }

    if (!g_previewStopEvent) {
        g_previewStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!g_previewStopEvent) {
            CloseHandle(g_previewRequestEvent);
            g_previewRequestEvent = NULL;
            WriteLog(LOG_LEVEL_ERROR, "Failed to create preview stop event");
            MarkPreviewWorkerStartFailure(now);
            return FALSE;
        }
    }

    if (!g_previewCancelEvent) {
        g_previewCancelEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!g_previewCancelEvent) {
            CloseHandle(g_previewRequestEvent);
            CloseHandle(g_previewStopEvent);
            g_previewRequestEvent = NULL;
            g_previewStopEvent = NULL;
            WriteLog(LOG_LEVEL_ERROR, "Failed to create preview cancel event");
            MarkPreviewWorkerStartFailure(now);
            return FALSE;
        }
    }

    ResetEvent(g_previewStopEvent);
    ResetEvent(g_previewCancelEvent);
    g_previewWorkerThread = CreateThread(NULL, 0, PreviewWorkerThread, NULL, 0, NULL);
    if (!g_previewWorkerThread) {
        CloseHandle(g_previewRequestEvent);
        CloseHandle(g_previewStopEvent);
        CloseHandle(g_previewCancelEvent);
        g_previewRequestEvent = NULL;
        g_previewStopEvent = NULL;
        g_previewCancelEvent = NULL;
        WriteLog(LOG_LEVEL_ERROR, "Failed to create preview worker thread");
        MarkPreviewWorkerStartFailure(now);
        return FALSE;
    }

    g_previewWorkerRetiring = FALSE;
    g_previewWorkerStartFailureCooldownUntil = 0;
    return TRUE;
}

static BOOL IsPreviewWorkerRetiringAfterCleanup(void) {
    BOOL retiring = FALSE;

    AcquireSRWLockExclusive(&g_previewWorkerLock);
    CleanupCompletedPreviewWorkerLocked();
    retiring = g_previewWorkerRetiring;
    ReleaseSRWLockExclusive(&g_previewWorkerLock);

    return retiring;
}

static BOOL WaitForPreviewRequestQuiet(HANDLE stopEvent, HANDLE requestEvent) {
    HANDLE waitHandles[2] = { stopEvent, requestEvent };

    for (;;) {
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, PREVIEW_REQUEST_DEBOUNCE_MS);
        if (waitResult == WAIT_OBJECT_0) {
            return FALSE;
        }
        if (waitResult == WAIT_OBJECT_0 + 1) {
            continue;
        }
        if (waitResult == WAIT_TIMEOUT) {
            return TRUE;
        }

        WriteLog(LOG_LEVEL_WARNING, "PreviewWorkerThread: debounce wait failed (error=%lu)", GetLastError());
        return TRUE;
    }
}

static BOOL ShutdownPreviewWorker(void) {
    HANDLE workerThread = NULL;
    BOOL stopped = TRUE;

    AcquireSRWLockExclusive(&g_previewWorkerLock);

    CleanupCompletedPreviewWorkerLocked();
    if (g_previewWorkerRetiring) {
        ReleaseSRWLockExclusive(&g_previewWorkerLock);
        return FALSE;
    }

    if (g_previewStopEvent) {
        SetEvent(g_previewStopEvent);
    }

    SignalPreviewDecodeCancelLocked();
    WakePreviewWorkerLocked();

    workerThread = g_previewWorkerThread;
    ReleaseSRWLockExclusive(&g_previewWorkerLock);

    if (workerThread) {
        DWORD waitResult = WaitForSingleObject(workerThread, PREVIEW_WORKER_SHUTDOWN_WAIT_MS);
        if (waitResult != WAIT_OBJECT_0) {
            WriteLog(LOG_LEVEL_WARNING,
                     "Preview worker did not stop within %lu ms (wait=%lu, error=%lu)",
                     (DWORD)PREVIEW_WORKER_SHUTDOWN_WAIT_MS, waitResult, GetLastError());
            stopped = FALSE;
        }
    }

    if (!stopped) {
        AcquireSRWLockExclusive(&g_previewWorkerLock);
        if (g_previewWorkerThread == workerThread) {
            g_previewWorkerRetiring = TRUE;
            CleanupCompletedPreviewWorkerLocked();
        }
        stopped = (g_previewWorkerThread != workerThread);
        ReleaseSRWLockExclusive(&g_previewWorkerLock);
        return stopped;
    }

    AcquireSRWLockExclusive(&g_previewWorkerLock);

    if (g_previewWorkerThread) {
        CloseHandle(g_previewWorkerThread);
        g_previewWorkerThread = NULL;
    }
    g_previewWorkerRetiring = FALSE;

    if (g_previewRequestEvent) {
        CloseHandle(g_previewRequestEvent);
        g_previewRequestEvent = NULL;
    }

    if (g_previewStopEvent) {
        CloseHandle(g_previewStopEvent);
        g_previewStopEvent = NULL;
    }

    if (g_previewCancelEvent) {
        CloseHandle(g_previewCancelEvent);
        g_previewCancelEvent = NULL;
    }

    ReleaseSRWLockExclusive(&g_previewWorkerLock);
    return TRUE;
}

static void PostPreviewLoadedMessage(void) {
    if (!IsTrayAnimationRuntimeActive()) return;
    HWND trayHwnd = GetValidTrayAnimationWindow();
    if (!trayHwnd) return;

    SetPendingTrayUpdate();

    if (!PostMessage(trayHwnd, CLOCK_WM_ANIMATION_PREVIEW_LOADED, 0, 0)) {
        ClearPendingTrayUpdate();
    }
}

/**
 * @brief Normalize config value (trim whitespace and quotes)
 */
static void NormalizeAnimConfigValue(char* s) {
    if (!s) return;
    char* p = s;
    while (*p && (isspace((unsigned char)*p) || *p == '"' || *p == '\'')) p++;
    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }
    size_t len = strlen(s);
    while (len > 0 && (isspace((unsigned char)s[len - 1]) || s[len - 1] == '"' || s[len - 1] == '\'')) {
        s[--len] = '\0';
    }
}

/**
 * @brief Record successful update
 */
static void RecordSuccessfulUpdate(void) {
    g_consecutiveUpdateFailures = 0;
    g_lastSuccessfulUpdateTime = GetTickCount();
    g_updateFailureReported = FALSE;
    g_lastPreviewShellFailureLogTick = 0;
    InterlockedExchange(&g_shellUpdateBackoffActive, 0);
    InterlockedExchange(&g_lastFailedUpdateAttemptTick, 0);
}

/**
 * @brief Record failed update
 * @return TRUE if repeated failures should trigger recovery handling
 */
static BOOL RecordFailedUpdate(void) {
    DWORD currentTime = GetTickCount();
    InterlockedExchange(&g_lastFailedUpdateAttemptTick, (LONG)currentTime);
    if (g_consecutiveUpdateFailures < UINT_MAX) {
        g_consecutiveUpdateFailures++;
    }
    if (g_updateFailureReported) {
        return FALSE;
    }
    
    if (g_consecutiveUpdateFailures >= MAX_CONSECUTIVE_FAILURES) {
        g_updateFailureReported = TRUE;
        InterlockedExchange(&g_shellUpdateBackoffActive, 1);
        return TRUE;
    }
    
    if (g_lastSuccessfulUpdateTime > 0) {
        DWORD elapsed = currentTime - g_lastSuccessfulUpdateTime;
        if (elapsed > UPDATE_TIMEOUT_MS) {
            g_updateFailureReported = TRUE;
            InterlockedExchange(&g_shellUpdateBackoffActive, 1);
            return TRUE;
        }
    }
    
    return FALSE;
}

/**
 * @brief Keep the selected animation alive across transient Shell failures
 */
static void HandleRepeatedTrayUpdateFailure(void) {
    WriteLog(LOG_LEVEL_WARNING,
             "Tray icon update is temporarily unavailable; retaining the selected animation and retrying");
}

static void LogPreviewShellFailureThrottled(void) {
    DWORD now = GetTickCount();
    if (g_lastPreviewShellFailureLogTick == 0 ||
        (DWORD)(now - g_lastPreviewShellFailureLogTick) >= UPDATE_TIMEOUT_MS) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Tray preview icon update is temporarily unavailable");
        g_lastPreviewShellFailureLogTick = now ? now : 1u;
    }
}

/**
 * @brief Compute scaled delay based on CPU/memory/timer
 */
static double ComputeAnimationSpeedScalePercent(AnimationSpeedMetric metric) {
    if (metric == ANIMATION_SPEED_ORIGINAL) {
        return 100.0;
    }
    if (metric == ANIMATION_SPEED_FIXED) {
        return GetAnimationFixedSpeedMultiplier() * 100.0;
    }

    double percent = 0.0;

    if (metric == ANIMATION_SPEED_CPU) {
        float cpu = 0.0f;
        SystemMonitor_GetCpuUsage(&cpu);
        percent = cpu;
    } else if (metric == ANIMATION_SPEED_TIMER) {
        if (!CLOCK_SHOW_CURRENT_TIME && !CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0) {
            double p = (double)countdown_elapsed_time / (double)CLOCK_TOTAL_TIME;
            if (p < 0.0) p = 0.0;
            if (p > 1.0) p = 1.0;
            percent = p * 100.0;
        }
    } else {
        float mem = 0.0f;
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

    if (IsTrayTooltipActive() || IsTrayInteractionSuspended()) {
        ClearPendingTrayUpdate();
        MarkTrayFrameDirty();
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
                RefreshTrayBackgroundWorkState();
            }
            if (RecordFailedUpdate()) {
                HandleRepeatedTrayUpdateFailure();
            }
        }
    }
}

static void UpdateTrayIconToCurrentFrame(void) {
    UpdateTrayIconToCurrentFrameInternal();
}

static void UpdateTrayIconToCurrentFrameForPreview(void) {
    if (IsTrayTooltipActive() || IsTrayInteractionSuspended()) {
        MarkTrayFrameDirty();
        return;
    }
    UpdateTrayIconToCurrentFrameInternal();
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
        IsTrayTooltipActive() || IsTrayInteractionSuspended();
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
    
    ResetBuiltinIconUpdateCache();
    EnsureTrayAnimationTimerState();
    UpdateTrayIconToCurrentFrame();

    result = TRUE;

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

static void CancelStartupAnimationRetry(void) {
    if (g_startupRetryHwnd) {
        KillTimer(g_startupRetryHwnd, STARTUP_ANIMATION_RETRY_TIMER_ID);
    }
    g_startupRetryHwnd = NULL;
    g_startupRetryAnimationName[0] = '\0';
    g_startupRetryAttemptCount = 0;
}

static void ScheduleStartupAnimationRetry(HWND hwnd, const char* animationName) {
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

static void CALLBACK TrayAnimationStartupRetryTimerProc(HWND hwnd, UINT msg,
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

static BOOL QueueAnimationPreviewRequest(const char* name, BOOL fromPath) {
    if (!name || !*name) return FALSE;
    if (!BeginTrayAnimationRuntimeUse()) return FALSE;

    BOOL queued = FALSE;
    char requestedName[MAX_PATH] = {0};
    if (!CopyStringExactA(name, requestedName, sizeof(requestedName))) {
        LOG_WARNING("Ignoring animation preview request because the name is too long: %s", name);
        goto done;
    }

    AcquireSRWLockExclusive(&g_previewWorkerLock);

    CleanupCompletedPreviewWorkerLocked();

    BOOL duplicateRequest = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    duplicateRequest =
        (g_isPreviewActive &&
         g_previewAnimationFromPath == fromPath &&
         g_previewAnimationName[0] != '\0' &&
         _stricmp(g_previewAnimationName, requestedName) == 0) ||
        (g_previewWorkerThread &&
         g_pendingPreviewFromPath == fromPath &&
         g_pendingPreviewName[0] != '\0' &&
         _stricmp(g_pendingPreviewName, requestedName) == 0);
    if (IsAnimCriticalSectionReady()) {
        LeaveCriticalSection(&g_animCriticalSection);
    }
    if (duplicateRequest) {
        queued = TRUE;
        ReleaseSRWLockExclusive(&g_previewWorkerLock);
        goto done;
    }

    if (!EnsurePreviewWorkerStartedLocked()) {
        ReleaseSRWLockExclusive(&g_previewWorkerLock);
        goto done;
    }

    SignalPreviewDecodeCancelLocked();

    InterlockedIncrement(&g_previewRequestSerial);

    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    CopyStringExactA(requestedName, g_pendingPreviewName, sizeof(g_pendingPreviewName));
    g_pendingPreviewFromPath = fromPath;
    if (IsAnimCriticalSectionReady()) {
        LeaveCriticalSection(&g_animCriticalSection);
    }

    if (g_previewRequestEvent) {
        WakePreviewWorkerLocked();
        queued = TRUE;
    }
    ReleaseSRWLockExclusive(&g_previewWorkerLock);

done:
    EndTrayAnimationRuntimeUse();
    return queued;
}

/**
 * @brief Queue animation preview from file path
 */
BOOL PreviewAnimationFromFile(HWND hwnd, const char* filePath) {
    (void)hwnd;
    return QueueAnimationPreviewRequest(filePath, TRUE);
}

/**
 * @brief Preview worker thread
 */
static DWORD WINAPI PreviewWorkerThread(LPVOID param) {
    (void)param;

    HANDLE waitHandles[2] = { g_previewStopEvent, g_previewRequestEvent };
    for (;;) {
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }
        if (waitResult != WAIT_OBJECT_0 + 1) {
            WriteLog(LOG_LEVEL_WARNING, "PreviewWorkerThread: request wait failed (result=%lu, error=%lu)",
                     waitResult, GetLastError());
            break;
        }

        if (!WaitForPreviewRequestQuiet(g_previewStopEvent, g_previewRequestEvent)) {
            break;
        }

        char requestedName[MAX_PATH] = {0};
        BOOL requestedFromPath = FALSE;
        LONG requestSerial = 0;

        AcquireSRWLockExclusive(&g_previewWorkerLock);

        requestSerial = InterlockedCompareExchange(&g_previewRequestSerial, 0, 0);
        if (IsAnimCriticalSectionReady()) {
            EnterCriticalSection(&g_animCriticalSection);
            CopyStringExactA(g_pendingPreviewName, requestedName, sizeof(requestedName));
            requestedFromPath = g_pendingPreviewFromPath;
            LeaveCriticalSection(&g_animCriticalSection);
        } else {
            CopyStringExactA(g_pendingPreviewName, requestedName, sizeof(requestedName));
            requestedFromPath = g_pendingPreviewFromPath;
        }

        if (!requestedName[0]) {
            ReleaseSRWLockExclusive(&g_previewWorkerLock);
            continue;
        }

        if (g_previewCancelEvent) {
            ResetEvent(g_previewCancelEvent);
        }

        ReleaseSRWLockExclusive(&g_previewWorkerLock);

        LoadedAnimation tempAnim;
        LoadedAnimation oldPreview;
        LoadedAnimation_Init(&tempAnim);
        LoadedAnimation_Init(&oldPreview);

        int cx = GetSystemMetrics(SM_CXSMICON);
        int cy = GetSystemMetrics(SM_CYSMICON);
        MemoryPool* localPool = AnimationNeedsDecodePool(requestedName)
            ? MemoryPool_Create(MEMORY_POOL_SIZE)
            : NULL;
        if (requestedFromPath) {
            LoadAnimationFromPathWithCancel(requestedName, &tempAnim, localPool, cx, cy,
                                            g_previewCancelEvent);
        } else {
            LoadAnimationByNameWithCancel(requestedName, &tempAnim, localPool, cx, cy,
                                          g_previewCancelEvent);
        }
        if (localPool) {
            MemoryPool_Destroy(localPool);
        }

        if (g_previewStopEvent && WaitForSingleObject(g_previewStopEvent, 0) == WAIT_OBJECT_0) {
            LoadedAnimation_Free(&tempAnim);
            break;
        }

        if (g_previewCancelEvent && WaitForSingleObject(g_previewCancelEvent, 0) == WAIT_OBJECT_0) {
            LoadedAnimation_Free(&tempAnim);
            continue;
        }

        if (!BeginTrayAnimationRuntimeUse()) {
            LoadedAnimation_Free(&oldPreview);
            LoadedAnimation_Free(&tempAnim);
            continue;
        }

        BOOL shouldApply = FALSE;
        if (IsAnimCriticalSectionReady()) {
            EnterCriticalSection(&g_animCriticalSection);
        }

        if (InterlockedCompareExchange(&g_previewRequestSerial, 0, 0) == requestSerial &&
            g_pendingPreviewFromPath == requestedFromPath &&
            g_pendingPreviewName[0] != '\0' &&
            _stricmp(g_pendingPreviewName, requestedName) == 0) {

            BOOL canActivate = (tempAnim.count > 0 || tempAnim.sourceType == ANIM_SOURCE_PERCENT ||
                                tempAnim.sourceType == ANIM_SOURCE_CAPSLOCK ||
                                _stricmp(requestedName, "__none__") == 0);

            if (canActivate) {
                SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
                SwapLoadedAnimation(&g_previewAnimation, &tempAnim);
                LoadedAnimation_Init(&tempAnim);
                g_previewIndex = 0;
                ResetFramePlaybackState();
                g_isPreviewActive = TRUE;
                g_previewAnimationFromPath = requestedFromPath;
                CopyStringExactA(requestedName, g_previewAnimationName,
                                 sizeof(g_previewAnimationName));
                shouldApply = TRUE;
            } else {
                SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
                LoadedAnimation_Init(&g_previewAnimation);
                g_isPreviewActive = FALSE;
                g_previewAnimationFromPath = FALSE;
                g_previewAnimationName[0] = '\0';
                g_pendingPreviewFromPath = FALSE;
                g_pendingPreviewName[0] = '\0';
                shouldApply = TRUE;
                WriteLog(LOG_LEVEL_WARNING, "PreviewWorkerThread: preview failed to load '%s'", requestedName);
            }
        }

        if (shouldApply) {
            g_pendingPreviewFromPath = FALSE;
            g_pendingPreviewName[0] = '\0';
        }

        if (IsAnimCriticalSectionReady()) {
            LeaveCriticalSection(&g_animCriticalSection);
        }

        LoadedAnimation_Free(&oldPreview);
        LoadedAnimation_Free(&tempAnim);

        if (shouldApply) {
            PostPreviewLoadedMessage();
        }

        EndTrayAnimationRuntimeUse();
    }

    CleanupRetiredPreviewWorkerOnExit();
    return 0;
}

/**
 * @brief Start animation preview
 */
void StartAnimationPreview(const char* name) {
    (void)QueueAnimationPreviewRequest(name, FALSE);
}

/**
 * @brief Cancel animation preview
 */
void CancelAnimationPreview(void) {
    LoadedAnimation oldPreview;
    LoadedAnimation_Init(&oldPreview);

    if (!BeginTrayAnimationRuntimeUse()) return;

    AcquireSRWLockExclusive(&g_previewWorkerLock);

    InterlockedIncrement(&g_previewRequestSerial);

    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
    }

    BOOL hasPreviewState = g_isPreviewActive || g_pendingPreviewName[0] != '\0';
    if (!hasPreviewState) {
        if (IsAnimCriticalSectionReady()) {
            LeaveCriticalSection(&g_animCriticalSection);
        }
        ReleaseSRWLockExclusive(&g_previewWorkerLock);
        goto done;
    }

    g_isPreviewActive = FALSE;
    g_previewAnimationFromPath = FALSE;
    g_previewAnimationName[0] = '\0';
    g_pendingPreviewFromPath = FALSE;
    g_pendingPreviewName[0] = '\0';
    SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
    LoadedAnimation_Init(&g_previewAnimation);
    ResetFramePlaybackState();

    if (IsAnimCriticalSectionReady()) {
        LeaveCriticalSection(&g_animCriticalSection);
    }

    SignalPreviewDecodeCancelLocked();
    WakePreviewWorkerLocked();
    ReleaseSRWLockExclusive(&g_previewWorkerLock);

    EnsureTrayAnimationTimerState();
    UpdateTrayIconToCurrentFrameForPreview();

done:
    LoadedAnimation_Free(&oldPreview);
    EndTrayAnimationRuntimeUse();
}

/**
 * @brief Preload animation from config
 */
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
static void UpdatePercentIconIfNeededInternal(BOOL hasMetricsSnapshot,
                                              float cpuPercent,
                                              float memPercent) {
    if (!BeginTrayAnimationRuntimeUse()) return;

    HWND trayHwnd = GetValidTrayAnimationWindow();
    if (!trayHwnd) goto done;
    /* Replacing the icon while Explorer owns a hover tooltip can make the
     * notification area briefly tear down and recreate that tooltip.  Keep
     * dynamic built-in icons stable for the whole hover/menu interaction;
     * the background timer refreshes the latest value after it ends. */
    if (IsTrayTooltipActive() || IsTrayInteractionSuspended()) goto done;

    char animationName[MAX_PATH] = {0};
    BOOL previewActive = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        CopyStringExactA(g_animationName, animationName, sizeof(animationName));
        previewActive = g_isPreviewActive;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        CopyStringExactA(g_animationName, animationName, sizeof(animationName));
        previewActive = g_isPreviewActive;
    }

    if (!animationName[0]) goto done;
    if (previewActive) goto done;
    
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
    
    if (hasPending &&
        (IsTrayTooltipActive() || IsTrayInteractionSuspended())) {
        ClearPendingTrayUpdate();
        MarkTrayFrameDirty();
    } else if (hasPending) {
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
