/**
 * @file tray.c
 * @brief System tray icon management and tooltip updates
 */
#include <windows.h>
#include <shellapi.h>
#include <string.h>
#include "language.h"
#include "../resource/resource.h"
#include "tray/tray.h"
#include "timer/timer.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_animation_loader.h"
#include "tray/tray_animation_percent.h"
#include "system_monitor.h"
#include "config.h"
#include "tray/tray_events.h"
#include "tray/tray_menu_submenus.h"
#include "log.h"

#define TOOLTIP_UPDATE_INTERVAL_MS 1000
#define ICON_RECT_CACHE_TIMEOUT_MS 250  /* Cache tray icon position to reduce Shell API calls */
#define TRAY_OPACITY_SAVE_TIMER_ID 42423
#define TRAY_OPACITY_SAVE_DELAY_MS 400
#define TRAY_OPACITY_SAVE_MAX_RETRIES 3
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

/** @brief Global tray icon data for Shell_NotifyIcon */
NOTIFYICONDATAW nid;

/** @brief Taskbar recreation message ID */
UINT WM_TASKBARCREATED = 0;

/** @brief Mouse hook for tray icon wheel events (installed on-demand) */
static HHOOK g_mouseHook = NULL;
static HWND g_mainHwnd = NULL;
static HINSTANCE g_hInstance = NULL;
static BOOL g_trayIconActive = FALSE;

/** @brief Opacity tooltip mode flag */
BOOL g_showingOpacityTip = FALSE;

/** @brief Cached tray icon rectangle for performance */
static RECT g_cachedIconRect = {0};
static DWORD g_lastRectUpdateTime = 0;
static volatile LONG g_trayInteractionSuspended = FALSE;
static int g_pendingOpacityToSave = -1;
static int g_opacityRollbackValue = -1;
static int g_pendingOpacitySaveRetryCount = 0;
static wchar_t g_lastTrayTooltip[256] = {0};

extern void ReadPercentIconColorsConfig(void);

static void InitTrayIconInternal(HWND hwnd, HINSTANCE hInstance,
                                 BOOL preloadAnimation,
                                 BOOL startBackgroundWork,
                                 BOOL useAnimationInitialIcon);
static void RemoveTrayIconInternal(BOOL finalCleanup);
static void CALLBACK TrayOpacitySaveTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);

static BOOL IsValidTrayMainWindow(HWND hwnd) {
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

static HWND GetValidTrayMainWindow(void) {
    HWND hwnd = g_mainHwnd;
    if (!IsValidTrayMainWindow(hwnd)) {
        g_mainHwnd = NULL;
        return NULL;
    }
    return hwnd;
}

static BOOL IsTrayIconActiveForWindow(HWND hwnd) {
    return IsValidTrayMainWindow(hwnd) &&
           g_trayIconActive &&
           nid.hWnd == hwnd &&
           nid.uID == CLOCK_ID_TRAY_APP_ICON;
}

BOOL IsTrayIconActive(HWND hwnd) {
    return IsTrayIconActiveForWindow(hwnd);
}

/**
 * @brief Check if mouse is over tray icon (with caching)
 * @note Uses cached position to reduce Shell API calls.
 */
static BOOL IsMouseOverTrayIconCached(POINT pt) {
    if (!g_trayIconActive || !nid.hWnd) {
        SetRectEmpty(&g_cachedIconRect);
        return FALSE;
    }

    DWORD now = GetTickCount();
    
    /* Refresh cache if expired */
    if (now - g_lastRectUpdateTime > ICON_RECT_CACHE_TIMEOUT_MS) {
        NOTIFYICONIDENTIFIER iconId = {0};
        iconId.cbSize = sizeof(iconId);
        iconId.hWnd = nid.hWnd;
        iconId.uID = nid.uID;
        
        HRESULT hr = Shell_NotifyIconGetRect(&iconId, &g_cachedIconRect);
        if (FAILED(hr)) {
            /* If API fails, invalidate cache */
            SetRectEmpty(&g_cachedIconRect);
        }
        g_lastRectUpdateTime = now;
    }
    
    return PtInRect(&g_cachedIconRect, pt);
}

/**
 * @brief Mouse hook callback for tray icon wheel events
 * @note Only handles wheel events, installed on-demand when mouse enters tray icon
 */
static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (IsTrayInteractionSuspended()) {
        return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
    }

    if (nCode >= 0 && wParam == WM_MOUSEWHEEL) {
        MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
        
        /* Only process if mouse is over tray icon */
        if (IsMouseOverTrayIconCached(pMouseStruct->pt)) {
            int delta = GET_WHEEL_DELTA_WPARAM(pMouseStruct->mouseData);
            BOOL ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            HWND hwndMain = GetValidTrayMainWindow();
            if (hwndMain &&
                PostMessage(hwndMain, CLOCK_WM_TRAY_OPACITY_WHEEL,
                            (WPARAM)(delta > 0 ? 1 : -1), (LPARAM)ctrlPressed)) {
                return 1;
            }
        }
    }

    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

/** @brief Animation type categories */
typedef enum {
    ANIM_TYPE_CUSTOM,
    ANIM_TYPE_LOGO,
    ANIM_TYPE_CPU,
    ANIM_TYPE_MEMORY,
    ANIM_TYPE_BATTERY,
    ANIM_TYPE_CAPSLOCK,
    ANIM_TYPE_NONE
} AnimationType;

/** @brief Formatted bytes with unit */
typedef struct {
    double value;
    const wchar_t* unit;
} FormattedBytes;

/**
 * @brief Format bytes/sec with unit scaling
 * @return Formatted value (e.g., 1536 → {1.5, "KB/s"})
 */
static FormattedBytes FormatBytesPerSecond(double bytes) {
    FormattedBytes result = { bytes, L"B/s" };
    
    if (result.value >= 1024.0) { result.value /= 1024.0; result.unit = L"KB/s"; }
    if (result.value >= 1024.0) { result.value /= 1024.0; result.unit = L"MB/s"; }
    if (result.value >= 1024.0) { result.value /= 1024.0; result.unit = L"GB/s"; }
    
    return result;
}

/**
 * @brief Determine animation type from name
 * @return Type category (CUSTOM, LOGO, CPU, MEMORY)
 */
static AnimationType GetAnimationType(const char* animName) {
    if (!animName) return ANIM_TYPE_CUSTOM;
    if (_stricmp(animName, "__logo__") == 0) return ANIM_TYPE_LOGO;
    if (_stricmp(animName, "__cpu__") == 0) return ANIM_TYPE_CPU;
    if (_stricmp(animName, "__mem__") == 0) return ANIM_TYPE_MEMORY;
    if (_stricmp(animName, "__battery__") == 0) return ANIM_TYPE_BATTERY;
    if (_stricmp(animName, "__capslock__") == 0) return ANIM_TYPE_CAPSLOCK;
    if (_stricmp(animName, "__none__") == 0) return ANIM_TYPE_NONE;
    return ANIM_TYPE_CUSTOM;
}

/** @brief Check if type is percent-based (CPU/Memory/Battery) */
static inline BOOL IsPercentIcon(AnimationType type) {
    return type == ANIM_TYPE_CPU || type == ANIM_TYPE_MEMORY || type == ANIM_TYPE_BATTERY;
}

/**
 * @brief Check if filename is static image
 * @return TRUE for .ico, .png, .bmp, .jpg, .tif extensions
 */
static BOOL IsStaticImageFile(const char* filename) {
    if (!filename) return FALSE;
    
    static const char* staticExtensions[] = {
        ".ico", ".png", ".bmp", ".jpg", ".jpeg", ".tif", ".tiff", NULL
    };
    
    size_t len = strlen(filename);
    for (int i = 0; staticExtensions[i]; i++) {
        size_t extLen = strlen(staticExtensions[i]);
        if (len >= extLen) {
            const char* fileSuffix = filename + (len - extLen);
            if (_stricmp(fileSuffix, staticExtensions[i]) == 0) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/**
 * @brief Get system metrics with zero-value warm-up
 * @note Forces refresh if percent icon initially shows 0%
 */
static void GetSystemMetricsWithWarmup(float* cpu, float* mem) {
    SystemMonitor_GetUsage(cpu, mem);

    static AnimationType s_lastWarmupType = ANIM_TYPE_CUSTOM;
    static BOOL s_warmupAttempted = FALSE;
    AnimationType type = GetAnimationType(GetCurrentAnimationName());
    if (type != s_lastWarmupType) {
        s_lastWarmupType = type;
        s_warmupAttempted = FALSE;
    }

    if (type != ANIM_TYPE_CPU && type != ANIM_TYPE_MEMORY) {
        return;
    }

    float chosen = (type == ANIM_TYPE_CPU) ? *cpu : *mem;
    if (!s_warmupAttempted) {
        s_warmupAttempted = TRUE;
        if ((int)(chosen + 0.5f) == 0) {
            SystemMonitor_ForceRefresh();
            SystemMonitor_GetUsage(cpu, mem);
        }
    }
}

/**
 * @brief Build basic tooltip with CPU, memory, optional network
 * @param hasNet Whether to include network speed lines
 */
static void BuildBasicTooltip(wchar_t* tip, size_t tipSize, float cpu, float mem,
                              float upBps, float downBps, BOOL hasNet) {
    if (hasNet) {
        FormattedBytes upload = FormatBytesPerSecond((double)upBps);
        FormattedBytes download = FormatBytesPerSecond((double)downBps);
        _snwprintf_s(tip, tipSize, _TRUNCATE, L"CPU %.1f%%\nMemory %.1f%%\nUpload %.1f %s\nDownload %.1f %s",
                     cpu, mem, upload.value, upload.unit, download.value, download.unit);
    } else {
        _snwprintf_s(tip, tipSize, _TRUNCATE, L"CPU %.1f%%\nMemory %.1f%%", cpu, mem);
    }
}

/**
 * @brief Check if animation speed should be displayed in tooltip
 * @return FALSE for builtin icons and static images
 */
static BOOL ShouldShowAnimationSpeed(const char* animName) {
    if (!animName) return FALSE;
    
    if (IsBuiltinAnimationName(animName)) return FALSE;
    if (IsStaticImageFile(animName)) return FALSE;
    
    return TRUE;
}

/**
 * @brief Calculate speed percentage from metric
 * @return Percentage for CPU/Memory, or countdown progress for Timer
 */
static double CalculateSpeedMetricPercent(AnimationSpeedMetric metric, float cpu, float mem) {
    if (metric == ANIMATION_SPEED_CPU) {
        return (double)cpu;
    }
    
    if (metric == ANIMATION_SPEED_TIMER) {
        if (!CLOCK_SHOW_CURRENT_TIME && !CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0) {
            double p = (double)countdown_elapsed_time / (double)CLOCK_TOTAL_TIME;
            if (p < 0.0) p = 0.0;
            if (p > 1.0) p = 1.0;
            return p * 100.0;
        }
        return 0.0;
    }
    
    return (double)mem;
}

/** @brief Append "Speed · [Metric] X%" line to tooltip */
static void AppendSpeedLine(wchar_t* tip, size_t tipSize, AnimationSpeedMetric metric,
                           float cpu, float mem) {
    /* Original speed mode: don't show speed line (always 100%, no useful info) */
    if (metric == ANIMATION_SPEED_ORIGINAL) {
        return;
    }
    
    double percent = CalculateSpeedMetricPercent(metric, cpu, mem);
    
    BOOL applyScaling = TRUE;
    if (metric == ANIMATION_SPEED_TIMER) {
        if (CLOCK_SHOW_CURRENT_TIME || CLOCK_COUNT_UP || CLOCK_TOTAL_TIME <= 0) {
            applyScaling = FALSE;
        }
    }
    
    double scalePercent = GetAnimationSpeedScaleForPercent(applyScaling ? percent : 0.0);
    if (scalePercent <= 0.0) scalePercent = 100.0;
    
    const wchar_t* metricLabel = L"Memory";
    if (metric == ANIMATION_SPEED_CPU) metricLabel = L"CPU";
    else if (metric == ANIMATION_SPEED_TIMER) metricLabel = L"Timer";
    
    wchar_t extra[128];
    _snwprintf_s(extra, _countof(extra), _TRUNCATE, L"\nSpeed · %s %.1f%%", metricLabel, scalePercent);
    wcsncat_s(tip, tipSize, extra, _TRUNCATE);
}

/** @brief Update tray icon tooltip */
void UpdateTrayTooltip(const wchar_t* tip) {
    HWND owner = GetValidTrayMainWindow();
    if (!tip || !owner || !IsTrayIconActiveForWindow(owner)) {
        return;
    }

    NOTIFYICONDATAW n = {0};
    n.cbSize = sizeof(n);
    n.hWnd = owner;
    n.uID = CLOCK_ID_TRAY_APP_ICON;
    n.uFlags = NIF_TIP;
    wcsncpy_s(n.szTip, _countof(n.szTip), tip, _TRUNCATE);
    if (wcscmp(g_lastTrayTooltip, n.szTip) == 0) {
        return;
    }
    if (Shell_NotifyIconW(NIM_MODIFY, &n)) {
        wcscpy_s(g_lastTrayTooltip, _countof(g_lastTrayTooltip), n.szTip);
    }
}

static void ClearPendingTrayOpacitySave(void) {
    g_pendingOpacityToSave = -1;
    g_opacityRollbackValue = -1;
    g_pendingOpacitySaveRetryCount = 0;
}

static void RollBackPendingTrayOpacitySave(HWND hwnd) {
    if (g_opacityRollbackValue >= 0) {
        CLOCK_WINDOW_OPACITY = g_opacityRollbackValue;
        if (IsValidTrayMainWindow(hwnd)) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
    }
    ClearPendingTrayOpacitySave();
}

static void FlushPendingTrayOpacitySave(HWND hwnd) {
    if (g_pendingOpacityToSave >= 0) {
        char configPath[MAX_PATH];
        GetConfigPath(configPath, sizeof(configPath));
        if (!WriteIniInt(INI_SECTION_DISPLAY, "WINDOW_OPACITY",
                         g_pendingOpacityToSave, configPath)) {
            g_pendingOpacitySaveRetryCount++;
            if (g_pendingOpacitySaveRetryCount >= TRAY_OPACITY_SAVE_MAX_RETRIES) {
                LOG_WARNING("Failed to save tray opacity after %d attempts; dropping pending value: %d",
                            g_pendingOpacitySaveRetryCount, g_pendingOpacityToSave);
                RollBackPendingTrayOpacitySave(hwnd);
                return;
            }
            LOG_WARNING("Failed to save tray opacity: %d (attempt %d/%d)",
                        g_pendingOpacityToSave,
                        g_pendingOpacitySaveRetryCount,
                        TRAY_OPACITY_SAVE_MAX_RETRIES);
            return;
        }
        ClearPendingTrayOpacitySave();
    }
}

static void DiscardPendingTrayOpacitySave(void) {
    ClearPendingTrayOpacitySave();
}

static void ReschedulePendingTrayOpacitySave(HWND hwnd) {
    if (g_pendingOpacityToSave < 0) {
        return;
    }

    if (!IsValidTrayMainWindow(hwnd)) {
        return;
    }

    if (!SetTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID,
                  TRAY_OPACITY_SAVE_DELAY_MS, TrayOpacitySaveTimerProc)) {
        LOG_WARNING("Failed to reschedule pending tray opacity save (error=%lu)",
                    GetLastError());
        RollBackPendingTrayOpacitySave(hwnd);
    }
}

static void CompleteTrayOpacityFeedback(HWND hwnd, BOOL refreshTooltip) {
    FlushPendingTrayOpacitySave(hwnd);

    if (g_showingOpacityTip) {
        g_showingOpacityTip = FALSE;
        if (refreshTooltip && hwnd && nid.hWnd) {
            TrayTipTimerProc(hwnd, WM_TIMER, TRAY_TIP_TIMER_ID, 0);
        }
    }
}

static void CALLBACK TrayOpacitySaveTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)msg;
    (void)time;

    if (id != TRAY_OPACITY_SAVE_TIMER_ID || !IsValidTrayMainWindow(hwnd)) return;

    KillTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID);
    CompleteTrayOpacityFeedback(hwnd, TRUE);
    if (g_pendingOpacityToSave >= 0 &&
        !SetTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID,
                  TRAY_OPACITY_SAVE_DELAY_MS, TrayOpacitySaveTimerProc)) {
        LOG_WARNING("Failed to reschedule tray opacity save retry (error=%lu)",
                    GetLastError());
        RollBackPendingTrayOpacitySave(hwnd);
    }
}

void HandleTrayOpacityWheel(HWND hwnd, int wheelDirection, BOOL ctrlPressed) {
    if (!IsValidTrayMainWindow(hwnd)) return;

    extern int ReadConfigOpacityStepNormal(void);
    extern int ReadConfigOpacityStepFast(void);

    int step = ctrlPressed ? ReadConfigOpacityStepFast() : ReadConfigOpacityStepNormal();
    if (step <= 0) step = 1;

    int oldOpacity = CLOCK_WINDOW_OPACITY;
    CLOCK_WINDOW_OPACITY += (wheelDirection > 0) ? step : -step;
    if (CLOCK_WINDOW_OPACITY < 0) CLOCK_WINDOW_OPACITY = 0;
    if (CLOCK_WINDOW_OPACITY > 100) CLOCK_WINDOW_OPACITY = 100;

    g_showingOpacityTip = TRUE;
    wchar_t opacityTip[64];
    _snwprintf_s(opacityTip, _countof(opacityTip), _TRUNCATE,
                 L"Opacity: %d%%", CLOCK_WINDOW_OPACITY);
    UpdateTrayTooltip(opacityTip);

    if (CLOCK_WINDOW_OPACITY != oldOpacity) {
        InvalidateRect(hwnd, NULL, FALSE);
        if (g_pendingOpacityToSave < 0) {
            g_opacityRollbackValue = oldOpacity;
        }
        g_pendingOpacityToSave = CLOCK_WINDOW_OPACITY;
        g_pendingOpacitySaveRetryCount = 0;
    }

    if (!SetTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID, TRAY_OPACITY_SAVE_DELAY_MS,
                  TrayOpacitySaveTimerProc)) {
        CompleteTrayOpacityFeedback(hwnd, TRUE);
        if (g_pendingOpacityToSave >= 0) {
            LOG_WARNING("Dropping pending tray opacity save after timer start failure");
            RollBackPendingTrayOpacitySave(hwnd);
        }
    }
}

/**
 * @brief Periodic timer callback (1s interval)
 * @note Updates tooltip with CPU, memory, network, and animation speed
 */
void CALLBACK TrayTipTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)time;

    if (msg != WM_TIMER ||
        id != TRAY_TIP_TIMER_ID ||
        !IsTrayIconActiveForWindow(hwnd)) {
        return;
    }

    if (IsTrayInteractionSuspended()) {
        return;
    }

    /* Keep percent icons fresh while preserving the temporary opacity tooltip. */
    if (g_showingOpacityTip) {
        TrayAnimation_UpdatePercentIconIfNeeded();
        return;
    }

    float cpu, mem, upBps, downBps;
    GetSystemMetricsWithWarmup(&cpu, &mem);
    BOOL hasNet = SystemMonitor_GetNetSpeed(&upBps, &downBps);

    wchar_t tip[256] = {0};
    BuildBasicTooltip(tip, _countof(tip), cpu, mem, upBps, downBps, hasNet);

    const char* animName = GetCurrentAnimationName();
    if (ShouldShowAnimationSpeed(animName)) {
        AnimationSpeedMetric metric = GetAnimationSpeedMetric();
        AppendSpeedLine(tip, _countof(tip), metric, cpu, mem);
    }

    UpdateTrayTooltip(tip);
    TrayAnimation_UpdatePercentIconWithMetrics(cpu, mem);
}

/**
 * @brief Register for taskbar recreation events
 * @note Enables auto-restore when Explorer restarts
 */
void RegisterTaskbarCreatedMessage() {
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
}

/**
 * @brief Get initial icon for percent animations
 * @return HICON with warm-up sampling, or NULL for non-percent types
 * @note Takes two samples to avoid initial 0% reading
 */
static HICON GetInitialPercentIcon(AnimationType type) {
    if (!IsPercentIcon(type)) return NULL;
    
    int percent = 0;
    
    if (type == ANIM_TYPE_BATTERY) {
        int batteryPercent = 0;
        if (SystemMonitor_GetBatteryPercent(&batteryPercent)) {
            percent = batteryPercent;
        }
    } else {
        float cpu = 0.0f, mem = 0.0f;
        SystemMonitor_ForceRefresh();
        SystemMonitor_GetUsage(&cpu, &mem);
        percent = (type == ANIM_TYPE_CPU) ? (int)(cpu + 0.5f) : (int)(mem + 0.5f);
    }
    
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    return CreatePercentIcon16(percent);
}

static HICON GetInitialDynamicBuiltinIcon(AnimationType type) {
    if (IsPercentIcon(type)) {
        return GetInitialPercentIcon(type);
    }

    if (type == ANIM_TYPE_CAPSLOCK) {
        return CreateCapsLockIcon(IsCapsLockOn());
    }

    return NULL;
}

/**
 * @brief Initialize and add tray icon
 * @param hwnd Window handle for callbacks
 * @param hInstance App instance for icon resources
 */
static void InitTrayIconInternal(HWND hwnd, HINSTANCE hInstance,
                                 BOOL preloadAnimation,
                                 BOOL startBackgroundWork,
                                 BOOL useAnimationInitialIcon) {
    if (!IsValidTrayMainWindow(hwnd)) {
        LOG_WARNING("InitTrayIconInternal called with invalid main window");
        return;
    }

    g_mainHwnd = hwnd;
    g_hInstance = hInstance;
    g_lastTrayTooltip[0] = L'\0';
    SetRectEmpty(&g_cachedIconRect);
    g_lastRectUpdateTime = 0;

    if (startBackgroundWork) {
        ReadPercentIconColorsConfig();
        SystemMonitor_Init();
    }
    if (preloadAnimation) {
        PreloadAnimationFromConfig();
    }
    
    BOOL destroyInitialIcon = FALSE;
    HICON hInitial = NULL;
    if (useAnimationInitialIcon) {
        const char* animName = GetCurrentAnimationName();
        AnimationType type = GetAnimationType(animName);
        hInitial = GetInitialDynamicBuiltinIcon(type);
        if (hInitial) {
            destroyInitialIcon = TRUE;
        } else {
            hInitial = GetInitialAnimationHicon();
        }
    }
    
    memset(&nid, 0, sizeof(nid));
    g_trayIconActive = FALSE;
    nid.cbSize = sizeof(nid);
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = hInitial ? hInitial : LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CATIME));
    nid.hWnd = hwnd;
    nid.uCallbackMessage = CLOCK_WM_TRAYICON;
    wcscpy_s(nid.szTip, _countof(nid.szTip), L"CPU --.-%\nMemory --.-%\nUpload --.- ?/s\nDownload --.- ?/s");

    if (Shell_NotifyIconW(NIM_ADD, &nid)) {
        g_trayIconActive = TRUE;
        wcscpy_s(g_lastTrayTooltip, _countof(g_lastTrayTooltip), nid.szTip);
    } else {
        LOG_WARNING("Failed to add tray icon (error=%lu)", GetLastError());
        ZeroMemory(&nid, sizeof(nid));
    }
    if (destroyInitialIcon) {
        DestroyIcon(hInitial);
    }
    
    /* Note: We don't use NOTIFYICON_VERSION_4 because it changes message format
     * and doesn't reliably send WM_MOUSEMOVE. Instead, we use a timer-based
     * approach to detect mouse hover over tray icon. */

    if (WM_TASKBARCREATED == 0) {
        RegisterTaskbarCreatedMessage();
    }
    
    if (startBackgroundWork && g_trayIconActive) {
        if (!SetTimer(hwnd, TRAY_TIP_TIMER_ID, TOOLTIP_UPDATE_INTERVAL_MS, (TIMERPROC)TrayTipTimerProc)) {
            LOG_WARNING("Failed to start tray tooltip timer (error=%lu)", GetLastError());
        }
    }

    /* Mouse hook is now installed on-demand when mouse hovers over tray icon */
}

void InitTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    InitTrayIconInternal(hwnd, hInstance, TRUE, TRUE, TRUE);
}

static void RemoveTrayIconInternal(BOOL finalCleanup) {
    if (g_trayIconActive && nid.hWnd) {
        KillTimer(nid.hWnd, TRAY_TIP_TIMER_ID);
        KillTimer(nid.hWnd, TRAY_OPACITY_SAVE_TIMER_ID);
        CompleteTrayOpacityFeedback(nid.hWnd, FALSE);
        if (finalCleanup) {
            DiscardPendingTrayOpacitySave();
        } else {
            ReschedulePendingTrayOpacitySave(nid.hWnd);
        }
    }

    /* Stop hover detection timer */
    StopTrayHoverDetection();

    /* Uninstall mouse hook */
    if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = NULL;
    }

    if (finalCleanup) {
        SystemMonitor_Shutdown();
        CleanupTraySubmenuResources();
    }
    if (g_trayIconActive && nid.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }
    g_trayIconActive = FALSE;
    ZeroMemory(&nid, sizeof(nid));
    g_mainHwnd = NULL;
    g_lastTrayTooltip[0] = L'\0';
    SetRectEmpty(&g_cachedIconRect);
    g_lastRectUpdateTime = 0;
    if (finalCleanup) {
        DiscardPendingTrayOpacitySave();
    }
}

/** @brief Remove tray icon and cleanup */
void RemoveTrayIcon(void) {
    RemoveTrayIconInternal(TRUE);
}

static BOOL IsUtf8ContinuationByte(unsigned char ch) {
    return (ch & 0xC0u) == 0x80u;
}

static size_t FindUtf8PrefixForWideCapacity(const char* text, size_t wideCapacity) {
    if (!text || wideCapacity <= 1) return 0;

    size_t bytes = 0;
    size_t chars = 0;
    while (text[bytes] && chars < wideCapacity - 1) {
        size_t charBytes = 1;
        unsigned char ch = (unsigned char)text[bytes];
        if ((ch & 0x80u) == 0) {
            charBytes = 1;
        } else if ((ch & 0xE0u) == 0xC0u) {
            charBytes = 2;
        } else if ((ch & 0xF0u) == 0xE0u) {
            charBytes = 3;
        } else if ((ch & 0xF8u) == 0xF0u) {
            charBytes = 4;
        } else {
            break;
        }

        for (size_t i = 1; i < charBytes; i++) {
            if (!IsUtf8ContinuationByte((unsigned char)text[bytes + i])) {
                return bytes;
            }
        }

        bytes += charBytes;
        chars++;
    }

    return bytes;
}

/**
 * @brief Show balloon notification
 * @param message UTF-8 message text
 * @note Displays for 3 seconds without title
 */
void ShowTrayNotification(HWND hwnd, const char* message) {
    if (!message) return;
    HWND owner = IsValidTrayMainWindow(hwnd) ? hwnd : GetValidTrayMainWindow();
    if (!owner || !IsTrayIconActiveForWindow(owner)) {
        return;
    }

    NOTIFYICONDATAW nid_notify = {0};
    nid_notify.cbSize = sizeof(NOTIFYICONDATAW);
    nid_notify.hWnd = owner;
    nid_notify.uID = CLOCK_ID_TRAY_APP_ICON;
    nid_notify.uFlags = NIF_INFO;
    nid_notify.dwInfoFlags = NIIF_NONE;
    nid_notify.uTimeout = 3000;

    int converted = MultiByteToWideChar(CP_UTF8, 0, message, -1, nid_notify.szInfo,
                                        (int)_countof(nid_notify.szInfo));
    if (converted <= 0) {
        size_t bytes = FindUtf8PrefixForWideCapacity(message, _countof(nid_notify.szInfo));
        if (bytes == 0 || bytes > (size_t)INT_MAX) {
            return;
        }
        converted = MultiByteToWideChar(CP_UTF8, 0, message, (int)bytes,
                                        nid_notify.szInfo,
                                        (int)_countof(nid_notify.szInfo) - 1);
        if (converted <= 0) {
            return;
        }
        nid_notify.szInfo[converted] = L'\0';
    }
    nid_notify.szInfoTitle[0] = L'\0';

    Shell_NotifyIconW(NIM_MODIFY, &nid_notify);
}

/** @brief Recreate tray icon after taskbar restart */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance) {
    StopTrayHoverDetection();

    if (g_trayIconActive && nid.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }
    g_trayIconActive = FALSE;
    ZeroMemory(&nid, sizeof(nid));
    InitTrayIconInternal(hwnd, hInstance, FALSE, FALSE, FALSE);
    if (!IsTrayIconActive(hwnd)) {
        KillTimer(hwnd, TRAY_TIP_TIMER_ID);
        KillTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID);
        CompleteTrayOpacityFeedback(hwnd, FALSE);
        ReschedulePendingTrayOpacitySave(hwnd);
        if (TrayAnimation_IsRunning()) {
            StopTrayAnimation(hwnd);
        }
        return;
    }
    if (TrayAnimation_IsRunning()) {
        TrayAnimation_RefreshCurrentIcon();
    } else {
        StartTrayAnimation(hwnd, TRAY_ANIMATION_DEFAULT_INTERVAL_MS);
    }
}

/** @brief Update tray icon by recreation */
void UpdateTrayIcon(HWND hwnd) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    RecreateTaskbarIcon(hwnd, hInstance);
}

/**
 * @brief Install mouse hook for tray wheel events
 * @note Called when mouse enters tray icon area (NIN_POPUPOPEN)
 */
void InstallTrayMouseHook(void) {
    if (!g_mouseHook && g_hInstance) {
        g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, g_hInstance, 0);
        /* Invalidate position cache to get fresh coordinates */
        g_lastRectUpdateTime = 0;
    }
}

/**
 * @brief Check if mouse hook is currently installed
 * @return TRUE if hook is installed
 */
BOOL IsTrayMouseHookInstalled(void) {
    return g_mouseHook != NULL;
}

/**
 * @brief Check if mouse is over tray icon area
 * @param pt Mouse position in screen coordinates
 * @return TRUE if mouse is over tray icon
 */
BOOL IsMouseOverTrayIconArea(POINT pt) {
    return IsMouseOverTrayIconCached(pt);
}

void SetTrayInteractionSuspended(BOOL suspended) {
    InterlockedExchange(&g_trayInteractionSuspended, suspended ? 1L : 0L);

    if (suspended) {
        if (g_mouseHook) {
            UnhookWindowsHookEx(g_mouseHook);
            g_mouseHook = NULL;
        }
        return;
    }

    if (g_showingOpacityTip) {
        g_showingOpacityTip = FALSE;
    }

    HWND hwndMain = GetValidTrayMainWindow();
    if (hwndMain) {
        TrayAnimation_RefreshCurrentIcon();
        TrayTipTimerProc(hwndMain, WM_TIMER, TRAY_TIP_TIMER_ID, 0);
    }
}

BOOL IsTrayInteractionSuspended(void) {
    return InterlockedCompareExchange(&g_trayInteractionSuspended, 0, 0) != 0;
}

/**
 * @brief Uninstall mouse hook for tray wheel events
 * @note Called when mouse leaves tray icon area (NIN_POPUPCLOSE)
 */
void UninstallTrayMouseHook(void) {
    if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = NULL;
    }
    /* Reset opacity tip mode when mouse leaves */
    if (g_showingOpacityTip) {
        g_showingOpacityTip = FALSE;
        HWND hwndMain = GetValidTrayMainWindow();
        if (hwndMain) {
            TrayTipTimerProc(hwndMain, WM_TIMER, TRAY_TIP_TIMER_ID, 0);
        }
    }
}
