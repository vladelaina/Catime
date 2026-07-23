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
#include "tray_animation_core_part01.inc"
#include "tray_animation_core_part02.inc"
#include "tray_animation_core_part03.inc"
#include "tray_animation_core_part04.inc"
#include "tray_animation_core_part05.inc"
#include "tray_animation_core_part06.inc"
#include "tray_animation_core_part07.inc"
#include "tray_animation_core_part08.inc"
#include "tray_animation_core_part09.inc"
