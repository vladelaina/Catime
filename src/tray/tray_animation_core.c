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
static BOOL g_isPreviewActive = FALSE;
static HWND g_trayHwnd = NULL;
static UINT g_baseFolderInterval = 150;
static UINT g_userMinIntervalMs = 0;

/* Main animation */
static LoadedAnimation g_mainAnimation;
static int g_mainIndex = 0;

/* Preview animation */
static LoadedAnimation g_previewAnimation;
static int g_previewIndex = 0;

/* Resources */
static MemoryPool* g_memoryPool = NULL;
static FrameRateController g_frameRateCtrl;

/* Thread safety */
static CRITICAL_SECTION g_animCriticalSection;
static BOOL g_criticalSectionInitialized = FALSE;
static BOOL g_pendingTrayUpdate = FALSE;

/* Error recovery:
 * - 5000ms timeout: Reasonable duration before declaring icon update as failed
 */
static UINT g_consecutiveUpdateFailures = 0;
static DWORD g_lastSuccessfulUpdateTime = 0;
#define MAX_CONSECUTIVE_FAILURES 5
#define UPDATE_TIMEOUT_MS 5000

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

    double percent = 0.0;
    AnimationSpeedMetric metric = GetAnimationSpeedMetric();
    
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
    
    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
        g_pendingTrayUpdate = FALSE;
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        g_pendingTrayUpdate = FALSE;
    }
    
    LoadedAnimation* currentAnim = g_isPreviewActive ? &g_previewAnimation : &g_mainAnimation;
    int* currentIndex = g_isPreviewActive ? &g_previewIndex : &g_mainIndex;
    
    /* Handle percent icons */
    if (currentAnim->sourceType == ANIM_SOURCE_PERCENT && !g_isPreviewActive) {
        float cpu = 0.0f, mem = 0.0f;
        SystemMonitor_GetUsage(&cpu, &mem);
        int p = (_stricmp(g_animationName, "__cpu__") == 0) ? (int)(cpu + 0.5f) : (int)(mem + 0.5f);
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
    
    PostMessage(g_trayHwnd, WM_TRAY_UPDATE_ICON, 0, 0);
}

/**
 * @brief Timer callback (worker thread)
 */
static void TrayAnimationTimerCallback(void* userData) {
    (void)userData;
    
    /* Skip logic for percent icons (updated separately) */
    if ((_stricmp(g_animationName, "__cpu__") == 0 || _stricmp(g_animationName, "__mem__") == 0) 
        && !g_isPreviewActive) {
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
    
    /* Initialize resources */
    if (!g_criticalSectionInitialized) {
        InitializeCriticalSection(&g_animCriticalSection);
        g_criticalSectionInitialized = TRUE;
    }
    
    if (!g_memoryPool) {
        g_memoryPool = MemoryPool_Create(MEMORY_POOL_SIZE);
    }
    
    LoadedAnimation_Init(&g_mainAnimation);
    LoadedAnimation_Init(&g_previewAnimation);
    g_mainIndex = 0;
    g_previewIndex = 0;
    g_isPreviewActive = FALSE;
    
    FrameRateController_Init(&g_frameRateCtrl, TRAY_UPDATE_INTERVAL_MS);
    
    /* Load animation from config */
    char nameBuf[MAX_PATH] = {0};
    ReadIniString("Animation", "ANIMATION_PATH", "__logo__", nameBuf, sizeof(nameBuf), config_path);
    NormalizeAnimConfigValue(nameBuf);
    
    if (nameBuf[0] != '\0') {
        const char* prefix = ANIMATIONS_PATH_PREFIX;
        if (_stricmp(nameBuf, "__logo__") == 0 || _stricmp(nameBuf, "__cpu__") == 0 || _stricmp(nameBuf, "__mem__") == 0) {
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
    
    /* Start timer */
    InitializeAnimationTimer(hwnd, INTERNAL_TICK_INTERVAL_MS, TrayAnimationTimerCallback, NULL);
}

/**
 * @brief Stop animation system
 */
void StopTrayAnimation(HWND hwnd) {
    CleanupAnimationTimer();
    
    LoadedAnimation_Free(&g_mainAnimation);
    LoadedAnimation_Free(&g_previewAnimation);
    
    if (g_memoryPool) {
        MemoryPool_Destroy(g_memoryPool);
        g_memoryPool = NULL;
    }
    
    if (g_criticalSectionInitialized) {
        DeleteCriticalSection(&g_animCriticalSection);
        g_criticalSectionInitialized = FALSE;
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
    
    /* Validate animation exists */
    if (!IsValidAnimationSource(name)) return FALSE;
    
    /* Seamless preview promotion */
    if (g_isPreviewActive && g_previewAnimationName[0] != '\0' &&
        _stricmp(g_previewAnimationName, name) == 0 && g_previewAnimation.count > 0) {
        
        if (g_criticalSectionInitialized) {
            EnterCriticalSection(&g_animCriticalSection);
        }
        
        LoadedAnimation_Free(&g_mainAnimation);
        
        g_mainAnimation = g_previewAnimation;
        g_mainIndex = g_previewIndex;
        
        /* Clear preview */
        LoadedAnimation_Init(&g_previewAnimation);
        g_previewIndex = 0;
        g_isPreviewActive = FALSE;
        g_previewAnimationName[0] = '\0';
        
        if (g_criticalSectionInitialized) {
            LeaveCriticalSection(&g_animCriticalSection);
        }
        
        strncpy(g_animationName, name, sizeof(g_animationName) - 1);
        g_animationName[sizeof(g_animationName) - 1] = '\0';
        
        /* Persist to config */
        char config_path[MAX_PATH] = {0};
        GetConfigPath(config_path, sizeof(config_path));
        char animPath[MAX_PATH];
        
        if (_stricmp(name, "__logo__") == 0 || _stricmp(name, "__cpu__") == 0 || _stricmp(name, "__mem__") == 0) {
            snprintf(animPath, sizeof(animPath), "%s", name);
        } else {
            snprintf(animPath, sizeof(animPath), "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", name);
        }
        
        WriteIniString("Animation", "ANIMATION_PATH", animPath, config_path);
        
        if (g_trayHwnd) {
            UpdateTrayIconToCurrentFrame();
        }
        
        return TRUE;
    }
    
    /* Load new animation */
    strncpy(g_animationName, name, sizeof(g_animationName) - 1);
    g_animationName[sizeof(g_animationName) - 1] = '\0';
    
    LoadedAnimation_Free(&g_mainAnimation);
    LoadedAnimation_Init(&g_mainAnimation);
    
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    LoadAnimationByName(g_animationName, &g_mainAnimation, g_memoryPool, cx, cy);
    
    g_mainIndex = 0;
    g_frameRateCtrl.framePosition = 0.0;
    
    if (g_isPreviewActive) {
        g_isPreviewActive = FALSE;
        g_previewAnimationName[0] = '\0';
        LoadedAnimation_Free(&g_previewAnimation);
    }
    
    /* Persist to config */
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    char animPath[MAX_PATH];
    
    if (_stricmp(name, "__logo__") == 0 || _stricmp(name, "__cpu__") == 0 || _stricmp(name, "__mem__") == 0) {
        snprintf(animPath, sizeof(animPath), "%s", name);
    } else {
        snprintf(animPath, sizeof(animPath), "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", name);
    }
    
    WriteIniString("Animation", "ANIMATION_PATH", animPath, config_path);
    
    if (g_trayHwnd) {
        UpdateTrayIconToCurrentFrame();
    }
    
    return TRUE;
}

/**
 * @brief Start animation preview
 */
void StartAnimationPreview(const char* name) {
    if (!name || !*name) return;
    
    strncpy(g_previewAnimationName, name, sizeof(g_previewAnimationName) - 1);
    g_previewAnimationName[sizeof(g_previewAnimationName) - 1] = '\0';
    
    LoadedAnimation_Free(&g_previewAnimation);
    LoadedAnimation_Init(&g_previewAnimation);
    
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    LoadAnimationByName(name, &g_previewAnimation, g_memoryPool, cx, cy);
    
    if (g_previewAnimation.count > 0) {
        g_isPreviewActive = TRUE;
        g_previewIndex = 0;
        g_frameRateCtrl.framePosition = 0.0;
        UpdateTrayIconToCurrentFrame();
    } else {
        WriteLog(LOG_LEVEL_WARNING, "Animation preview failed to load: '%s'", name);
        g_previewAnimationName[0] = '\0';
    }
}

/**
 * @brief Cancel animation preview
 */
void CancelAnimationPreview(void) {
    if (!g_isPreviewActive) return;
    
    g_isPreviewActive = FALSE;
    g_previewAnimationName[0] = '\0';
    LoadedAnimation_Free(&g_previewAnimation);
    g_frameRateCtrl.framePosition = 0.0;
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
        if (_stricmp(nameBuf, "__logo__") == 0 || _stricmp(nameBuf, "__cpu__") == 0 || _stricmp(nameBuf, "__mem__") == 0) {
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
    if (_stricmp(g_animationName, "__cpu__") == 0 || _stricmp(g_animationName, "__mem__") == 0) {
        return NULL;
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
    
    if (_stricmp(value, "__logo__") == 0 || _stricmp(value, "__cpu__") == 0 || _stricmp(value, "__mem__") == 0) {
        strncpy(name, value, sizeof(name) - 1);
    } else if (_strnicmp(value, prefix, (int)strlen(prefix)) == 0) {
        const char* rel = value + strlen(prefix);
        strncpy(name, rel, sizeof(name) - 1);
    } else {
        strncpy(name, value, sizeof(name) - 1);
    }
    
    if (name[0] == '\0') return;
    if (_stricmp(g_animationName, name) == 0) return;
    
    strncpy(g_animationName, name, sizeof(g_animationName) - 1);
    g_animationName[sizeof(g_animationName) - 1] = '\0';
    
    LoadedAnimation_Free(&g_mainAnimation);
    LoadedAnimation_Init(&g_mainAnimation);
    
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    LoadAnimationByName(g_animationName, &g_mainAnimation, g_memoryPool, cx, cy);
    
    g_mainIndex = 0;
    g_frameRateCtrl.framePosition = 0.0;
    
    if (g_isPreviewActive) {
        g_isPreviewActive = FALSE;
        g_previewAnimationName[0] = '\0';
        LoadedAnimation_Free(&g_previewAnimation);
    }
    
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
 * @brief Update percent icon if needed
 */
void TrayAnimation_UpdatePercentIconIfNeeded(void) {
    if (!g_trayHwnd || !IsWindow(g_trayHwnd)) return;
    if (!g_animationName[0]) return;
    if (_stricmp(g_animationName, "__cpu__") != 0 && _stricmp(g_animationName, "__mem__") != 0) return;
    if (g_isPreviewActive) return;
    
    float cpu = 0.0f, mem = 0.0f;
    SystemMonitor_GetUsage(&cpu, &mem);
    int p = (_stricmp(g_animationName, "__cpu__") == 0) ? (int)(cpu + 0.5f) : (int)(mem + 0.5f);
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    
    HICON hIcon = CreatePercentIcon16(p);
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

