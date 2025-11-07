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
#include "tray/tray_animation_percent.h"
#include "system_monitor.h"
#include "config.h"

#define TOOLTIP_UPDATE_INTERVAL_MS 1000
#define PERCENT_ICON_WARMUP_MS 120

/** @brief Global tray icon data for Shell_NotifyIcon */
NOTIFYICONDATAW nid;

/** @brief Taskbar recreation message ID */
UINT WM_TASKBARCREATED = 0;

extern void ReadPercentIconColorsConfig(void);

/** @brief Animation type categories */
typedef enum {
    ANIM_TYPE_CUSTOM,
    ANIM_TYPE_LOGO,
    ANIM_TYPE_CPU,
    ANIM_TYPE_MEMORY
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
    return ANIM_TYPE_CUSTOM;
}

/** @brief Check if type is percent-based (CPU/Memory) */
static inline BOOL IsPercentIcon(AnimationType type) {
    return type == ANIM_TYPE_CPU || type == ANIM_TYPE_MEMORY;
}

/** @brief Check if type is builtin (not custom) */
static inline BOOL IsBuiltinIcon(AnimationType type) {
    return type != ANIM_TYPE_CUSTOM;
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
    
    AnimationType type = GetAnimationType(GetCurrentAnimationName());
    if (IsPercentIcon(type)) {
        float chosen = (type == ANIM_TYPE_CPU) ? *cpu : *mem;
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
        swprintf_s(tip, tipSize, L"CPU %.1f%%\nMemory %.1f%%\nUpload %.1f %s\nDownload %.1f %s",
                   cpu, mem, upload.value, upload.unit, download.value, download.unit);
    } else {
        swprintf_s(tip, tipSize, L"CPU %.1f%%\nMemory %.1f%%", cpu, mem);
    }
}

/**
 * @brief Check if animation speed should be displayed in tooltip
 * @return FALSE for builtin icons and static images
 */
static BOOL ShouldShowAnimationSpeed(const char* animName) {
    if (!animName) return FALSE;
    
    AnimationType type = GetAnimationType(animName);
    if (IsBuiltinIcon(type)) return FALSE;
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
    swprintf_s(extra, _countof(extra), L"\nSpeed · %s %.1f%%", metricLabel, scalePercent);
    wcsncat_s(tip, tipSize, extra, _TRUNCATE);
}

/** @brief Update tray icon tooltip */
static void UpdateTrayTooltip(const wchar_t* tip) {
    NOTIFYICONDATAW n = {0};
    n.cbSize = sizeof(n);
    n.hWnd = nid.hWnd;
    n.uID = nid.uID;
    n.uFlags = NIF_TIP;
    wcsncpy_s(n.szTip, _countof(n.szTip), tip, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

/**
 * @brief Periodic timer callback (1s interval)
 * @note Updates tooltip with CPU, memory, network, and animation speed
 */
static void CALLBACK TrayTipTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)hwnd; (void)msg; (void)id; (void)time;
    
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
    TrayAnimation_UpdatePercentIconIfNeeded();
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
    
    float cpu = 0.0f, mem = 0.0f;
    
    SystemMonitor_ForceRefresh();
    Sleep(PERCENT_ICON_WARMUP_MS);
    SystemMonitor_ForceRefresh();
    SystemMonitor_GetUsage(&cpu, &mem);
    
    int percent = (type == ANIM_TYPE_CPU) ? (int)(cpu + 0.5f) : (int)(mem + 0.5f);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    return CreatePercentIcon16(percent);
}

/**
 * @brief Initialize and add tray icon
 * @param hwnd Window handle for callbacks
 * @param hInstance App instance for icon resources
 */
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    ReadPercentIconColorsConfig();
    SystemMonitor_Init();
    PreloadAnimationFromConfig();
    
    const char* animName = GetCurrentAnimationName();
    AnimationType type = GetAnimationType(animName);
    HICON hInitial = GetInitialPercentIcon(type);
    if (!hInitial) {
        hInitial = GetInitialAnimationHicon();
    }
    
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = hInitial ? hInitial : LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CATIME));
    nid.hWnd = hwnd;
    nid.uCallbackMessage = CLOCK_WM_TRAYICON;
    wcscpy_s(nid.szTip, _countof(nid.szTip), L"CPU --.-%\nMemory --.-%\nUpload --.- ?/s\nDownload --.- ?/s");
    
    Shell_NotifyIconW(NIM_ADD, &nid);
    
    if (WM_TASKBARCREATED == 0) {
        RegisterTaskbarCreatedMessage();
    }
    
    SetTimer(hwnd, TRAY_TIP_TIMER_ID, TOOLTIP_UPDATE_INTERVAL_MS, (TIMERPROC)TrayTipTimerProc);
}

/** @brief Remove tray icon and cleanup */
void RemoveTrayIcon(void) {
    if (nid.hWnd) {
        KillTimer(nid.hWnd, TRAY_TIP_TIMER_ID);
    }
    SystemMonitor_Shutdown();
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

/**
 * @brief Show balloon notification
 * @param message UTF-8 message text
 * @note Displays for 3 seconds without title
 */
void ShowTrayNotification(HWND hwnd, const char* message) {
    NOTIFYICONDATAW nid_notify = {0};
    nid_notify.cbSize = sizeof(NOTIFYICONDATAW);
    nid_notify.hWnd = hwnd;
    nid_notify.uID = CLOCK_ID_TRAY_APP_ICON;
    nid_notify.uFlags = NIF_INFO;
    nid_notify.dwInfoFlags = NIIF_NONE;
    nid_notify.uTimeout = 3000;
    
    MultiByteToWideChar(CP_UTF8, 0, message, -1, nid_notify.szInfo, sizeof(nid_notify.szInfo)/sizeof(WCHAR));
    nid_notify.szInfoTitle[0] = L'\0';
    
    Shell_NotifyIconW(NIM_MODIFY, &nid_notify);
}

/** @brief Recreate tray icon after taskbar restart */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance) {
    RemoveTrayIcon();
    InitTrayIcon(hwnd, hInstance);
}

/** @brief Update tray icon by recreation */
void UpdateTrayIcon(HWND hwnd) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    RecreateTaskbarIcon(hwnd, hInstance);
}