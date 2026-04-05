/**
 * @file tray_animation_core.c
 * @brief Animation lifecycle coordination
 */

#include "tray/tray_animation_core.h"
#include "tray/tray_animation_decoder.h"
#include "tray/tray_animation_loader.h"
#include "tray/tray_animation_timer.h"
#include "tray/tray_animation_percent.h"
#include "tray/tray_animation_menu.h"
#include "utils/memory_pool.h"
#include "config.h"
#include "system_monitor.h"
#include "timer/timer.h"
#include "tray/tray.h"
#include "log.h"
#include "../resource/resource.h"
#include <shellapi.h>
#include <string.h>
#include <ctype.h>

/* Constants:
 * - 10ms internal tick: High-frequency updates for smooth animations
 * - 50ms tray update: 20 FPS balances smoothness with system tray refresh limitations
 */
#define INTERNAL_TICK_INTERVAL_MS 10
#define TRAY_UPDATE_INTERVAL_MS 50
#define WM_TRAY_UPDATE_ICON (WM_USER + 100)
#define MEMORY_POOL_SIZE (256 * 1024)

/* Global state */
static char g_animationName[MAX_PATH] = "__logo__";
static char g_previewAnimationName[MAX_PATH] = "";
BOOL g_isPreviewActive = FALSE;
static HWND g_trayHwnd = NULL;
static UINT g_baseFolderInterval = 150;
static UINT g_userMinIntervalMs = 0;

/* Main animation */
static LoadedAnimation g_mainAnimation;
static int g_mainIndex = 0;

/* Preview animation */
static LoadedAnimation g_previewAnimation;
static int g_previewIndex = 0;

/* Async loading */
static HANDLE g_previewWorkerThread = NULL;
static HANDLE g_previewRequestEvent = NULL;
static HANDLE g_previewStopEvent = NULL;
static volatile LONG g_previewRequestSerial = 0;
static char g_pendingPreviewName[MAX_PATH] = "";

/* Resources */
static MemoryPool* g_memoryPool = NULL;
static FrameRateController g_frameRateCtrl;

/* Thread safety */
static CRITICAL_SECTION g_animCriticalSection;
static volatile LONG g_criticalSectionInitialized = 0;
static BOOL g_pendingTrayUpdate = FALSE;

/* Error recovery:
 * - 5000ms timeout: Reasonable duration before declaring icon update as failed
 */
static UINT g_consecutiveUpdateFailures = 0;
static DWORD g_lastSuccessfulUpdateTime = 0;
#define MAX_CONSECUTIVE_FAILURES 5
#define UPDATE_TIMEOUT_MS 5000

static void TrayAnimationTimerCallback(void* userData);
static DWORD WINAPI PreviewWorkerThread(LPVOID param);

extern BOOL LoadAnimationFromPath(const char* path, LoadedAnimation* anim, void* pool, int cx, int cy);

static BOOL ShouldRunTrayAnimationTimer(void) {
    const LoadedAnimation* currentAnim = g_isPreviewActive ? &g_previewAnimation : &g_mainAnimation;
    return currentAnim->isAnimated && currentAnim->count > 1;
}

static void EnsureTrayAnimationTimerState(void) {
    if (!g_trayHwnd || !IsWindow(g_trayHwnd)) return;

    BOOL shouldRun = FALSE;
    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
        shouldRun = ShouldRunTrayAnimationTimer();
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        shouldRun = ShouldRunTrayAnimationTimer();
    }

    if (shouldRun) {
        if (!IsAnimationTimerActive()) {
            FrameRateController_Init(&g_frameRateCtrl, TRAY_UPDATE_INTERVAL_MS);
            InitializeAnimationTimer(g_trayHwnd, INTERNAL_TICK_INTERVAL_MS, TrayAnimationTimerCallback, NULL);
        }
    } else if (IsAnimationTimerActive()) {
        CleanupAnimationTimer();
    }
}

static void SwapLoadedAnimation(LoadedAnimation* target, const LoadedAnimation* source) {
    if (!target || !source) return;
    *target = *source;
}

static BOOL EnsurePreviewWorkerStarted(void) {
    if (g_previewWorkerThread) {
        return TRUE;
    }

    if (!g_previewRequestEvent) {
        g_previewRequestEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!g_previewRequestEvent) {
            WriteLog(LOG_LEVEL_ERROR, "Failed to create preview request event");
            return FALSE;
        }
    }

    if (!g_previewStopEvent) {
        g_previewStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!g_previewStopEvent) {
            CloseHandle(g_previewRequestEvent);
            g_previewRequestEvent = NULL;
            WriteLog(LOG_LEVEL_ERROR, "Failed to create preview stop event");
            return FALSE;
        }
    }

    ResetEvent(g_previewStopEvent);
    g_previewWorkerThread = CreateThread(NULL, 0, PreviewWorkerThread, NULL, 0, NULL);
    if (!g_previewWorkerThread) {
        CloseHandle(g_previewRequestEvent);
        CloseHandle(g_previewStopEvent);
        g_previewRequestEvent = NULL;
        g_previewStopEvent = NULL;
        WriteLog(LOG_LEVEL_ERROR, "Failed to create preview worker thread");
        return FALSE;
    }

    return TRUE;
}

static void ShutdownPreviewWorker(void) {
    if (g_previewStopEvent) {
        SetEvent(g_previewStopEvent);
    }

    if (g_previewRequestEvent) {
        SetEvent(g_previewRequestEvent);
    }

    if (g_previewWorkerThread) {
        WaitForSingleObject(g_previewWorkerThread, INFINITE);
        CloseHandle(g_previewWorkerThread);
        g_previewWorkerThread = NULL;
    }

    if (g_previewRequestEvent) {
        CloseHandle(g_previewRequestEvent);
        g_previewRequestEvent = NULL;
    }

    if (g_previewStopEvent) {
        CloseHandle(g_previewStopEvent);
        g_previewStopEvent = NULL;
    }
}

static void PostPreviewLoadedMessage(void) {
    if (!g_trayHwnd || !IsWindow(g_trayHwnd)) return;

    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
        g_pendingTrayUpdate = TRUE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_pendingTrayUpdate = TRUE;
    }

    PostMessage(g_trayHwnd, CLOCK_WM_ANIMATION_PREVIEW_LOADED, 0, 0);
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
}

/**
 * @brief Record failed update
 * @return TRUE if should fallback
 */
static BOOL RecordFailedUpdate(void) {
    g_consecutiveUpdateFailures++;
    
    if (g_consecutiveUpdateFailures >= MAX_CONSECUTIVE_FAILURES) {
        WriteLog(LOG_LEVEL_WARNING, "Animation update failed %d times, entering fallback mode",
                 g_consecutiveUpdateFailures);
        return TRUE;
    }
    
    DWORD currentTime = GetTickCount();
    if (g_lastSuccessfulUpdateTime > 0) {
        DWORD elapsed = currentTime - g_lastSuccessfulUpdateTime;
        if (elapsed > UPDATE_TIMEOUT_MS) {
            WriteLog(LOG_LEVEL_WARNING, "No successful update for %dms, entering fallback mode", elapsed);
            return TRUE;
        }
    }
    
    return FALSE;
}

/**
 * @brief Fallback to logo icon
 */
static void FallbackToLogoIcon(void) {
    WriteLog(LOG_LEVEL_INFO, "Falling back to logo icon due to errors");
    g_consecutiveUpdateFailures = 0;
    g_lastSuccessfulUpdateTime = GetTickCount();
    SetCurrentAnimationName("__logo__");
}

/**
 * @brief Compute scaled delay based on CPU/memory/timer
 */
static UINT ComputeScaledDelay(UINT baseDelay) {
    if (baseDelay == 0) baseDelay = g_baseFolderInterval;

    AnimationSpeedMetric metric = GetAnimationSpeedMetric();
    
    /* Original speed mode: return baseDelay unchanged */
    if (metric == ANIMATION_SPEED_ORIGINAL) {
        return baseDelay;
    }

    double percent = 0.0;
    
    if (metric == ANIMATION_SPEED_CPU) {
        float cpu = 0.0f, mem = 0.0f;
        SystemMonitor_GetUsage(&cpu, &mem);
        percent = cpu;
    } else if (metric == ANIMATION_SPEED_TIMER) {
        if (!CLOCK_SHOW_CURRENT_TIME && !CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0) {
            double p = (double)countdown_elapsed_time / (double)CLOCK_TOTAL_TIME;
            if (p < 0.0) p = 0.0;
            if (p > 1.0) p = 1.0;
            percent = p * 100.0;
        }
    } else {
        float cpu = 0.0f, mem = 0.0f;
        SystemMonitor_GetUsage(&cpu, &mem);
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
    if (scalePercent <= 0.0) scalePercent = 100.0;
    
    double scale = scalePercent / 100.0;
    if (scale < 0.1) scale = 0.1;
    
    UINT scaledDelay = (UINT)(baseDelay / scale);
    if (g_userMinIntervalMs > 0 && scaledDelay < g_userMinIntervalMs) {
        scaledDelay = g_userMinIntervalMs;
    }
    
    return scaledDelay;
}

/**
 * @brief Update tray icon to current frame (UI thread only)
 */
static void UpdateTrayIconToCurrentFrame(void) {
    if (!g_trayHwnd || !IsWindow(g_trayHwnd)) return;
    if (IsTrayInteractionSuspended()) return;
    
    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
        g_pendingTrayUpdate = FALSE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_pendingTrayUpdate = FALSE;
    }
    
    LoadedAnimation* currentAnim = g_isPreviewActive ? &g_previewAnimation : &g_mainAnimation;
    int* currentIndex = g_isPreviewActive ? &g_previewIndex : &g_mainIndex;
    const char* targetName = g_isPreviewActive ? g_previewAnimationName : g_animationName;
    
    /* Handle transparent/none icon */
    if (_stricmp(targetName, "__none__") == 0) {
        /* Create a fully transparent 16x16 icon */
        BYTE andMask[32];  /* 16x16 / 8 = 32 bytes, all 1s = transparent */
        BYTE xorMask[32];  /* All 0s = black, but masked out */
        memset(andMask, 0xFF, sizeof(andMask));  /* All transparent */
        memset(xorMask, 0x00, sizeof(xorMask));
        
        HICON hIcon = CreateIcon(NULL, 16, 16, 1, 1, andMask, xorMask);
        if (hIcon) {
            NOTIFYICONDATAW nid = {0};
            nid.cbSize = sizeof(nid);
            nid.hWnd = g_trayHwnd;
            nid.uID = CLOCK_ID_TRAY_APP_ICON;
            nid.uFlags = NIF_ICON;
            nid.hIcon = hIcon;
            Shell_NotifyIconW(NIM_MODIFY, &nid);
            DestroyIcon(hIcon);
            RecordSuccessfulUpdate();
        }
        return;
    }
    
    /* Handle percent icons - both normal and preview mode */
    if (currentAnim->sourceType == ANIM_SOURCE_PERCENT) {
        int p = 0;
        
        const BuiltinAnimDef* def = GetBuiltinAnimDef(targetName);
        if (def && def->getValue) {
            p = def->getValue();
        }
        
        if (p < 0) p = 0;
        if (p > 100) p = 100;

        HICON hIcon = CreatePercentIcon16(p);
        if (hIcon) {
            NOTIFYICONDATAW nid = {0};
            nid.cbSize = sizeof(nid);
            nid.hWnd = g_trayHwnd;
            nid.uID = CLOCK_ID_TRAY_APP_ICON;
            nid.uFlags = NIF_ICON;
            nid.hIcon = hIcon;
            Shell_NotifyIconW(NIM_MODIFY, &nid);
            DestroyIcon(hIcon);
            RecordSuccessfulUpdate();
        } else {
            WriteLog(LOG_LEVEL_ERROR, "Failed to create percent icon for %d%%", p);
        }
        return;
    }
    
    /* Handle Caps Lock indicator */
    if (currentAnim->sourceType == ANIM_SOURCE_CAPSLOCK) {
        BOOL capsOn = IsCapsLockOn();
        
        HICON hIcon = CreateCapsLockIcon(capsOn);
        if (hIcon) {
            NOTIFYICONDATAW nid = {0};
            nid.cbSize = sizeof(nid);
            nid.hWnd = g_trayHwnd;
            nid.uID = CLOCK_ID_TRAY_APP_ICON;
            nid.uFlags = NIF_ICON;
            nid.hIcon = hIcon;
            Shell_NotifyIconW(NIM_MODIFY, &nid);
            DestroyIcon(hIcon);
            RecordSuccessfulUpdate();
        }
        return;
    }
    
    if (currentAnim->count <= 0) {
        if (g_isPreviewActive) {
            g_isPreviewActive = FALSE;
            return;
        }
        if (RecordFailedUpdate()) {
            FallbackToLogoIcon();
        }
        return;
    }
    
    if (*currentIndex >= currentAnim->count) *currentIndex = 0;
    
    HICON hIcon = currentAnim->icons[*currentIndex];
    if (!hIcon) {
        WriteLog(LOG_LEVEL_WARNING, "NULL icon at index %d", *currentIndex);
        if (g_isPreviewActive) {
            g_isPreviewActive = FALSE;
            return;
        }
        if (RecordFailedUpdate()) {
            FallbackToLogoIcon();
        }
        return;
    }
    
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_trayHwnd;
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON;
    nid.hIcon = hIcon;
    
    BOOL success = Shell_NotifyIconW(NIM_MODIFY, &nid);
    
    if (success) {
        RecordSuccessfulUpdate();
        FrameRateController_RecordLatency(&g_frameRateCtrl, GetTickCount());
    } else {
        WriteLog(LOG_LEVEL_WARNING, "Shell_NotifyIconW failed");
        if (RecordFailedUpdate()) {
            FallbackToLogoIcon();
        }
    }
}

/**
 * @brief Request tray update (thread-safe)
 */
static void RequestTrayIconUpdate(void) {
    if (!g_trayHwnd || !IsWindow(g_trayHwnd)) return;
    
    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
        g_pendingTrayUpdate = TRUE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_pendingTrayUpdate = TRUE;
    }

    if (IsTrayInteractionSuspended()) {
        return;
    }
    
    PostMessage(g_trayHwnd, WM_TRAY_UPDATE_ICON, 0, 0);
}

/**
 * @brief Timer callback (worker thread)
 */
static void TrayAnimationTimerCallback(void* userData) {
    (void)userData;
    
    /* Skip logic for percent icons (updated separately) and __none__ (static) */
    if (!g_isPreviewActive && IsBuiltinAnimationName(g_animationName)) {
        return;
    }
    
    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    
    LoadedAnimation* currentAnim = g_isPreviewActive ? &g_previewAnimation : &g_mainAnimation;
    int* currentIndex = g_isPreviewActive ? &g_previewIndex : &g_mainIndex;
    
    if (currentAnim->count > 0 && currentAnim->isAnimated) {
        UINT baseDelay = currentAnim->delays[*currentIndex];
        if (baseDelay == 0) baseDelay = g_baseFolderInterval;
        
        UINT scaledDelay = ComputeScaledDelay(baseDelay);
        
        /* Debug logging for animation timing */
        /* WriteLog(LOG_LEVEL_DEBUG, "TrayAnim: Index=%d/%d BaseDelay=%u Scaled=%u", 
                 *currentIndex, currentAnim->count, baseDelay, scaledDelay); */

        if (FrameRateController_ShouldAdvanceFrame(&g_frameRateCtrl, INTERNAL_TICK_INTERVAL_MS, scaledDelay)) {
            *currentIndex = (*currentIndex + 1) % currentAnim->count;
        }
    }
    
    if (FrameRateController_ShouldUpdateTray(&g_frameRateCtrl)) {
        if (g_criticalSectionInitialized) {
            LeaveCriticalSection(&g_animCriticalSection);
        }
        RequestTrayIconUpdate();
        return;
    }
    
    if (g_criticalSectionInitialized) {
        LeaveCriticalSection(&g_animCriticalSection);
    }
}

/**
 * @brief Start animation system
 */
void StartTrayAnimation(HWND hwnd, UINT intervalMs) {
    g_trayHwnd = hwnd;
    g_baseFolderInterval = intervalMs > 0 ? intervalMs : 150;
    
    /* Read folder interval from config */
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    int folderMs = ReadIniInt("Animation", "ANIMATION_FOLDER_INTERVAL_MS", (int)g_baseFolderInterval, config_path);
    if (folderMs > 0) {
        g_baseFolderInterval = (UINT)folderMs;
    }
    
    /* Initialize resources - use atomic operation for thread safety */
    if (InterlockedCompareExchange(&g_criticalSectionInitialized, 1, 0) == 0) {
        InitializeCriticalSection(&g_animCriticalSection);
    }
    
    if (!g_memoryPool) {
        g_memoryPool = MemoryPool_Create(MEMORY_POOL_SIZE);
    }
    
    LoadedAnimation_Init(&g_mainAnimation);
    LoadedAnimation_Init(&g_previewAnimation);
    g_pendingPreviewName[0] = '\0';
    g_previewAnimationName[0] = '\0';
    g_mainIndex = 0;
    g_previewIndex = 0;
    g_isPreviewActive = FALSE;
    InterlockedExchange(&g_previewRequestSerial, 0);
    
    FrameRateController_Init(&g_frameRateCtrl, TRAY_UPDATE_INTERVAL_MS);
    
    /* Load animation from config */
    char nameBuf[MAX_PATH] = {0};
    ReadIniString("Animation", "ANIMATION_PATH", "__logo__", nameBuf, sizeof(nameBuf), config_path);
    NormalizeAnimConfigValue(nameBuf);
    
    if (nameBuf[0] != '\0') {
        const char* prefix = ANIMATIONS_PATH_PREFIX;
        if (IsBuiltinAnimationName(nameBuf)) {
            strncpy(g_animationName, nameBuf, sizeof(g_animationName) - 1);
        } else if (_strnicmp(nameBuf, prefix, (int)strlen(prefix)) == 0) {
            const char* rel = nameBuf + strlen(prefix);
            if (*rel) {
                strncpy(g_animationName, rel, sizeof(g_animationName) - 1);
            }
        } else {
            strncpy(g_animationName, nameBuf, sizeof(g_animationName) - 1);
        }
        g_animationName[sizeof(g_animationName) - 1] = '\0';
    }
    
    /* Load frames */
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    LoadAnimationByName(g_animationName, &g_mainAnimation, g_memoryPool, cx, cy);
    
    if (g_mainAnimation.count > 0) {
        UpdateTrayIconToCurrentFrame();
    }

    EnsureTrayAnimationTimerState();
}

/**
 * @brief Stop animation system
 */
void StopTrayAnimation(HWND hwnd) {
    (void)hwnd;

    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
        g_pendingPreviewName[0] = '\0';
        g_previewAnimationName[0] = '\0';
        g_isPreviewActive = FALSE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_pendingPreviewName[0] = '\0';
        g_previewAnimationName[0] = '\0';
        g_isPreviewActive = FALSE;
    }
    InterlockedIncrement(&g_previewRequestSerial);

    ShutdownPreviewWorker();

    CleanupAnimationTimer();

    LoadedAnimation_Free(&g_mainAnimation);
    LoadedAnimation_Free(&g_previewAnimation);

    if (g_memoryPool) {
        MemoryPool_Destroy(g_memoryPool);
        g_memoryPool = NULL;
    }

    if (g_criticalSectionInitialized == 1) {
        DeleteCriticalSection(&g_animCriticalSection);
        InterlockedExchange(&g_criticalSectionInitialized, 0);
    }

    g_consecutiveUpdateFailures = 0;
    g_lastSuccessfulUpdateTime = 0;
    g_trayHwnd = NULL;
}

/**
 * @brief Get current animation name
 */
const char* GetCurrentAnimationName(void) {
    return g_animationName;
}

/**
 * @brief Set current animation
 */
BOOL SetCurrentAnimationName(const char* name) {
    if (!name || !*name) return FALSE;

    /* Prevent redundant reloads if name is same and no preview is active */
    if (!g_isPreviewActive && _stricmp(g_animationName, name) == 0) {
        return TRUE;
    }

    /* Validate animation exists */
    if (!IsValidAnimationSource(name)) {
        return FALSE;
    }
    
    /* Seamless preview promotion */
    if (g_isPreviewActive && g_previewAnimationName[0] != '\0' &&
        _stricmp(g_previewAnimationName, name) == 0 && 
        (g_previewAnimation.count > 0 || g_previewAnimation.sourceType == ANIM_SOURCE_PERCENT ||
         g_previewAnimation.sourceType == ANIM_SOURCE_CAPSLOCK)) {
        LoadedAnimation oldMain;
        LoadedAnimation_Init(&oldMain);

        if (g_criticalSectionInitialized) {
            EnterCriticalSection(&g_animCriticalSection);
        }

        SwapLoadedAnimation(&oldMain, &g_mainAnimation);
        SwapLoadedAnimation(&g_mainAnimation, &g_previewAnimation);
        LoadedAnimation_Init(&g_previewAnimation);
        g_mainIndex = g_previewIndex;
        
        /* Clear preview */
        g_previewIndex = 0;
        g_isPreviewActive = FALSE;
        g_previewAnimationName[0] = '\0';
        g_pendingPreviewName[0] = '\0';
        InterlockedIncrement(&g_previewRequestSerial);
        strncpy(g_animationName, name, sizeof(g_animationName) - 1);
        g_animationName[sizeof(g_animationName) - 1] = '\0';
        
        if (g_criticalSectionInitialized) {
            LeaveCriticalSection(&g_animCriticalSection);
        }

        LoadedAnimation_Free(&oldMain);
        
        /* Persist to config */
        char config_path[MAX_PATH] = {0};
        GetConfigPath(config_path, sizeof(config_path));
        char animPath[MAX_PATH];
        
        if (IsBuiltinAnimationName(name)) {
            snprintf(animPath, sizeof(animPath), "%s", name);
        } else {
            snprintf(animPath, sizeof(animPath), "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", name);
        }
        
        WriteIniString("Animation", "ANIMATION_PATH", animPath, config_path);

        EnsureTrayAnimationTimerState();
        if (g_trayHwnd) {
            UpdateTrayIconToCurrentFrame();
        }
        
        return TRUE;
    }

    LoadedAnimation newMain;
    LoadedAnimation oldMain;
    LoadedAnimation oldPreview;
    LoadedAnimation_Init(&newMain);
    LoadedAnimation_Init(&oldMain);
    LoadedAnimation_Init(&oldPreview);

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    LoadAnimationByName(name, &newMain, g_memoryPool, cx, cy);

    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
    }

    SwapLoadedAnimation(&oldMain, &g_mainAnimation);
    SwapLoadedAnimation(&g_mainAnimation, &newMain);
    LoadedAnimation_Init(&newMain);
    g_mainIndex = 0;
    g_frameRateCtrl.framePosition = 0.0;

    if (g_isPreviewActive || g_pendingPreviewName[0] != '\0') {
        SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
        LoadedAnimation_Init(&g_previewAnimation);
        g_isPreviewActive = FALSE;
        g_previewAnimationName[0] = '\0';
        g_pendingPreviewName[0] = '\0';
        InterlockedIncrement(&g_previewRequestSerial);
    }

    strncpy(g_animationName, name, sizeof(g_animationName) - 1);
    g_animationName[sizeof(g_animationName) - 1] = '\0';

    if (g_criticalSectionInitialized) {
        LeaveCriticalSection(&g_animCriticalSection);
    }

    LoadedAnimation_Free(&oldMain);
    LoadedAnimation_Free(&oldPreview);
    
    /* Persist to config */
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    char animPath[MAX_PATH];
    
    if (IsBuiltinAnimationName(name)) {
        snprintf(animPath, sizeof(animPath), "%s", name);
    } else {
        snprintf(animPath, sizeof(animPath), "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", name);
    }
    
    WriteIniString("Animation", "ANIMATION_PATH", animPath, config_path);

    EnsureTrayAnimationTimerState();
    if (g_trayHwnd) {
        UpdateTrayIconToCurrentFrame();
    }
    
    return TRUE;
}

/**
 * @brief Preview animation from file path
 */
void PreviewAnimationFromFile(HWND hwnd, const char* filePath) {
    (void)hwnd;
    if (!filePath || !*filePath) return;

    LoadedAnimation newPreview;
    LoadedAnimation oldPreview;
    LoadedAnimation_Init(&newPreview);
    LoadedAnimation_Init(&oldPreview);

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    LoadAnimationFromPath(filePath, &newPreview, g_memoryPool, cx, cy);

    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    InterlockedIncrement(&g_previewRequestSerial);
    g_pendingPreviewName[0] = '\0';
    SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
    SwapLoadedAnimation(&g_previewAnimation, &newPreview);
    LoadedAnimation_Init(&newPreview);
    g_previewIndex = 0;
    g_isPreviewActive = TRUE;
    strncpy(g_previewAnimationName, filePath, sizeof(g_previewAnimationName) - 1);
    g_previewAnimationName[sizeof(g_previewAnimationName) - 1] = '\0';
    
    if (g_criticalSectionInitialized) {
        LeaveCriticalSection(&g_animCriticalSection);
    }

    LoadedAnimation_Free(&oldPreview);
    
    /* Force update */
    EnsureTrayAnimationTimerState();
    UpdateTrayIconToCurrentFrame();
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
            continue;
        }

        char requestedName[MAX_PATH] = {0};
        LONG requestSerial = InterlockedCompareExchange(&g_previewRequestSerial, 0, 0);

        if (g_criticalSectionInitialized) {
            EnterCriticalSection(&g_animCriticalSection);
            strncpy(requestedName, g_pendingPreviewName, sizeof(requestedName) - 1);
            requestedName[sizeof(requestedName) - 1] = '\0';
            LeaveCriticalSection(&g_animCriticalSection);
        } else {
            strncpy(requestedName, g_pendingPreviewName, sizeof(requestedName) - 1);
            requestedName[sizeof(requestedName) - 1] = '\0';
        }

        if (!requestedName[0]) {
            continue;
        }

        WriteLog(LOG_LEVEL_INFO, "PreviewWorkerThread: loading '%s'", requestedName);

        LoadedAnimation tempAnim;
        LoadedAnimation oldPreview;
        LoadedAnimation_Init(&tempAnim);
        LoadedAnimation_Init(&oldPreview);

        int cx = GetSystemMetrics(SM_CXSMICON);
        int cy = GetSystemMetrics(SM_CYSMICON);
        MemoryPool* localPool = MemoryPool_Create(MEMORY_POOL_SIZE);
        LoadAnimationByName(requestedName, &tempAnim, localPool, cx, cy);
        if (localPool) {
            MemoryPool_Destroy(localPool);
        }

        if (g_previewStopEvent && WaitForSingleObject(g_previewStopEvent, 0) == WAIT_OBJECT_0) {
            LoadedAnimation_Free(&tempAnim);
            break;
        }

        BOOL shouldApply = FALSE;
        if (g_criticalSectionInitialized) {
            EnterCriticalSection(&g_animCriticalSection);
        }

        if (InterlockedCompareExchange(&g_previewRequestSerial, 0, 0) == requestSerial &&
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
                g_frameRateCtrl.framePosition = 0.0;
                g_isPreviewActive = TRUE;
                strncpy(g_previewAnimationName, requestedName, sizeof(g_previewAnimationName) - 1);
                g_previewAnimationName[sizeof(g_previewAnimationName) - 1] = '\0';
                shouldApply = TRUE;
                WriteLog(LOG_LEVEL_INFO, "PreviewWorkerThread: preview activated for '%s'", requestedName);
            } else {
                SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
                LoadedAnimation_Init(&g_previewAnimation);
                g_isPreviewActive = FALSE;
                g_previewAnimationName[0] = '\0';
                g_pendingPreviewName[0] = '\0';
                shouldApply = TRUE;
                WriteLog(LOG_LEVEL_WARNING, "PreviewWorkerThread: preview failed to load '%s'", requestedName);
            }
        }

        if (shouldApply) {
            g_pendingPreviewName[0] = '\0';
        }

        if (g_criticalSectionInitialized) {
            LeaveCriticalSection(&g_animCriticalSection);
        }

        LoadedAnimation_Free(&oldPreview);
        LoadedAnimation_Free(&tempAnim);

        if (shouldApply) {
            PostPreviewLoadedMessage();
        }
    }

    return 0;
}

/**
 * @brief Start animation preview
 */
void StartAnimationPreview(const char* name) {
    if (!name || !*name) return;

    WriteLog(LOG_LEVEL_INFO, "StartAnimationPreview: called with '%s'", name);

    if (g_isPreviewActive && g_previewAnimationName[0] != '\0' &&
        _stricmp(g_previewAnimationName, name) == 0) {
        WriteLog(LOG_LEVEL_INFO, "StartAnimationPreview: already previewing '%s'", name);
        return;
    }

    if (!EnsurePreviewWorkerStarted()) {
        return;
    }

    LONG requestSerial = InterlockedIncrement(&g_previewRequestSerial);

    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    strncpy(g_pendingPreviewName, name, sizeof(g_pendingPreviewName) - 1);
    g_pendingPreviewName[sizeof(g_pendingPreviewName) - 1] = '\0';
    if (g_criticalSectionInitialized) {
        LeaveCriticalSection(&g_animCriticalSection);
    }

    WriteLog(LOG_LEVEL_INFO, "StartAnimationPreview: queued '%s' (serial=%ld)", name, requestSerial);
    if (g_previewRequestEvent) {
        SetEvent(g_previewRequestEvent);
    }
}

/**
 * @brief Cancel animation preview
 */
void CancelAnimationPreview(void) {
    WriteLog(LOG_LEVEL_INFO, "CancelAnimationPreview: called, g_isPreviewActive=%d", g_isPreviewActive);
    if (!g_isPreviewActive && g_pendingPreviewName[0] == '\0') return;

    InterlockedIncrement(&g_previewRequestSerial);

    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
    }

    WriteLog(LOG_LEVEL_INFO, "CancelAnimationPreview: clearing preview state");
    g_isPreviewActive = FALSE;
    g_previewAnimationName[0] = '\0';
    g_pendingPreviewName[0] = '\0';
    LoadedAnimation_Free(&g_previewAnimation);
    LoadedAnimation_Init(&g_previewAnimation);
    g_frameRateCtrl.framePosition = 0.0;

    if (g_criticalSectionInitialized) {
        LeaveCriticalSection(&g_animCriticalSection);
    }

    WriteLog(LOG_LEVEL_INFO, "CancelAnimationPreview: updating tray icon to current frame");
    EnsureTrayAnimationTimerState();
    UpdateTrayIconToCurrentFrame();
}

/**
 * @brief Preload animation from config
 */
void PreloadAnimationFromConfig(void) {
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    char nameBuf[MAX_PATH] = {0};
    ReadIniString("Animation", "ANIMATION_PATH", "__logo__", nameBuf, sizeof(nameBuf), config_path);
    NormalizeAnimConfigValue(nameBuf);
    
    if (nameBuf[0] != '\0') {
        const char* prefix = ANIMATIONS_PATH_PREFIX;
        if (IsBuiltinAnimationName(nameBuf)) {
            strncpy(g_animationName, nameBuf, sizeof(g_animationName) - 1);
        } else if (_strnicmp(nameBuf, prefix, (int)strlen(prefix)) == 0) {
            const char* rel = nameBuf + strlen(prefix);
            if (*rel) {
                strncpy(g_animationName, rel, sizeof(g_animationName) - 1);
            }
        } else {
            strncpy(g_animationName, nameBuf, sizeof(g_animationName) - 1);
        }
        g_animationName[sizeof(g_animationName) - 1] = '\0';
    }
    
    if (!g_memoryPool) {
        g_memoryPool = MemoryPool_Create(MEMORY_POOL_SIZE);
    }
    
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    LoadAnimationByName(g_animationName, &g_mainAnimation, g_memoryPool, cx, cy);
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
        BYTE andMask[32];
        BYTE xorMask[32];
        memset(andMask, 0xFF, sizeof(andMask));
        memset(xorMask, 0x00, sizeof(xorMask));
        return CreateIcon(NULL, 16, 16, 1, 1, andMask, xorMask);
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
    
    const char* prefix = "%LOCALAPPDATA%\\Catime\\resources\\animations\\";
    char name[MAX_PATH] = {0};
    
    if (IsBuiltinAnimationName(value)) {
        strncpy(name, value, sizeof(name) - 1);
    } else if (_strnicmp(value, prefix, (int)strlen(prefix)) == 0) {
        const char* rel = value + strlen(prefix);
        strncpy(name, rel, sizeof(name) - 1);
    } else {
        strncpy(name, value, sizeof(name) - 1);
    }
    
    if (name[0] == '\0') return;
    if (_stricmp(g_animationName, name) == 0) return;

 
    LoadedAnimation newMain;
    LoadedAnimation oldMain;
    LoadedAnimation oldPreview;
    LoadedAnimation_Init(&newMain);
    LoadedAnimation_Init(&oldMain);
    LoadedAnimation_Init(&oldPreview);

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    LoadAnimationByName(name, &newMain, g_memoryPool, cx, cy);

    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    strncpy(g_animationName, name, sizeof(g_animationName) - 1);
    g_animationName[sizeof(g_animationName) - 1] = '\0';
    SwapLoadedAnimation(&oldMain, &g_mainAnimation);
    SwapLoadedAnimation(&g_mainAnimation, &newMain);
    LoadedAnimation_Init(&newMain);
    g_mainIndex = 0;
    g_frameRateCtrl.framePosition = 0.0;
    
    if (g_isPreviewActive || g_pendingPreviewName[0] != '\0') {
        g_isPreviewActive = FALSE;
        g_previewAnimationName[0] = '\0';
        g_pendingPreviewName[0] = '\0';
        InterlockedIncrement(&g_previewRequestSerial);
        SwapLoadedAnimation(&oldPreview, &g_previewAnimation);
        LoadedAnimation_Init(&g_previewAnimation);
    }

    if (g_criticalSectionInitialized) {
        LeaveCriticalSection(&g_animCriticalSection);
    }

    LoadedAnimation_Free(&oldMain);
    LoadedAnimation_Free(&oldPreview);
    
    EnsureTrayAnimationTimerState();
    if (g_trayHwnd && g_mainAnimation.count > 0) {
        UpdateTrayIconToCurrentFrame();
    }
}

/**
 * @brief Set base interval
 */
void TrayAnimation_SetBaseIntervalMs(UINT ms) {
    if (ms == 0) ms = 150;
    g_baseFolderInterval = ms;
}

/**
 * @brief Set minimum interval
 */
void TrayAnimation_SetMinIntervalMs(UINT ms) {
    g_userMinIntervalMs = ms;
}

/**
 * @brief Recompute timer delay (no-op, kept for compatibility)
 */
void TrayAnimation_RecomputeTimerDelay(void) {
    /* No-op: delay computed dynamically now */
}

/**
 * @brief Clear current animation name to force reload
 */
void TrayAnimation_ClearCurrentName(void) {
    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    g_animationName[0] = '\0';
    if (g_criticalSectionInitialized) {
        LeaveCriticalSection(&g_animCriticalSection);
    }
}

/**
 * @brief Update percent icon if needed
 */
void TrayAnimation_UpdatePercentIconIfNeeded(void) {
    if (!g_trayHwnd || !IsWindow(g_trayHwnd)) return;
    if (IsTrayInteractionSuspended()) return;
    if (!g_animationName[0]) return;
    if (g_isPreviewActive) return;
    
    const BuiltinAnimDef* def = GetBuiltinAnimDef(g_animationName);
    if (!def) return;
    
    HICON hIcon = NULL;
    
    /* Handle percent type (CPU, Memory, Battery) */
    if (def->type == ANIM_SOURCE_PERCENT) {
        int p = 0;
        if (def->getValue) {
            p = def->getValue();
        }
        if (p < 0) p = 0;
        if (p > 100) p = 100;
        hIcon = CreatePercentIcon16(p);
    }
    /* Handle Caps Lock indicator */
    else if (def->type == ANIM_SOURCE_CAPSLOCK) {
        hIcon = CreateCapsLockIcon(IsCapsLockOn());
    }
    else {
        return;
    }
    
    if (!hIcon) return;
    
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_trayHwnd;
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON;
    nid.hIcon = hIcon;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
    
    DestroyIcon(hIcon);
}

/**
 * @brief Handle WM_TRAY_UPDATE_ICON message
 */
BOOL TrayAnimation_HandleUpdateMessage(void) {
    BOOL hasPending = FALSE;

    EnsureTrayAnimationTimerState();
    
    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
        hasPending = g_pendingTrayUpdate;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        hasPending = g_pendingTrayUpdate;
    }
    
    if (hasPending) {
        UpdateTrayIconToCurrentFrame();
        return TRUE;
    }
    
    return FALSE;
}
