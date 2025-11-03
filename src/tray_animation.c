/**
 * @file tray_animation.c
 * @brief High-precision system tray icon animation
 * 
 * Architecture:
 * - Multimedia timer (timeSetEvent) at 10ms for smooth internal frame logic
 * - Fixed 50ms tray update interval to prevent Windows Explorer throttling
 * - Adaptive frame rate monitoring to handle system load variations
 * - Pre-rendered HICON frames to eliminate runtime blending overhead
 */

#include <windows.h> 
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include <wincodec.h>
#include <propvarutil.h>
#include <objbase.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

#include "../include/tray.h"
#include "../include/config.h"
#include "../include/timer.h"
#include "../include/tray_menu.h"
#include "../include/tray_animation.h"
#include "../include/system_monitor.h"
#include "../include/log.h"

/** @brief Animation menu entry for natural sorting (folders first, then numeric) */
typedef struct {
    wchar_t name[MAX_PATH];
    char rel_path_utf8[MAX_PATH];
    BOOL is_dir;
} AnimationEntry;

/**
 * @brief Natural string comparison with numeric ordering
 * @details Handles leading zeros and multi-digit numbers correctly (e.g., "img2" < "img10")
 */
static int NaturalCompareW(const wchar_t* a, const wchar_t* b) {
    const wchar_t* pa = a;
    const wchar_t* pb = b;
    while (*pa && *pb) {
        if (iswdigit(*pa) && iswdigit(*pb)) {
            const wchar_t* za = pa; while (*za == L'0') za++;
            const wchar_t* zb = pb; while (*zb == L'0') zb++;
            size_t leadA = (size_t)(za - pa);
            size_t leadB = (size_t)(zb - pb);
            if (leadA != leadB) return (leadA > leadB) ? -1 : 1;
            const wchar_t* ea = za; while (iswdigit(*ea)) ea++;
            const wchar_t* eb = zb; while (iswdigit(*eb)) eb++;
            size_t lena = (size_t)(ea - za);
            size_t lenb = (size_t)(eb - zb);
            if (lena != lenb) return (lena < lenb) ? -1 : 1;
            int dcmp = wcsncmp(za, zb, lena);
            if (dcmp != 0) return (dcmp < 0) ? -1 : 1;
            pa = ea;
            pb = eb;
            continue;
        }
        wchar_t ca = towlower(*pa);
        wchar_t cb = towlower(*pb);
        if (ca != cb) return (ca < cb) ? -1 : 1;
        pa++; pb++;
    }
    if (*pa) return 1;
    if (*pb) return -1;
    return 0;
}

/** @brief qsort comparator: directories first, then natural numeric order */
static int CompareAnimationEntries(const void* a, const void* b) {
    const AnimationEntry* entryA = (const AnimationEntry*)a;
    const AnimationEntry* entryB = (const AnimationEntry*)b;
    if (entryA->is_dir != entryB->is_dir) {
        return entryB->is_dir - entryA->is_dir;
    }
    return NaturalCompareW(entryA->name, entryB->name);
}

static void CALLBACK HighPrecisionTimerCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);
static void CALLBACK FallbackTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);

/** @brief Timing configuration: decouples internal logic (10ms) from tray updates (50ms) */
#define INTERNAL_TICK_INTERVAL_MS 10
#define TRAY_UPDATE_INTERVAL_MS 50
#define TRAY_ANIM_TIMER_ID 42420
#define WM_TRAY_UPDATE_ICON (WM_USER + 100)

static UINT g_userMinIntervalMs = 0;

/** @brief High-precision timer state */
static MMRESULT g_mmTimerId = 0;
static BOOL g_useHighPrecisionTimer = TRUE;
static UINT g_internalAccumulator = 0;
static UINT g_currentEffectiveInterval = TRAY_UPDATE_INTERVAL_MS;
static BOOL g_pendingTrayUpdate = FALSE;
static CRITICAL_SECTION g_animCriticalSection;
static BOOL g_criticalSectionInitialized = FALSE;

/** @brief Adaptive frame rate tracking */
static DWORD g_lastTrayUpdateTime = 0;
static UINT g_consecutiveLateUpdates = 0;
static UINT g_targetInternalInterval = INTERNAL_TICK_INTERVAL_MS;
static double g_internalFramePosition = 0.0;

/** @brief Animation resources: icons, indices, and timing */
static HICON g_trayIcons[MAX_TRAY_FRAMES];
static int g_trayIconCount = 0;
static int g_trayIconIndex = 0;
static UINT g_trayInterval = 0;
static HWND g_trayHwnd = NULL;
static char g_animationName[MAX_PATH] = "__logo__";
static char g_previewAnimationName[MAX_PATH] = "";
static BOOL g_isPreviewActive = FALSE;
static HICON g_previewIcons[MAX_TRAY_FRAMES];
static int g_previewCount = 0;
static int g_previewIndex = 0;
static BOOL g_isAnimated = FALSE;
static UINT g_frameDelaysMs[MAX_TRAY_FRAMES];
static BOOL g_isPreviewAnimated = FALSE;
static UINT g_previewFrameDelaysMs[MAX_TRAY_FRAMES];

/** @brief Composition canvas for GIF/WebP frame blending (32bpp PBGRA) */
static UINT g_animCanvasWidth = 0;
static UINT g_animCanvasHeight = 0;
static BYTE* g_animCanvas = NULL;
static BYTE* g_previewAnimCanvas = NULL;

/** @brief Memory pool: reuses 256KB buffer to reduce malloc overhead in frame decoding loops */
#define MEMORY_POOL_BUFFER_SIZE (256 * 1024)
static BYTE* g_memoryPoolBuffer = NULL;
static SIZE_T g_memoryPoolSize = 0;
static BOOL g_memoryPoolInUse = FALSE;

/** @brief Error recovery: fallback to logo after consecutive failures or timeout */
static UINT g_consecutiveUpdateFailures = 0;
static DWORD g_lastSuccessfulUpdateTime = 0;
#define MAX_CONSECUTIVE_FAILURES 5
#define UPDATE_TIMEOUT_MS 5000

/** @brief Routes decoded frames to either main tray or preview slot */
typedef struct {
    HICON* icons;
    int*   count;
    int*   index;
    UINT*  delays;
    BOOL*  isAnimatedFlag;
    BYTE** canvas;
} DecodeTarget;

/**
 * @brief Allocate from pool or fallback to malloc
 * @param size Requested size in bytes
 * @return Memory pointer or NULL on failure
 * @note Pool is single-use: only one allocation active at a time
 */
static void* MemoryPool_Alloc(SIZE_T size) {
    if (!g_memoryPoolInUse && size <= MEMORY_POOL_BUFFER_SIZE) {
        if (!g_memoryPoolBuffer) {
            g_memoryPoolBuffer = (BYTE*)malloc(MEMORY_POOL_BUFFER_SIZE);
            if (g_memoryPoolBuffer) {
                g_memoryPoolSize = MEMORY_POOL_BUFFER_SIZE;
            }
        }
        
        if (g_memoryPoolBuffer && size <= g_memoryPoolSize) {
            g_memoryPoolInUse = TRUE;
            return g_memoryPoolBuffer;
        }
    }
    
    return malloc(size);
}

/**
 * @brief Release memory back to pool or free it
 * @param ptr Pointer to memory (pool or heap)
 */
static void MemoryPool_Free(void* ptr) {
    if (!ptr) return;
    
    if (ptr == g_memoryPoolBuffer) {
        g_memoryPoolInUse = FALSE;
        return;
    }
    
    free(ptr);
}

/**
 * @brief Destroy memory pool on shutdown
 */
static void MemoryPool_Cleanup(void) {
    if (g_memoryPoolBuffer) {
        free(g_memoryPoolBuffer);
        g_memoryPoolBuffer = NULL;
        g_memoryPoolSize = 0;
        g_memoryPoolInUse = FALSE;
    }
}

/** @brief Reset error counters after successful update */
static void RecordSuccessfulUpdate(void) {
    g_consecutiveUpdateFailures = 0;
    g_lastSuccessfulUpdateTime = GetTickCount();
}

/**
 * @brief Track update failure and determine if fallback needed
 * @return TRUE if thresholds exceeded (5 failures or 5s timeout)
 */
static BOOL RecordFailedUpdate(void) {
    g_consecutiveUpdateFailures++;
    
    if (g_consecutiveUpdateFailures >= MAX_CONSECUTIVE_FAILURES) {
        WriteLog(LOG_LEVEL_WARNING, 
                 "Animation update failed %d times consecutively, entering fallback mode",
                 g_consecutiveUpdateFailures);
        return TRUE;
    }
    
    DWORD currentTime = GetTickCount();
    if (g_lastSuccessfulUpdateTime > 0) {
        DWORD elapsed = currentTime - g_lastSuccessfulUpdateTime;
        if (elapsed > UPDATE_TIMEOUT_MS) {
            WriteLog(LOG_LEVEL_WARNING, 
                     "No successful update for %dms, entering fallback mode", elapsed);
            return TRUE;
        }
    }
    
    return FALSE;
}

/** @brief Switch to logo icon as safe fallback */
static void FallbackToLogoIcon(void) {
    WriteLog(LOG_LEVEL_INFO, "Falling back to logo icon due to animation errors");
    g_consecutiveUpdateFailures = 0;
    g_lastSuccessfulUpdateTime = GetTickCount();
    SetCurrentAnimationName("__logo__");
}

/** @brief Strip whitespace and quotes from config values */
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
 * @brief Select target for decoded frames (main tray or preview)
 * @param isPreview TRUE for preview slot, FALSE for main tray
 * @return Pointers to target arrays and state variables
 */
static DecodeTarget GetDecodeTarget(BOOL isPreview) {
    if (isPreview) {
        return (DecodeTarget){
            .icons = g_previewIcons,
            .count = &g_previewCount,
            .index = &g_previewIndex,
            .delays = g_previewFrameDelaysMs,
            .isAnimatedFlag = &g_isPreviewAnimated,
            .canvas = &g_previewAnimCanvas
        };
    }
    return (DecodeTarget){
        .icons = g_trayIcons,
        .count = &g_trayIconCount,
        .index = &g_trayIconIndex,
        .delays = g_frameDelaysMs,
        .isAnimatedFlag = &g_isAnimated,
        .canvas = &g_animCanvas
    };
}

/**
 * @brief Apply dynamic speed scaling to base frame delay
 * @param baseDelay Base delay in milliseconds
 * @return Scaled delay (respects user minimum if set)
 * @note Scaling is based on CPU/mem usage or timer progress
 */
static UINT ComputeScaledDelay(UINT baseDelay) {
    if (baseDelay == 0) baseDelay = g_trayInterval > 0 ? g_trayInterval : 150;

    double percent = 0.0;
    AnimationSpeedMetric metric = GetAnimationSpeedMetric();
    if (metric == ANIMATION_SPEED_CPU) {
        float cpu = 0.0f, mem = 0.0f;
        SystemMonitor_GetUsage(&cpu, &mem);
        percent = cpu;
    } else if (metric == ANIMATION_SPEED_TIMER) {
        if (!CLOCK_SHOW_CURRENT_TIME) {
            if (!CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0) {
                double p = (double)countdown_elapsed_time / (double)CLOCK_TOTAL_TIME;
                if (p < 0.0) p = 0.0; if (p > 1.0) p = 1.0;
                percent = p * 100.0;
            } else {
                percent = 0.0;
            }
        } else {
            percent = 0.0;
        }
    } else {
        float cpu = 0.0f, mem = 0.0f;
        SystemMonitor_GetUsage(&cpu, &mem);
        percent = mem;
    }

    BOOL applyScaling = TRUE;
    if (metric == ANIMATION_SPEED_TIMER) {
        extern BOOL CLOCK_COUNT_UP;
        extern BOOL CLOCK_SHOW_CURRENT_TIME;
        extern int CLOCK_TOTAL_TIME;
        if (CLOCK_SHOW_CURRENT_TIME || CLOCK_COUNT_UP || CLOCK_TOTAL_TIME <= 0) {
            applyScaling = FALSE;
        }
        if (percent >= 100.0) {
            applyScaling = FALSE;
        }
    }

    double scalePercent = 100.0;
    if (applyScaling) {
        scalePercent = GetAnimationSpeedScaleForPercent(percent);
        if (scalePercent <= 0.0) scalePercent = 100.0;
    } else {
        scalePercent = GetAnimationSpeedScaleForPercent(0.0);
        if (scalePercent <= 0.0) scalePercent = 100.0;
    }
    double scale = scalePercent / 100.0;
    if (scale < 0.1) scale = 0.1;
    UINT scaledDelay = (UINT)(baseDelay / scale);
    UINT floorMs = (g_userMinIntervalMs > 0) ? g_userMinIntervalMs : 0;
    if (scaledDelay < floorMs) scaledDelay = floorMs;
    return scaledDelay;
}

/**
 * @brief Adapt update interval based on actual performance
 * @note Slows down if system can't keep up (>150% target for 3 updates)
 * @note Speeds back up gradually if performing well (<80% target)
 */
static void AdaptiveFrameRateUpdate(void) {
    DWORD currentTime = GetTickCount();
    
    if (g_lastTrayUpdateTime == 0) {
        g_lastTrayUpdateTime = currentTime;
        return;
    }
    
    DWORD actualElapsed = currentTime - g_lastTrayUpdateTime;
    g_lastTrayUpdateTime = currentTime;
    
    if (actualElapsed > g_currentEffectiveInterval * 3 / 2) {
        g_consecutiveLateUpdates++;
        
        if (g_consecutiveLateUpdates >= 3) {
            if (g_currentEffectiveInterval < 200) {
                g_currentEffectiveInterval += 10;
            }
            g_consecutiveLateUpdates = 0;
        }
    } else if (actualElapsed < g_currentEffectiveInterval * 4 / 5) {
        g_consecutiveLateUpdates = 0;
        
        if (g_currentEffectiveInterval > TRAY_UPDATE_INTERVAL_MS) {
            g_currentEffectiveInterval -= 2;
            if (g_currentEffectiveInterval < TRAY_UPDATE_INTERVAL_MS) {
                g_currentEffectiveInterval = TRAY_UPDATE_INTERVAL_MS;
            }
        }
    } else {
        g_consecutiveLateUpdates = 0;
    }
}

/**
 * @brief Accumulate fractional frame progress and check for frame advance
 * @return TRUE if accumulated enough time to show next frame
 * @note Uses sub-frame accumulation for smooth variable-speed animation
 */
static BOOL AdvanceInternalFramePosition(void) {
    int frameCount = g_isPreviewActive ? g_previewCount : g_trayIconCount;
    if (frameCount <= 0) return FALSE;
    
    UINT baseDelay;
    if (g_isPreviewActive) {
        baseDelay = g_isPreviewAnimated ? g_previewFrameDelaysMs[g_previewIndex] : g_trayInterval;
    } else {
        baseDelay = g_isAnimated ? g_frameDelaysMs[g_trayIconIndex] : g_trayInterval;
    }
    if (baseDelay == 0) baseDelay = g_trayInterval > 0 ? g_trayInterval : 150;
    
    UINT scaledDelay = ComputeScaledDelay(baseDelay);
    if (scaledDelay == 0) scaledDelay = 50;
    
    double frameAdvancement = (double)g_targetInternalInterval / (double)scaledDelay;
    g_internalFramePosition += frameAdvancement;
    
    if (g_internalFramePosition >= 1.0) {
        g_internalFramePosition -= 1.0;
        return TRUE;
    }
    
    return FALSE;
}

/**
 * @brief Request tray update from timer thread (thread-safe)
 * @note Sets flag and posts message; actual Shell API call in main thread
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
 * @brief Apply current frame to tray icon (MUST run in main UI thread)
 * @warning Calls Shell_NotifyIconW - not safe from worker threads
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
    
    int count = g_isPreviewActive ? g_previewCount : g_trayIconCount;
    if (count <= 0) {
        if (g_isPreviewActive) {
            g_isPreviewActive = FALSE;
            return;
        }
        if (_stricmp(g_animationName, "__cpu__") == 0 || _stricmp(g_animationName, "__mem__") == 0) {
            float cpu = 0.0f, mem = 0.0f;
            SystemMonitor_GetUsage(&cpu, &mem);
            int p = (_stricmp(g_animationName, "__cpu__") == 0) ? (int)(cpu + 0.5f) : (int)(mem + 0.5f);
            if (p < 0) p = 0; if (p > 100) p = 100;
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
        if (RecordFailedUpdate()) {
            FallbackToLogoIcon();
        }
        return;
    }
    
    if (g_isPreviewActive) {
        if (g_previewIndex >= g_previewCount) g_previewIndex = 0;
    } else {
        if (g_trayIconIndex >= g_trayIconCount) g_trayIconIndex = 0;
    }
    
    HICON hIcon = g_isPreviewActive ? g_previewIcons[g_previewIndex] : g_trayIcons[g_trayIconIndex];
    
    if (!hIcon) {
        WriteLog(LOG_LEVEL_WARNING, "Attempting to update with NULL icon at index %d", 
                 g_isPreviewActive ? g_previewIndex : g_trayIconIndex);
        
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
    } else {
        WriteLog(LOG_LEVEL_WARNING, "Shell_NotifyIconW failed to update tray icon");
        
        if (RecordFailedUpdate()) {
            FallbackToLogoIcon();
        }
        return;
    }
    
    AdaptiveFrameRateUpdate();
}

/**
 * @brief Multimedia timer callback at 10ms intervals (100Hz)
 * @warning Executes in worker thread - NO direct UI calls allowed
 * @note Advances frame logic at high rate, posts updates to UI thread at 50ms
 */
static void CALLBACK HighPrecisionTimerCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    (void)uTimerID; (void)uMsg; (void)dwUser; (void)dw1; (void)dw2;
    
    if ((_stricmp(g_animationName, "__cpu__") == 0 || _stricmp(g_animationName, "__mem__") == 0) && !g_isPreviewActive) {
        g_internalAccumulator += g_targetInternalInterval;
        if (g_internalAccumulator >= g_currentEffectiveInterval) {
            g_internalAccumulator = 0;
        }
        return;
    }
    
    if (g_criticalSectionInitialized) {
        EnterCriticalSection(&g_animCriticalSection);
    }
    
    BOOL shouldAdvanceFrame = AdvanceInternalFramePosition();
    
    if (shouldAdvanceFrame) {
        if (g_isPreviewActive) {
            if (g_previewCount > 0) {
                g_previewIndex = (g_previewIndex + 1) % g_previewCount;
            }
        } else {
            if (g_trayIconCount > 0) {
                g_trayIconIndex = (g_trayIconIndex + 1) % g_trayIconCount;
            }
        }
    }
    
    g_internalAccumulator += g_targetInternalInterval;
    
    if (g_internalAccumulator >= g_currentEffectiveInterval) {
        g_internalAccumulator = 0;
        
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

/** @brief SetTimer fallback if multimedia timer unavailable */
static void CALLBACK FallbackTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)hwnd; (void)msg; (void)id; (void)time;
    HighPrecisionTimerCallback(0, 0, 0, 0, 0);
}

/**
 * @brief Initialize multimedia timer (timeSetEvent) at 10ms
 * @return TRUE on success, FALSE to use SetTimer fallback
 * @note Requests 1ms system timer resolution for best accuracy
 */
static BOOL InitializeAnimationTimer(void) {
    if (!g_criticalSectionInitialized) {
        InitializeCriticalSection(&g_animCriticalSection);
        g_criticalSectionInitialized = TRUE;
    }
    
    MMRESULT mmRes = timeBeginPeriod(1);
    if (mmRes != TIMERR_NOERROR) {
        mmRes = timeBeginPeriod(2);
        if (mmRes != TIMERR_NOERROR) {
            g_useHighPrecisionTimer = FALSE;
            return FALSE;
        }
    }
    
    g_mmTimerId = timeSetEvent(
        g_targetInternalInterval,
        1,
        HighPrecisionTimerCallback,
        0,
        TIME_PERIODIC | TIME_KILL_SYNCHRONOUS
    );
    
    if (g_mmTimerId == 0) {
        timeEndPeriod(1);
        g_useHighPrecisionTimer = FALSE;
        return FALSE;
    }
    
    g_useHighPrecisionTimer = TRUE;
    return TRUE;
}

/** @brief Destroy timer and restore system timer resolution */
static void CleanupHighPrecisionTimer(void) {
    if (g_mmTimerId != 0) {
        timeKillEvent(g_mmTimerId);
        g_mmTimerId = 0;
    }
    
    if (g_useHighPrecisionTimer) {
        timeEndPeriod(1);
        g_useHighPrecisionTimer = FALSE;
    }
    
    if (g_criticalSectionInitialized) {
        DeleteCriticalSection(&g_animCriticalSection);
        g_criticalSectionInitialized = FALSE;
    }
}

/**
 * @brief Construct full path to animation resource
 * @param name Animation folder or file name
 * @param path Output buffer for full path
 * @param size Output buffer size
 */
static void BuildAnimationFolder(const char* name, char* path, size_t size) {
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    size_t len = strlen(base);
    if (len > 0 && (base[len-1] == '/' || base[len-1] == '\\')) {
        snprintf(path, size, "%s%s", base, name);
    } else {
        snprintf(path, size, "%s\\%s", base, name);
    }
}

/**
 * @brief Destroy icon set and optionally reset canvas dimensions
 * @param resetCanvasSize TRUE for main tray (affects global size), FALSE for preview
 */
static void FreeIconSet(HICON icons[], int* count, int* index, BOOL* isAnimated, BYTE** canvas, BOOL resetCanvasSize) {
    for (int i = 0; i < *count; ++i) {
        if (icons[i]) {
            DestroyIcon(icons[i]);
            icons[i] = NULL;
        }
    }
    *count = 0;
    *index = 0;
    *isAnimated = FALSE;
    
    if (*canvas) {
        free(*canvas);
        *canvas = NULL;
    }
    if (resetCanvasSize) {
        g_animCanvasWidth = 0;
        g_animCanvasHeight = 0;
    }
}

static BOOL EndsWithIgnoreCase(const char* str, const char* suffix) {
    if (!str || !suffix) return FALSE;
    size_t ls = strlen(str), lsuf = strlen(suffix);
    if (lsuf > ls) return FALSE;
    return _stricmp(str + (ls - lsuf), suffix) == 0;
}

static BOOL IsGifSelection(const char* name) {
    return name && EndsWithIgnoreCase(name, ".gif");
}

static BOOL IsWebPSelection(const char* name) {
    return name && EndsWithIgnoreCase(name, ".webp");
}

static BOOL IsStaticImageSelection(const char* name) {
    if (!name) return FALSE;
    return EndsWithIgnoreCase(name, ".ico") ||
           EndsWithIgnoreCase(name, ".png") ||
           EndsWithIgnoreCase(name, ".bmp") ||
           EndsWithIgnoreCase(name, ".jpg") ||
           EndsWithIgnoreCase(name, ".jpeg") ||
           EndsWithIgnoreCase(name, ".tif") ||
           EndsWithIgnoreCase(name, ".tiff");
}

/**
 * @brief Composite pixel onto canvas using "source over" alpha blending
 * @note PBGRA format: pixel[0]=B, pixel[1]=G, pixel[2]=R, pixel[3]=A
 */
static void BlendPixel(BYTE* canvas, UINT canvasStride, UINT x, UINT y, BYTE r, BYTE g, BYTE b, BYTE a) {
    if (a == 0) return;
    
    BYTE* pixel = canvas + (y * canvasStride) + (x * 4);
    if (a == 255) {
        pixel[0] = b;
        pixel[1] = g;
        pixel[2] = r;
        pixel[3] = a;
    } else {
        UINT srcAlpha = a;
        UINT dstAlpha = pixel[3];
        
        if (dstAlpha == 0) {
            pixel[0] = b;
            pixel[1] = g;
            pixel[2] = r;
            pixel[3] = a;
        } else {
            UINT invSrcAlpha = 255 - srcAlpha;
            UINT newAlpha = srcAlpha + (dstAlpha * invSrcAlpha) / 255;
            
            if (newAlpha > 0) {
                pixel[0] = (BYTE)((b * srcAlpha + pixel[0] * dstAlpha * invSrcAlpha / 255) / newAlpha);
                pixel[1] = (BYTE)((g * srcAlpha + pixel[1] * dstAlpha * invSrcAlpha / 255) / newAlpha);
                pixel[2] = (BYTE)((r * srcAlpha + pixel[2] * dstAlpha * invSrcAlpha / 255) / newAlpha);
                pixel[3] = (BYTE)newAlpha;
            }
        }
    }
}

/**
 * @brief Fill canvas region with solid color
 * @note Used for GIF disposal mode 2 (restore background)
 */
static void ClearCanvasRect(BYTE* canvas, UINT canvasWidth, UINT canvasHeight, 
                           UINT left, UINT top, UINT width, UINT height, 
                           BYTE bgR, BYTE bgG, BYTE bgB, BYTE bgA) {
    UINT canvasStride = canvasWidth * 4;
    UINT right = left + width;
    UINT bottom = top + height;
    
    if (left >= canvasWidth || top >= canvasHeight) return;
    if (right > canvasWidth) right = canvasWidth;
    if (bottom > canvasHeight) bottom = canvasHeight;
    
    for (UINT y = top; y < bottom; y++) {
        for (UINT x = left; x < right; x++) {
            BYTE* pixel = canvas + (y * canvasStride) + (x * 4);
            pixel[0] = bgB;
            pixel[1] = bgG;
            pixel[2] = bgR;
            pixel[3] = bgA;
        }
    }
}

/**
 * @brief Convert WIC bitmap to HICON with aspect-preserving scaling
 * @param pFactory WIC factory instance
 * @param source WIC bitmap source (any format)
 * @param cx Target icon width (usually SM_CXSMICON)
 * @param cy Target icon height (usually SM_CYSMICON)
 * @return HICON or NULL on failure
 * @note Centers image if aspect ratio doesn't match
 */
static HICON CreateIconFromWICSource(IWICImagingFactory* pFactory,
                                     IWICBitmapSource* source,
                                     int cx,
                                     int cy) {
    if (!pFactory || !source || cx <= 0 || cy <= 0) return NULL;

    HICON hIcon = NULL;

    IWICBitmapScaler* pScaler = NULL;
    HRESULT hr = pFactory->lpVtbl->CreateBitmapScaler(pFactory, &pScaler);
    if (SUCCEEDED(hr) && pScaler) {
        UINT srcW = 0, srcH = 0;
        if (FAILED(source->lpVtbl->GetSize(source, &srcW, &srcH)) || srcW == 0 || srcH == 0) {
            srcW = (UINT)cx;
            srcH = (UINT)cy;
        }

        double scaleX = (double)cx / (double)srcW;
        double scaleY = (double)cy / (double)srcH;
        double scale = scaleX < scaleY ? scaleX : scaleY;
        if (scale <= 0.0) scale = 1.0;

        UINT dstW = (UINT)((double)srcW * scale + 0.5);
        UINT dstH = (UINT)((double)srcH * scale + 0.5);
        if (dstW == 0) dstW = 1;
        if (dstH == 0) dstH = 1;

        hr = pScaler->lpVtbl->Initialize(pScaler, source, dstW, dstH, WICBitmapInterpolationModeFant);
        if (SUCCEEDED(hr)) {
            IWICFormatConverter* pConverter = NULL;
            hr = pFactory->lpVtbl->CreateFormatConverter(pFactory, &pConverter);
            if (SUCCEEDED(hr) && pConverter) {
                hr = pConverter->lpVtbl->Initialize(pConverter,
                                                    (IWICBitmapSource*)pScaler,
                                                    &GUID_WICPixelFormat32bppPBGRA,
                                                    WICBitmapDitherTypeNone,
                                                    NULL,
                                                    0.0,
                                                    WICBitmapPaletteTypeCustom);
                if (SUCCEEDED(hr)) {
                    BITMAPINFO bi; ZeroMemory(&bi, sizeof(bi));
                    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bi.bmiHeader.biWidth = cx;
                    bi.bmiHeader.biHeight = -cy;
                    bi.bmiHeader.biPlanes = 1;
                    bi.bmiHeader.biBitCount = 32;
                    bi.bmiHeader.biCompression = BI_RGB;
                    VOID* pvBits = NULL;
                    HBITMAP hbmColor = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
                    if (hbmColor && pvBits) {
                        ZeroMemory(pvBits, (SIZE_T)(cy * (cx * 4)));

                        UINT scaledStride = dstW * 4;
                        UINT scaledSize = dstH * scaledStride;
                        BYTE* tmp = (BYTE*)MemoryPool_Alloc(scaledSize);
                        if (tmp) {
                            if (SUCCEEDED(pConverter->lpVtbl->CopyPixels(pConverter, NULL, scaledStride, scaledSize, tmp))) {
                                int xoff = (cx - (int)dstW) / 2;
                                int yoff = (cy - (int)dstH) / 2;
                                if (xoff < 0) xoff = 0;
                                if (yoff < 0) yoff = 0;
                                for (UINT y = 0; y < dstH; ++y) {
                                    BYTE* dstRow = (BYTE*)pvBits + ((yoff + (int)y) * cx + xoff) * 4;
                                    BYTE* srcRow = tmp + y * scaledStride;
                                    memcpy(dstRow, srcRow, scaledStride);
                                }
                            }
                            MemoryPool_Free(tmp);
                        }

                        ICONINFO ii; ZeroMemory(&ii, sizeof(ii));
                        ii.fIcon = TRUE;
                        ii.hbmColor = hbmColor;

                        ii.hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);
                        if (ii.hbmMask) {
                            HDC hdcMem = GetDC(NULL);
                            HDC hdcColor = CreateCompatibleDC(hdcMem);
                            HDC hdcMask = CreateCompatibleDC(hdcMem);
                            SelectObject(hdcColor, hbmColor);
                            SelectObject(hdcMask, ii.hbmMask);

                            BitBlt(hdcMask, 0, 0, cx, cy, NULL, 0, 0, BLACKNESS);
                            SetBkColor(hdcColor, RGB(0,0,0));
                            BitBlt(hdcMask, 0, 0, cx, cy, hdcColor, 0, 0, SRCCOPY);
                            BitBlt(hdcMask, 0, 0, cx, cy, NULL, 0, 0, DSTINVERT);

                            DeleteDC(hdcColor);
                            DeleteDC(hdcMask);
                            ReleaseDC(NULL, hdcMem);
                        }

                        hIcon = CreateIconIndirect(&ii);
                        if (ii.hbmMask) DeleteObject(ii.hbmMask);
                        DeleteObject(hbmColor);
                    }
                }
                pConverter->lpVtbl->Release(pConverter);
            }
        }
        pScaler->lpVtbl->Release(pScaler);
    }

    return hIcon;
}

/**
 * @brief Convert PBGRA pixel buffer to scaled HICON
 * @param canvasPixels 32bpp PBGRA pixel array
 * @return HICON or NULL on failure
 */
static HICON CreateIconFromPBGRA(IWICImagingFactory* pFactory,
                                 const BYTE* canvasPixels,
                                 UINT canvasWidth,
                                 UINT canvasHeight,
                                 int cx,
                                 int cy) {
    if (!pFactory || !canvasPixels || canvasWidth == 0 || canvasHeight == 0 || cx <= 0 || cy <= 0) return NULL;

    HICON hIcon = NULL;
    IWICBitmap* pBitmap = NULL;

    const UINT stride = canvasWidth * 4;
    const UINT size = canvasHeight * stride;

    HRESULT hr = pFactory->lpVtbl->CreateBitmapFromMemory(pFactory, canvasWidth, canvasHeight, &GUID_WICPixelFormat32bppPBGRA, stride, size, (BYTE*)canvasPixels, &pBitmap);
    if (SUCCEEDED(hr) && pBitmap) {
        hIcon = CreateIconFromWICSource(pFactory, (IWICBitmapSource*)pBitmap, cx, cy);
        pBitmap->lpVtbl->Release(pBitmap);
    }

    return hIcon;
}

/**
 * @brief Decode GIF/WebP animation and pre-render all frames to HICON
 * @param utf8Path Path to .gif or .webp file
 * @param target Output arrays (tray or preview)
 * 
 * @note Performance strategy: pre-composite ALL frames at load time
 * - Trades memory for speed (no runtime blending/decoding)
 * - Uses memory pool to reduce malloc overhead in frame loop
 * - Ideal for tray icons where latency matters more than memory
 */
static void LoadAnimatedImage(const char* utf8Path, DecodeTarget* target) {
    if (!utf8Path || !*utf8Path) return;

    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH);

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);

    HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IWICImagingFactory* pFactory = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&pFactory);
    if (FAILED(hr) || !pFactory) {
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return;
    }

    IWICBitmapDecoder* pDecoder = NULL;
    hr = pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, wPath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr) || !pDecoder) {
        if (pFactory) pFactory->lpVtbl->Release(pFactory);
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return;
    }

    GUID containerFormat;
    BOOL isGif = FALSE;
    if (SUCCEEDED(pDecoder->lpVtbl->GetContainerFormat(pDecoder, &containerFormat))) {
        if (IsEqualGUID(&containerFormat, &GUID_ContainerFormatGif)) {
            isGif = TRUE;
        }
    }

    UINT canvasWidth = 0, canvasHeight = 0;

    if (isGif) {
        IWICMetadataQueryReader* pGlobalMeta = NULL;
        if (SUCCEEDED(pDecoder->lpVtbl->GetMetadataQueryReader(pDecoder, &pGlobalMeta)) && pGlobalMeta) {
            PROPVARIANT var;
            PropVariantInit(&var);
            if (SUCCEEDED(pGlobalMeta->lpVtbl->GetMetadataByName(pGlobalMeta, L"/logscrdesc/Width", &var))) {
                if (var.vt == VT_UI2) canvasWidth = var.uiVal;
                else if (var.vt == VT_I2) canvasWidth = (UINT)var.iVal;
            }
            PropVariantClear(&var);

            PropVariantInit(&var);
            if (SUCCEEDED(pGlobalMeta->lpVtbl->GetMetadataByName(pGlobalMeta, L"/logscrdesc/Height", &var))) {
                if (var.vt == VT_UI2) canvasHeight = var.uiVal;
                else if (var.vt == VT_I2) canvasHeight = (UINT)var.iVal;
            }
            PropVariantClear(&var);
            pGlobalMeta->lpVtbl->Release(pGlobalMeta);
        }
    }


    if (canvasWidth == 0 || canvasHeight == 0) {
        IWICBitmapFrameDecode* pFirstFrame = NULL;
        if (SUCCEEDED(pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFirstFrame)) && pFirstFrame) {
            pFirstFrame->lpVtbl->GetSize(pFirstFrame, &canvasWidth, &canvasHeight);
            pFirstFrame->lpVtbl->Release(pFirstFrame);
        }
    }

    if (canvasWidth == 0 || canvasHeight == 0) {
        pDecoder->lpVtbl->Release(pDecoder);
        if (pFactory) pFactory->lpVtbl->Release(pFactory);
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return;
    }

    g_animCanvasWidth = canvasWidth;
    g_animCanvasHeight = canvasHeight;

    UINT canvasStride = canvasWidth * 4;
    UINT canvasSize = canvasHeight * canvasStride;
    *(target->canvas) = (BYTE*)malloc(canvasSize);
    if (!*(target->canvas)) {
        pDecoder->lpVtbl->Release(pDecoder);
        if (pFactory) pFactory->lpVtbl->Release(pFactory);
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return;
    }
    memset(*(target->canvas), 0, canvasSize);

    UINT frameCount = 0;
    if (SUCCEEDED(pDecoder->lpVtbl->GetFrameCount(pDecoder, &frameCount))) {
        UINT prevDisposal = 0;
        UINT prevLeft = 0, prevTop = 0, prevWidth = 0, prevHeight = 0;

        for (UINT i = 0; i < frameCount && *(target->count) < MAX_TRAY_FRAMES; ++i) {
            IWICBitmapFrameDecode* pFrame = NULL;
            if (FAILED(pDecoder->lpVtbl->GetFrame(pDecoder, i, &pFrame)) || !pFrame) continue;

            if (isGif && i > 0) {
                if (prevDisposal == 2) {
                    ClearCanvasRect(*(target->canvas), canvasWidth, canvasHeight, prevLeft, prevTop, prevWidth, prevHeight, 0, 0, 0, 0);
                }
            } else if (!isGif) {
                 memset(*(target->canvas), 0, canvasSize);
            }

            UINT delayMs = 100;
            UINT disposal = 0;
            UINT frameLeft = 0, frameTop = 0, frameWidth = 0, frameHeight = 0;

            IWICMetadataQueryReader* pMeta = NULL;
            if (SUCCEEDED(pFrame->lpVtbl->GetMetadataQueryReader(pFrame, &pMeta)) && pMeta) {
                PROPVARIANT var;
                if (isGif) {
                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/grctlext/Delay", &var))) {
                        if (var.vt == VT_UI2 || var.vt == VT_I2) {
                            USHORT cs = (var.vt == VT_UI2) ? var.uiVal : (USHORT)var.iVal;
                            if (cs == 0) cs = 10;
                            delayMs = (UINT)cs * 10U;
                        }
                    }
                    PropVariantClear(&var);

                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/grctlext/Disposal", &var))) {
                        if (var.vt == VT_UI1) disposal = var.bVal;
                    }
                    PropVariantClear(&var);

                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/imgdesc/Left", &var))) {
                        if (var.vt == VT_UI2) frameLeft = var.uiVal;
                    }
                    PropVariantClear(&var);
                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/imgdesc/Top", &var))) {
                        if (var.vt == VT_UI2) frameTop = var.uiVal;
                    }
                    PropVariantClear(&var);
                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/imgdesc/Width", &var))) {
                        if (var.vt == VT_UI2) frameWidth = var.uiVal;
                    }
                        PropVariantClear(&var);
                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/imgdesc/Height", &var))) {
                        if (var.vt == VT_UI2) frameHeight = var.uiVal;
                    }
                    PropVariantClear(&var);
                } else {
                    PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/webp/delay", &var))) {
                        if (var.vt == VT_UI4) delayMs = var.ulVal;
                    }
                    PropVariantClear(&var);
                }
                pMeta->lpVtbl->Release(pMeta);
            }

            pFrame->lpVtbl->GetSize(pFrame, &frameWidth, &frameHeight);

            IWICFormatConverter* pConverter = NULL;
            if (SUCCEEDED(pFactory->lpVtbl->CreateFormatConverter(pFactory, &pConverter)) && pConverter) {
                if (SUCCEEDED(pConverter->lpVtbl->Initialize(pConverter, (IWICBitmapSource*)pFrame, &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom))) {
                    UINT frameStride = frameWidth * 4;
                    UINT frameBufferSize = frameHeight * frameStride;
                    BYTE* frameBuffer = (BYTE*)MemoryPool_Alloc(frameBufferSize);
                    if (frameBuffer) {
                        if (SUCCEEDED(pConverter->lpVtbl->CopyPixels(pConverter, NULL, frameStride, frameBufferSize, frameBuffer))) {
                            if (isGif) {
                                for (UINT y = 0; y < frameHeight; y++) {
                                    for (UINT x = 0; x < frameWidth; x++) {
                                        if (frameLeft + x < canvasWidth && frameTop + y < canvasHeight) {
                                            BYTE* srcPixel = frameBuffer + (y * frameStride) + (x * 4);
                                            BlendPixel(*(target->canvas), canvasStride, frameLeft + x, frameTop + y, srcPixel[2], srcPixel[1], srcPixel[0], srcPixel[3]);
                                        }
                                    }
                                }
                            } else {
                                if (frameWidth == canvasWidth && frameHeight == canvasHeight) {
                                    memcpy(*(target->canvas), frameBuffer, canvasSize);
                                } else {
                                    UINT offsetX = (canvasWidth > frameWidth) ? (canvasWidth - frameWidth) / 2 : 0;
                                    UINT offsetY = (canvasHeight > frameHeight) ? (canvasHeight - frameHeight) / 2 : 0;
                                    for (UINT y = 0; y < frameHeight && (offsetY + y) < canvasHeight; y++) {
                                        memcpy(*(target->canvas) + ((offsetY + y) * canvasStride) + (offsetX * 4), frameBuffer + (y * frameStride), frameStride);
                                    }
                                }
                            }
                        }
                        MemoryPool_Free(frameBuffer);
                    }
                }
                pConverter->lpVtbl->Release(pConverter);
            }
            
            HICON hIcon = CreateIconFromPBGRA(pFactory, *(target->canvas), canvasWidth, canvasHeight, cx, cy);
            if (hIcon) {
                target->icons[*(target->count)] = hIcon;
                target->delays[*(target->count)] = delayMs;
                (*(target->count))++;
            }

            if (isGif) {
                prevDisposal = disposal;
                prevLeft = frameLeft;
                prevTop = frameTop;
                prevWidth = frameWidth;
                prevHeight = frameHeight;
            }
            pFrame->lpVtbl->Release(pFrame);
        }
    }

    pDecoder->lpVtbl->Release(pDecoder);
    if (pFactory) pFactory->lpVtbl->Release(pFactory);
    if (SUCCEEDED(hrInit)) CoUninitialize();

    if (*(target->count) > 0) {
        *(target->isAnimatedFlag) = TRUE;
        *(target->index) = 0;
    }
}

/**
 * @brief Load sequential icon frames from folder (sorted naturally by number)
 * @param utf8Folder Full path to folder containing .ico/.png/.bmp/etc files
 * @param icons Output array for HICONs
 * @param count Output count of loaded icons
 */
static void LoadIconsFromFolder(const char* utf8Folder, HICON* icons, int* count) {
    wchar_t wFolder[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Folder, -1, wFolder, MAX_PATH);

    typedef struct { int hasNum; int num; wchar_t name[MAX_PATH]; wchar_t path[MAX_PATH]; } AnimFile;
    AnimFile files[MAX_TRAY_FRAMES];
    int fileCount = 0;

    void AddFilesWithPattern(const wchar_t* pattern) {
        WIN32_FIND_DATAW ffd;
        HANDLE hFind = FindFirstFileW(pattern, &ffd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                wchar_t* dot = wcsrchr(ffd.cFileName, L'.');
                if (!dot) continue;
                size_t nameLen = (size_t)(dot - ffd.cFileName);
                if (nameLen == 0 || nameLen >= MAX_PATH) continue;

                int hasNum = 0, numVal = 0;
                for (size_t i = 0; i < nameLen; ++i) {
                    if (iswdigit(ffd.cFileName[i])) {
                        hasNum = 1;
                        numVal = 0;
                        while (i < nameLen && iswdigit(ffd.cFileName[i])) {
                            numVal = numVal * 10 + (ffd.cFileName[i] - L'0');
                            i++;
                        }
                        break;
                    }
                }

                if (fileCount < MAX_TRAY_FRAMES) {
                    files[fileCount].hasNum = hasNum;
                    files[fileCount].num = numVal;
                    wcsncpy(files[fileCount].name, ffd.cFileName, nameLen);
                    files[fileCount].name[nameLen] = L'\0';
                    _snwprintf_s(files[fileCount].path, MAX_PATH, _TRUNCATE, L"%s\\%s", wFolder, ffd.cFileName);
                    fileCount++;
                }
            } while (FindNextFileW(hFind, &ffd));
            FindClose(hFind);
        }
    }

    const wchar_t* patterns[] = { L"\\*.ico", L"\\*.png", L"\\*.bmp", L"\\*.jpg", L"\\*.jpeg", L"\\*.webp", L"\\*.tif", L"\\*.tiff" };
    for (int i = 0; i < sizeof(patterns)/sizeof(patterns[0]); ++i) {
        wchar_t wSearch[MAX_PATH] = {0};
        _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s%s", wFolder, patterns[i]);
        AddFilesWithPattern(wSearch);
    }

    if (fileCount == 0) return;

    int cmpAnimFile(const void* a, const void* b) {
        const AnimFile* fa = (const AnimFile*)a;
        const AnimFile* fb = (const AnimFile*)b;
        if (fa->hasNum && fb->hasNum) {
            if (fa->num != fb->num) return fa->num < fb->num ? -1 : 1;
        }
        return _wcsicmp(fa->name, fb->name);
    }
    qsort(files, (size_t)fileCount, sizeof(AnimFile), cmpAnimFile);

    for (int i = 0; i < fileCount; ++i) {
        HICON hIcon = NULL;
        const wchar_t* ext = wcsrchr(files[i].path, L'.');
        if (ext && (_wcsicmp(ext, L".ico") == 0)) {
            hIcon = (HICON)LoadImageW(NULL, files[i].path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        } else {
            int cx = GetSystemMetrics(SM_CXSMICON);
            int cy = GetSystemMetrics(SM_CYSMICON);
            
            IWICImagingFactory* pFactory = NULL;
            HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
            if (SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&pFactory)) && pFactory) {
                IWICBitmapDecoder* pDecoder = NULL;
                if (SUCCEEDED(pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, files[i].path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder)) && pDecoder) {
                    IWICBitmapFrameDecode* pFrame = NULL;
                    if (SUCCEEDED(pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFrame)) && pFrame) {
                        hIcon = CreateIconFromWICSource(pFactory, (IWICBitmapSource*)pFrame, cx, cy);
                        pFrame->lpVtbl->Release(pFrame);
                    }
                    pDecoder->lpVtbl->Release(pDecoder);
                }
                pFactory->lpVtbl->Release(pFactory);
            }
            if (SUCCEEDED(hrInit)) CoUninitialize();
        }
        if (hIcon) {
            icons[(*count)++] = hIcon;
        }
    }
}

/**
 * @brief Load animation resources by name (logo, percent, GIF, folder, etc.)
 * @param name Animation identifier or path
 * @param isPreview TRUE to load into preview slot, FALSE for main tray
 */
static void LoadAnimationByName(const char* name, BOOL isPreview) {
    DecodeTarget target = GetDecodeTarget(isPreview);
    
    FreeIconSet(target.icons, target.count, target.index, target.isAnimatedFlag, target.canvas, !isPreview);

    if (!name || !*name) return;

    if (_stricmp(name, "__logo__") == 0) {
        HICON hIcon = LoadIconW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_CATIME));
        if (hIcon) {
            target.icons[(*(target.count))++] = hIcon;
        }
    } else if (_stricmp(name, "__cpu__") == 0 || _stricmp(name, "__mem__") == 0) {
        if (isPreview) {
            float cpu = 0.0f, mem = 0.0f;
            SystemMonitor_GetUsage(&cpu, &mem);
            int percent = (_stricmp(name, "__cpu__") == 0) ? (int)(cpu + 0.5f) : (int)(mem + 0.5f);
            if (percent < 0) percent = 0;
            if (percent > 100) percent = 100;
            
            HICON hIcon = CreatePercentIcon16(percent);
            if (hIcon) {
                target.icons[(*(target.count))++] = hIcon;
            }
        } else {
            *(target.count) = 0;
            *(target.index) = 0;
            *(target.isAnimatedFlag) = FALSE;
        }
    } else if (IsGifSelection(name) || IsWebPSelection(name)) {
        char filePath[MAX_PATH] = {0};
        BuildAnimationFolder(name, filePath, sizeof(filePath));
        LoadAnimatedImage(filePath, &target);
    } else if (IsStaticImageSelection(name)) {
        char filePath[MAX_PATH] = {0};
        BuildAnimationFolder(name, filePath, sizeof(filePath));

        int cx = GetSystemMetrics(SM_CXSMICON);
        int cy = GetSystemMetrics(SM_CYSMICON);
        HICON hIcon = NULL;

        wchar_t wPath[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wPath, MAX_PATH);
        const wchar_t* ext = wcsrchr(wPath, L'.');
        if (ext && _wcsicmp(ext, L".ico") == 0) {
            hIcon = (HICON)LoadImageW(NULL, wPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        } else {
            HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
            IWICImagingFactory* pFactory = NULL;
            if (SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&pFactory)) && pFactory) {
                IWICBitmapDecoder* pDecoder = NULL;
                if (SUCCEEDED(pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, wPath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder)) && pDecoder) {
                    IWICBitmapFrameDecode* pFrame = NULL;
                    if (SUCCEEDED(pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFrame)) && pFrame) {
                        hIcon = CreateIconFromWICSource(pFactory, (IWICBitmapSource*)pFrame, cx, cy);
                        pFrame->lpVtbl->Release(pFrame);
                    }
                    pDecoder->lpVtbl->Release(pDecoder);
                }
                pFactory->lpVtbl->Release(pFactory);
            }
            if (SUCCEEDED(hrInit)) CoUninitialize();
        }

        if (hIcon) {
            target.icons[(*(target.count))++] = hIcon;
            *(target.isAnimatedFlag) = FALSE;
        }
    } else {
        char folder[MAX_PATH] = {0};
        BuildAnimationFolder(name, folder, sizeof(folder));
        LoadIconsFromFolder(folder, target.icons, target.count);
    }
}

static void LoadTrayIcons(void) {
    LoadAnimationByName(g_animationName, FALSE);
}

/**
 * @brief Initialize animation system and start timer
 * @param hwnd Main window handle for message posting
 * @param intervalMs Base interval for folder animations (default 150ms)
 */
void StartTrayAnimation(HWND hwnd, UINT intervalMs) {
    g_trayHwnd = hwnd;
    g_trayInterval = intervalMs > 0 ? intervalMs : 150;
    {
        char config_path[MAX_PATH] = {0};
        GetConfigPath(config_path, sizeof(config_path));
        int folderMs = ReadIniInt("Animation", "ANIMATION_FOLDER_INTERVAL_MS", (int)g_trayInterval, config_path);
        if (folderMs <= 0) folderMs = 150;
        g_trayInterval = (UINT)folderMs;
    }
    g_isPreviewActive = FALSE;
    g_previewCount = 0;
    g_previewIndex = 0;
    
    g_internalAccumulator = 0;
    g_internalFramePosition = 0.0;
    g_currentEffectiveInterval = TRAY_UPDATE_INTERVAL_MS;
    g_lastTrayUpdateTime = 0;
    g_consecutiveLateUpdates = 0;
    g_targetInternalInterval = INTERNAL_TICK_INTERVAL_MS;


    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    char nameBuf[MAX_PATH] = {0};
    ReadIniString("Animation", "ANIMATION_PATH", "__logo__", nameBuf, sizeof(nameBuf), config_path);
    NormalizeAnimConfigValue(nameBuf);
    if (nameBuf[0] != '\0') {
        const char* prefix = "%LOCALAPPDATA%\\Catime\\resources\\animations\\";
        if (_stricmp(nameBuf, "__logo__") == 0) {
            strncpy(g_animationName, "__logo__", sizeof(g_animationName) - 1);
            g_animationName[sizeof(g_animationName) - 1] = '\0';
        } else if (_strnicmp(nameBuf, prefix, (int)strlen(prefix)) == 0) {
            const char* rel = nameBuf + strlen(prefix);
            if (*rel) {
                strncpy(g_animationName, rel, sizeof(g_animationName) - 1);
                g_animationName[sizeof(g_animationName) - 1] = '\0';
            }
        } else {
            strncpy(g_animationName, nameBuf, sizeof(g_animationName) - 1);
            g_animationName[sizeof(g_animationName) - 1] = '\0';
        }
    }

    LoadTrayIcons();

    if (g_trayIconCount > 0) {
        UpdateTrayIconToCurrentFrame();
    }
    
    if (!InitializeAnimationTimer()) {
        SetTimer(hwnd, TRAY_ANIM_TIMER_ID, g_targetInternalInterval, FallbackTimerProc);
    }
}

/**
 * @brief Stop animation system and free all resources
 * @param hwnd Window handle (for KillTimer)
 */
void StopTrayAnimation(HWND hwnd) {
    CleanupHighPrecisionTimer();
    KillTimer(hwnd, TRAY_ANIM_TIMER_ID);
    
    FreeIconSet(g_trayIcons, &g_trayIconCount, &g_trayIconIndex, &g_isAnimated, &g_animCanvas, TRUE);
    FreeIconSet(g_previewIcons, &g_previewCount, &g_previewIndex, &g_isPreviewAnimated, &g_previewAnimCanvas, FALSE);
    
    MemoryPool_Cleanup();
    
    g_internalAccumulator = 0;
    g_internalFramePosition = 0.0;
    g_currentEffectiveInterval = TRAY_UPDATE_INTERVAL_MS;
    g_lastTrayUpdateTime = 0;
    g_consecutiveLateUpdates = 0;
    
    g_consecutiveUpdateFailures = 0;
    g_lastSuccessfulUpdateTime = 0;
    
    g_trayHwnd = NULL;
}

/** @brief Get current animation name/path */
const char* GetCurrentAnimationName(void) {
    return g_animationName;
}

/**
 * @brief Switch to new animation and persist to config
 * @param name Animation name/path (e.g., "__logo__", "cat.gif", "spinner")
 * @return TRUE on success, FALSE if invalid
 * @note Performs seamless preview promotion if switching to current preview
 */
BOOL SetCurrentAnimationName(const char* name) {
    if (!name || !*name) return FALSE;

    char folder[MAX_PATH] = {0};
    if (_stricmp(name, "__logo__") == 0) {
        strncpy(g_animationName, name, sizeof(g_animationName) - 1);
        g_animationName[sizeof(g_animationName) - 1] = '\0';
        char config_path[MAX_PATH] = {0};
        GetConfigPath(config_path, sizeof(config_path));
        WriteIniString("Animation", "ANIMATION_PATH", "__logo__", config_path);
        LoadTrayIcons();
        g_trayIconIndex = 0;
        g_internalFramePosition = 0.0;
        if (g_trayHwnd && g_trayIconCount > 0) {
            UpdateTrayIconToCurrentFrame();
        }
        return TRUE;
    }
    if (_stricmp(name, "__cpu__") == 0 || _stricmp(name, "__mem__") == 0) {
        strncpy(g_animationName, name, sizeof(g_animationName) - 1);
        g_animationName[sizeof(g_animationName) - 1] = '\0';
        char config_path[MAX_PATH] = {0};
        GetConfigPath(config_path, sizeof(config_path));
        WriteIniString("Animation", "ANIMATION_PATH", name, config_path);

        LoadTrayIcons();
        g_trayIconIndex = 0;
        g_internalFramePosition = 0.0;
        if (g_isPreviewActive) {
            g_isPreviewActive = FALSE;
            FreeIconSet(g_previewIcons, &g_previewCount, &g_previewIndex, &g_isPreviewAnimated, &g_previewAnimCanvas, FALSE);
        }
        if (g_trayHwnd) {
            UpdateTrayIconToCurrentFrame();
        }
        return TRUE;
    }
    BuildAnimationFolder(name, folder, sizeof(folder));
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, folder, -1, wPath, MAX_PATH);

    DWORD attrs = GetFileAttributesW(wPath);
    BOOL valid = FALSE;
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            wchar_t wSearch[MAX_PATH] = {0};
            _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", wPath);
            WIN32_FIND_DATAW ffd; HANDLE hFind = FindFirstFileW(wSearch, &ffd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                    wchar_t* ext = wcsrchr(ffd.cFileName, L'.');
                    if (ext && (_wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0 || _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 || _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0 || _wcsicmp(ext, L".tif") == 0 || _wcsicmp(ext, L".tiff") == 0)) {
                        valid = TRUE; break;
                    }
                } while (FindNextFileW(hFind, &ffd));
                FindClose(hFind);
            }
        } else if (IsGifSelection(name)) {
            valid = TRUE; /** a valid single GIF file */
        } else if (IsWebPSelection(name)) {
            valid = TRUE; /** a valid single WebP file */
        } else if (IsStaticImageSelection(name)) {
            valid = TRUE; /** a valid single static image file */
        }
    }
    if (!valid) return FALSE;

    if (g_isPreviewActive && g_previewAnimationName[0] != '\0' && 
        _stricmp(g_previewAnimationName, name) == 0 && g_previewCount > 0) {
        
        if (g_criticalSectionInitialized) {
            EnterCriticalSection(&g_animCriticalSection);
        }
        
        FreeIconSet(g_trayIcons, &g_trayIconCount, &g_trayIconIndex, &g_isAnimated, &g_animCanvas, TRUE);
        

        for (int i = 0; i < g_previewCount; i++) {
            g_trayIcons[i] = g_previewIcons[i];
            g_previewIcons[i] = NULL;
            g_frameDelaysMs[i] = g_previewFrameDelaysMs[i];
        }
        g_trayIconCount = g_previewCount;
        g_trayIconIndex = g_previewIndex;
        g_isAnimated = g_isPreviewAnimated;
        
        if (g_previewAnimCanvas) {
            free(g_previewAnimCanvas);
            g_previewAnimCanvas = NULL;
        }
        
        g_previewCount = 0;
        g_previewIndex = 0;
        g_isPreviewAnimated = FALSE;
        g_isPreviewActive = FALSE;
        g_previewAnimationName[0] = '\0';
        
        if (g_criticalSectionInitialized) {
            LeaveCriticalSection(&g_animCriticalSection);
        }
        
        strncpy(g_animationName, name, sizeof(g_animationName) - 1);
        g_animationName[sizeof(g_animationName) - 1] = '\0';
        
        char config_path[MAX_PATH] = {0};
        GetConfigPath(config_path, sizeof(config_path));
        char animPath[MAX_PATH];
        snprintf(animPath, sizeof(animPath), "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", g_animationName);
        WriteIniString("Animation", "ANIMATION_PATH", animPath, config_path);
        
        if (g_trayHwnd) {
            UpdateTrayIconToCurrentFrame();
        }
        
        return TRUE;
    }


    strncpy(g_animationName, name, sizeof(g_animationName) - 1);
    g_animationName[sizeof(g_animationName) - 1] = '\0';

    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    char animPath[MAX_PATH];
    snprintf(animPath, sizeof(animPath), "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", g_animationName);
    WriteIniString("Animation", "ANIMATION_PATH", animPath, config_path);

    LoadTrayIcons();
    g_trayIconIndex = 0;
    g_internalFramePosition = 0.0;
    if (g_isPreviewActive) {
        g_isPreviewActive = FALSE;
        g_previewAnimationName[0] = '\0';
        FreeIconSet(g_previewIcons, &g_previewCount, &g_previewIndex, &g_isPreviewAnimated, &g_previewAnimCanvas, FALSE);
    }
    if (g_trayHwnd) {
        UpdateTrayIconToCurrentFrame();
    }
    return TRUE;
}

/**
 * @brief Load animation into preview slot without persisting
 * @param name Animation name/path to preview
 * @note Preview can be promoted to main via SetCurrentAnimationName (seamless)
 */
void StartAnimationPreview(const char* name) {
    if (!name || !*name) return;

    strncpy(g_previewAnimationName, name, sizeof(g_previewAnimationName) - 1);
    g_previewAnimationName[sizeof(g_previewAnimationName) - 1] = '\0';

    LoadAnimationByName(name, TRUE);

    if (g_previewCount > 0) {
        g_isPreviewActive = TRUE;
        g_previewIndex = 0;
        g_internalFramePosition = 0.0;
        
        UpdateTrayIconToCurrentFrame();
    } else {
        WriteLog(LOG_LEVEL_WARNING, "Animation preview failed to load: '%s'", name);
        g_previewAnimationName[0] = '\0';
    }
}

/** @brief End preview and restore main animation */
void CancelAnimationPreview(void) {
    if (!g_isPreviewActive) return;
    g_isPreviewActive = FALSE;
    g_previewAnimationName[0] = '\0';
    FreeIconSet(g_previewIcons, &g_previewCount, &g_previewIndex, &g_isPreviewAnimated, &g_previewAnimCanvas, FALSE);
    
    g_internalFramePosition = 0.0;
    
    UpdateTrayIconToCurrentFrame();
}

/**
 * @brief Load animation from config before UI initialization
 * @note Called early to have first frame ready for tray icon creation
 */
void PreloadAnimationFromConfig(void) {
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    char nameBuf[MAX_PATH] = {0};
    ReadIniString("Animation", "ANIMATION_PATH", "__logo__", nameBuf, sizeof(nameBuf), config_path);
    NormalizeAnimConfigValue(nameBuf);
    if (nameBuf[0] != '\0') {
        const char* prefix = "%LOCALAPPDATA%\\Catime\\resources\\animations\\";
        if (_stricmp(nameBuf, "__logo__") == 0) {
            strncpy(g_animationName, "__logo__", sizeof(g_animationName) - 1);
            g_animationName[sizeof(g_animationName) - 1] = '\0';
        } else if (_strnicmp(nameBuf, prefix, (int)strlen(prefix)) == 0) {
            const char* rel = nameBuf + strlen(prefix);
            if (*rel) {
                strncpy(g_animationName, rel, sizeof(g_animationName) - 1);
                g_animationName[sizeof(g_animationName) - 1] = '\0';
            }
        } else {
            strncpy(g_animationName, nameBuf, sizeof(g_animationName) - 1);
            g_animationName[sizeof(g_animationName) - 1] = '\0';
        }
    }
    LoadTrayIcons();
}

/**
 * @brief Get first frame for initial tray icon setup
 * @return First animation frame or NULL for percent icons
 */
HICON GetInitialAnimationHicon(void) {
    if (_stricmp(g_animationName, "__cpu__") == 0 || _stricmp(g_animationName, "__mem__") == 0) {
        return NULL;
    }
    if (g_trayIconCount > 0) {
        return g_trayIcons[0];
    }
    if (_stricmp(g_animationName, "__logo__") == 0) {
        return LoadIconW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_CATIME));
    }
    return NULL;
}

/**
 * @brief Apply animation path from config watcher without writing back
 * @param value Full animation path from config file
 * @note Skips reload if already displaying requested animation
 */
void ApplyAnimationPathValueNoPersist(const char* value) {
    if (!value || !*value) return;
    const char* prefix = "%LOCALAPPDATA%\\Catime\\resources\\animations\\";
    char name[MAX_PATH] = {0};
    if (_stricmp(value, "__logo__") == 0) {
        strncpy(name, "__logo__", sizeof(name) - 1);
    } else if (_strnicmp(value, prefix, (int)strlen(prefix)) == 0) {
        const char* rel = value + strlen(prefix);
        strncpy(name, rel, sizeof(name) - 1);
    } else {
        strncpy(name, value, sizeof(name) - 1);
    }
    if (name[0] == '\0') return;

    if (_stricmp(g_animationName, name) == 0) {
        return;
    }

    strncpy(g_animationName, name, sizeof(g_animationName) - 1);
    g_animationName[sizeof(g_animationName) - 1] = '\0';

    LoadTrayIcons();
    g_trayIconIndex = 0;
    g_internalFramePosition = 0.0;
    
    if (g_isPreviewActive) {
        g_isPreviewActive = FALSE;
        g_previewAnimationName[0] = '\0';
        FreeIconSet(g_previewIcons, &g_previewCount, &g_previewIndex, &g_isPreviewAnimated, &g_previewAnimCanvas, FALSE);
    }
    
    if (!g_trayHwnd) return;
    
    if (g_trayIconCount > 0) {
        UpdateTrayIconToCurrentFrame();
    }
}

/**
 * @brief Generate tray icon with numeric percentage text
 * @param percent Value 0-100+ to display
 * @return HICON or NULL on failure
 */
HICON CreatePercentIcon16(int percent) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16; if (cy <= 0) cy = 16;

    BITMAPINFO bi; ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = cx;
    bi.bmiHeader.biHeight = -cy;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    VOID* pvBits = NULL;
    HBITMAP hbmColor = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    if (!hbmColor || !pvBits) {
        if (hbmColor) DeleteObject(hbmColor);
        return NULL;
    }

    HBITMAP hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);
    if (!hbmMask) {
        DeleteObject(hbmColor);
        return NULL;
    }

    HDC hdc = GetDC(NULL);
    HDC mem = CreateCompatibleDC(hdc);
    HGDIOBJ old = SelectObject(mem, hbmColor);

    RECT rc = {0, 0, cx, cy};
    extern COLORREF GetPercentIconBgColor(void);
    HBRUSH bk = CreateSolidBrush(GetPercentIconBgColor());
    FillRect(mem, &rc, bk);
    DeleteObject(bk);

    SetBkMode(mem, TRANSPARENT);
    extern COLORREF GetPercentIconTextColor(void);
    SetTextColor(mem, GetPercentIconTextColor());

    HFONT hFont = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              ANTIALIASED_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldf = hFont ? (HFONT)SelectObject(mem, hFont) : NULL;

    wchar_t txt[4];
    if (percent > 999) percent = 999; if (percent < 0) percent = 0;
    if (percent >= 100) {
        wsprintfW(txt, L"%d", percent);
    } else {
        wsprintfW(txt, L"%d", percent);
    }

    SIZE sz = {0};
    GetTextExtentPoint32W(mem, txt, lstrlenW(txt), &sz);
    int x = (cx - sz.cx) / 2;
    int y = (cy - sz.cy) / 2;

    TextOutW(mem, x, y, txt, lstrlenW(txt));

    if (oldf) SelectObject(mem, oldf);
    if (hFont) DeleteObject(hFont);

    SelectObject(mem, old);
    ReleaseDC(NULL, hdc);
    DeleteDC(mem);

    ICONINFO ii; ZeroMemory(&ii, sizeof(ii));
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask = hbmMask;
    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hbmMask);
    DeleteObject(hbmColor);
    return hIcon;
}

/**
 * @brief Update percent icon (CPU/mem) from periodic updater
 * @note Only updates if current animation is __cpu__ or __mem__
 */
void TrayAnimation_UpdatePercentIconIfNeeded(void) {
    if (!g_trayHwnd) return;
    if (!IsWindow(g_trayHwnd)) return;
    if (!g_animationName[0]) return;
    if (_stricmp(g_animationName, "__cpu__") != 0 && _stricmp(g_animationName, "__mem__") != 0) return;
    
    if (g_isPreviewActive) return;

    float cpu = 0.0f, mem = 0.0f;
    SystemMonitor_GetUsage(&cpu, &mem);
    int p = (_stricmp(g_animationName, "__cpu__") == 0) ? (int)(cpu + 0.5f) : (int)(mem + 0.5f);
    if (p < 0) p = 0; if (p > 100) p = 100;

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

/** @brief No-op: speed scaling now computed dynamically in frame callback */
void TrayAnimation_RecomputeTimerDelay(void) {
    (void)0;
}

void TrayAnimation_SetMinIntervalMs(UINT ms) {
    g_userMinIntervalMs = ms;
    TrayAnimation_RecomputeTimerDelay();
}

/**
 * @brief Handle WM_TRAY_UPDATE_ICON message in main thread
 * @return TRUE if update was pending and handled
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

void TrayAnimation_SetBaseIntervalMs(UINT ms) {
    if (ms == 0) ms = 150;
    g_trayInterval = ms;
    TrayAnimation_RecomputeTimerDelay();
}

static void OpenAnimationsFolder(void) {
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, base, -1, wPath, MAX_PATH);
    ShellExecuteW(NULL, L"open", wPath, NULL, NULL, SW_SHOWNORMAL);
}

/**
 * @brief Check if folder is a leaf (no subfolders or animated images)
 * @return TRUE if folder contains only static image frames
 */
static BOOL IsAnimationLeafFolderW(const wchar_t* folderPathW) {
    wchar_t wSearch[MAX_PATH] = {0};
    _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
    
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wSearch, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return TRUE;

    BOOL hasSubItems = FALSE;
    do {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
        
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            hasSubItems = TRUE;
            break;
        }
        wchar_t* ext = wcsrchr(ffd.cFileName, L'.');
        if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0)) {
            hasSubItems = TRUE;
            break;
        }
    } while (FindNextFileW(hFind, &ffd));
    FindClose(hFind);
    
    return !hasSubItems;
}

/**
 * @brief Handle animation selection from context menu
 * @param hwnd Window handle
 * @param id Menu command ID
 * @return TRUE if handled, FALSE otherwise
 */
BOOL HandleAnimationMenuCommand(HWND hwnd, UINT id) {
    if (id == CLOCK_IDM_ANIMATIONS_OPEN_DIR) {
        OpenAnimationsFolder();
        return TRUE;
    }
    if (id == CLOCK_IDM_ANIMATIONS_USE_LOGO) {
        return SetCurrentAnimationName("__logo__");
    }
    if (id == CLOCK_IDM_ANIMATIONS_USE_CPU) {
        return SetCurrentAnimationName("__cpu__");
    }
    if (id == CLOCK_IDM_ANIMATIONS_USE_MEM) {
        return SetCurrentAnimationName("__mem__");
    }
    if (id >= CLOCK_IDM_ANIMATIONS_BASE && id < CLOCK_IDM_ANIMATIONS_BASE + 1000) {
        char animRootUtf8[MAX_PATH] = {0};
        GetAnimationsFolderPath(animRootUtf8, sizeof(animRootUtf8));
        wchar_t wRoot[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, animRootUtf8, -1, wRoot, MAX_PATH);

        UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;

        BOOL FindAnimationByIdRecursive(const wchar_t* folderPathW, const char* folderPathUtf8, UINT* nextIdPtr, UINT targetId, AnimationEntry* found_entry) {
            AnimationEntry* entries = (AnimationEntry*)malloc(sizeof(AnimationEntry) * MAX_TRAY_FRAMES);
            if (!entries) return FALSE;
            int entryCount = 0;

            wchar_t wSearch[MAX_PATH] = {0};
            _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
            
            WIN32_FIND_DATAW ffd;
            HANDLE hFind = FindFirstFileW(wSearch, &ffd);
            if (hFind == INVALID_HANDLE_VALUE) {
                free(entries);
                return FALSE;
            }

            do {
                if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
                if (entryCount >= MAX_TRAY_FRAMES) break;

                AnimationEntry* e = &entries[entryCount];
                e->is_dir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                wcsncpy(e->name, ffd.cFileName, MAX_PATH - 1);
                e->name[MAX_PATH - 1] = L'\0';

            char itemUtf8[MAX_PATH] = {0};
            WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, itemUtf8, MAX_PATH, NULL, NULL);
            if (folderPathUtf8 && folderPathUtf8[0] != '\0') {
                _snprintf_s(e->rel_path_utf8, MAX_PATH, _TRUNCATE, "%s\\%s", folderPathUtf8, itemUtf8);
            } else {
                _snprintf_s(e->rel_path_utf8, MAX_PATH, _TRUNCATE, "%s", itemUtf8);
            }
                
                if (e->is_dir) {
                    entryCount++;
                } else {
                    wchar_t* ext = wcsrchr(e->name, L'.');
                    if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0 ||
                                _wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0 ||
                                _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 ||
                                _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".tif") == 0 ||
                                _wcsicmp(ext, L".tiff") == 0)) {
                        entryCount++;
                    }
                }
            } while (FindNextFileW(hFind, &ffd));
            FindClose(hFind);

            if (entryCount == 0) {
                free(entries);
                return FALSE;
            }
            qsort(entries, entryCount, sizeof(AnimationEntry), CompareAnimationEntries);

            for (int i = 0; i < entryCount; ++i) {
                AnimationEntry* e = &entries[i];
                if (e->is_dir) {
                    wchar_t wSubFolderPath[MAX_PATH] = {0};
                    _snwprintf_s(wSubFolderPath, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPathW, e->name);

                    if (IsAnimationLeafFolderW(wSubFolderPath)) {
                        // This is a leaf folder, it's a single clickable item.
                        if (*nextIdPtr == targetId) {
                            *found_entry = *e;
                            free(entries);
                            return TRUE;
                        }
                        (*nextIdPtr)++;
                    } else {
                        // This is a branch folder (submenu), recurse without incrementing ID for the folder itself.
                        if (FindAnimationByIdRecursive(wSubFolderPath, e->rel_path_utf8, nextIdPtr, targetId, found_entry)) {
                            free(entries);
                            return TRUE;
                        }
                    }
                } else {
                    // This is a file (.gif/.webp), it's a single clickable item.
                    if (*nextIdPtr == targetId) {
                        *found_entry = *e;
                        free(entries);
                        return TRUE;
                    }
                    (*nextIdPtr)++;
                }
            }
            free(entries);
            return FALSE;
        }

        AnimationEntry rootEntries[MAX_TRAY_FRAMES];
        int rootEntryCount = 0;
        wchar_t wRootSearch[MAX_PATH] = {0};
        _snwprintf_s(wRootSearch, MAX_PATH, _TRUNCATE, L"%s\\*", wRoot);
        
        WIN32_FIND_DATAW ffd;
        HANDLE hFind = FindFirstFileW(wRootSearch, &ffd);
        if (hFind != INVALID_HANDLE_VALUE) {
             do {
                if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
                if (rootEntryCount >= MAX_TRAY_FRAMES) break;

                AnimationEntry* e = &rootEntries[rootEntryCount];
                e->is_dir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                wcsncpy(e->name, ffd.cFileName, MAX_PATH - 1);
                e->name[MAX_PATH - 1] = L'\0';
                WideCharToMultiByte(CP_UTF8, 0, e->name, -1, e->rel_path_utf8, MAX_PATH, NULL, NULL);

                if (e->is_dir) {
                    rootEntryCount++;
                } else {
                    wchar_t* ext = wcsrchr(e->name, L'.');
                    if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0 ||
                                _wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0 ||
                                _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 ||
                                _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".tif") == 0 ||
                                _wcsicmp(ext, L".tiff") == 0)) {
                        rootEntryCount++;
                    }
                }
            } while (FindNextFileW(hFind, &ffd));
            FindClose(hFind);
        }
        
        if (rootEntryCount > 0) {
            qsort(rootEntries, rootEntryCount, sizeof(AnimationEntry), CompareAnimationEntries);
            for (int i = 0; i < rootEntryCount; ++i) {
                AnimationEntry* e = &rootEntries[i];
                if (e->is_dir) {
                    wchar_t wFolderPath[MAX_PATH] = {0};
                    _snwprintf_s(wFolderPath, MAX_PATH, _TRUNCATE, L"%s\\%s", wRoot, e->name);

                    if (IsAnimationLeafFolderW(wFolderPath)) {
                        if (nextId == id) {
                            return SetCurrentAnimationName(e->rel_path_utf8);
                        }
                        nextId++;
                    } else {
                        AnimationEntry found_entry;
                        if (FindAnimationByIdRecursive(wFolderPath, e->rel_path_utf8, &nextId, id, &found_entry)) {
                            return SetCurrentAnimationName(found_entry.rel_path_utf8);
                        }
                    }
                } else {
                    if (nextId == id) {
                        return SetCurrentAnimationName(e->rel_path_utf8);
                    }
                    nextId++;
                }
            }
        }
        return FALSE;
    }
    return FALSE;
}

