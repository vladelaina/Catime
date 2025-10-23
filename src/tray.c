/**
 * @file tray.c
 * @brief Refactored system tray icon management with improved modularity
 * @version 2.0 - Enhanced maintainability through function decomposition
 * 
 * Major improvements:
 * - Extracted helper functions for byte formatting and animation type checking
 * - Decomposed 142-line timer function into focused modules
 * - Centralized external declarations for clarity
 * - Simplified static image detection logic
 */
#include <windows.h>
#include <shellapi.h>
#include <string.h>
#include "../include/language.h"
#include "../resource/resource.h"
#include "../include/tray.h"
#include "../include/tray_animation.h"
#include "../include/system_monitor.h"
#include "../include/config.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Timer ID for periodically updating tray tooltip with CPU/MEM */
#define TRAY_TIP_TIMER_ID 42421

/** @brief Tooltip update interval in milliseconds */
#define TOOLTIP_UPDATE_INTERVAL_MS 1000

/** @brief Percent icon warm-up delay for accurate initial reading */
#define PERCENT_ICON_WARMUP_MS 120

/* ============================================================================
 * Global Variables
 * ============================================================================ */

/** @brief Global tray icon data structure for Shell_NotifyIcon operations */
NOTIFYICONDATAW nid;

/** @brief Custom Windows message ID for taskbar recreation events */
UINT WM_TASKBARCREATED = 0;

/* ============================================================================
 * External Declarations
 * ============================================================================ */

extern BOOL CLOCK_COUNT_UP;
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern int CLOCK_TOTAL_TIME;
extern int countdown_elapsed_time;
extern void TrayAnimation_UpdatePercentIconIfNeeded(void);
extern void ReadPercentIconColorsConfig(void);

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/** @brief Animation icon type categories */
typedef enum {
    ANIM_TYPE_CUSTOM,      /**< User-defined animation (GIF/WebP/folder) */
    ANIM_TYPE_LOGO,        /**< Built-in application logo */
    ANIM_TYPE_CPU,         /**< CPU usage percentage icon */
    ANIM_TYPE_MEMORY       /**< Memory usage percentage icon */
} AnimationType;

/** @brief Formatted byte value with appropriate unit */
typedef struct {
    double value;          /**< Scaled numeric value */
    const wchar_t* unit;   /**< Display unit (B/s, KB/s, MB/s, GB/s) */
} FormattedBytes;

/* ============================================================================
 * Helper Functions - Formatting
 * ============================================================================ */

/**
 * @brief Format bytes per second with appropriate unit scaling
 * @param bytes Raw bytes per second value
 * @return Formatted value with unit (e.g., 1536 -> {1.5, "KB/s"})
 */
static FormattedBytes FormatBytesPerSecond(double bytes) {
    FormattedBytes result = { bytes, L"B/s" };
    
    if (result.value >= 1024.0) { result.value /= 1024.0; result.unit = L"KB/s"; }
    if (result.value >= 1024.0) { result.value /= 1024.0; result.unit = L"MB/s"; }
    if (result.value >= 1024.0) { result.value /= 1024.0; result.unit = L"GB/s"; }
    
    return result;
}

/* ============================================================================
 * Helper Functions - Animation Type Detection
 * ============================================================================ */

/**
 * @brief Determine animation type from name
 * @param animName Animation name string (can be NULL)
 * @return Animation type category
 */
static AnimationType GetAnimationType(const char* animName) {
    if (!animName) return ANIM_TYPE_CUSTOM;
    if (_stricmp(animName, "__logo__") == 0) return ANIM_TYPE_LOGO;
    if (_stricmp(animName, "__cpu__") == 0) return ANIM_TYPE_CPU;
    if (_stricmp(animName, "__mem__") == 0) return ANIM_TYPE_MEMORY;
    return ANIM_TYPE_CUSTOM;
}

/**
 * @brief Check if animation is a percentage-based icon
 * @param type Animation type
 * @return TRUE for CPU or Memory percentage icons
 */
static inline BOOL IsPercentIcon(AnimationType type) {
    return type == ANIM_TYPE_CPU || type == ANIM_TYPE_MEMORY;
}

/**
 * @brief Check if animation is a built-in icon type
 * @param type Animation type
 * @return TRUE for logo, CPU, or memory icons
 */
static inline BOOL IsBuiltinIcon(AnimationType type) {
    return type != ANIM_TYPE_CUSTOM;
}

/**
 * @brief Check if filename is a static image format
 * @param filename File name to check
 * @return TRUE if file extension matches static image formats
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

/* ============================================================================
 * Helper Functions - System Metrics
 * ============================================================================ */

/**
 * @brief Get system metrics with zero-value warm-up for percent icons
 * @param cpu Output CPU usage percentage
 * @param mem Output memory usage percentage
 */
static void GetSystemMetricsWithWarmup(float* cpu, float* mem) {
    SystemMonitor_GetUsage(cpu, mem);
    
    /* Force refresh if percent icon shows zero at startup */
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
 * @brief Build basic tooltip with CPU, memory, and optional network info
 * @param tip Output buffer for tooltip text
 * @param tipSize Size of tip buffer in wide characters
 * @param cpu CPU usage percentage
 * @param mem Memory usage percentage
 * @param upBps Upload speed in bytes per second
 * @param downBps Download speed in bytes per second
 * @param hasNet Whether network speed is available
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
 * @brief Check if animation speed should be displayed
 * @param animName Current animation name
 * @return TRUE if speed line should be shown
 */
static BOOL ShouldShowAnimationSpeed(const char* animName) {
    if (!animName) return FALSE;
    
    AnimationType type = GetAnimationType(animName);
    if (IsBuiltinIcon(type)) return FALSE;
    if (IsStaticImageFile(animName)) return FALSE;
    
    return TRUE;
}

/**
 * @brief Calculate animation speed percentage based on metric
 * @param metric Speed metric type (CPU, Memory, or Timer)
 * @param cpu Current CPU usage
 * @param mem Current memory usage
 * @return Percentage value for speed calculation
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
    
    return (double)mem;  /* ANIMATION_SPEED_MEMORY */
}

/**
 * @brief Append animation speed line to tooltip
 * @param tip Tooltip buffer to append to
 * @param tipSize Size of tooltip buffer
 * @param metric Speed metric type
 * @param cpu Current CPU usage
 * @param mem Current memory usage
 */
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

/**
 * @brief Update tray icon tooltip text
 * @param tip Tooltip text to display
 */
static void UpdateTrayTooltip(const wchar_t* tip) {
    NOTIFYICONDATAW n = {0};
    n.cbSize = sizeof(n);
    n.hWnd = nid.hWnd;
    n.uID = nid.uID;
    n.uFlags = NIF_TIP;
    wcsncpy_s(n.szTip, _countof(n.szTip), tip, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

/* ============================================================================
 * Timer Callback
 * ============================================================================ */

/**
 * @brief Periodic timer callback to update tray tooltip with system metrics
 * @param hwnd Window handle (unused)
 * @param msg Message (unused)
 * @param id Timer ID (unused)
 * @param time System time (unused)
 * 
 * Updates tooltip every second with CPU, memory, network speed, and
 * optionally animation speed based on current metric settings.
 */
static void CALLBACK TrayTipTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)hwnd; (void)msg; (void)id; (void)time;
    
    /* Gather system metrics */
    float cpu, mem, upBps, downBps;
    GetSystemMetricsWithWarmup(&cpu, &mem);
    BOOL hasNet = SystemMonitor_GetNetSpeed(&upBps, &downBps);
    
    /* Build basic tooltip */
    wchar_t tip[256] = {0};
    BuildBasicTooltip(tip, _countof(tip), cpu, mem, upBps, downBps, hasNet);
    
    /* Append animation speed if applicable */
    const char* animName = GetCurrentAnimationName();
    if (ShouldShowAnimationSpeed(animName)) {
        AnimationSpeedMetric metric = GetAnimationSpeedMetric();
        AppendSpeedLine(tip, _countof(tip), metric, cpu, mem);
    }
    
    /* Update tray icon */
    UpdateTrayTooltip(tip);
    TrayAnimation_UpdatePercentIconIfNeeded();
}

/**
 * @brief One-shot timer to obtain first non-zero CPU/MEM sample and update percent icon.
 */
/* removed: TrayFirstCpuTimerProc */

/**
 * @brief Register for taskbar recreation notification messages
 * Enables automatic tray icon restoration when Windows Explorer restarts
 */
void RegisterTaskbarCreatedMessage() {
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
}

/* ============================================================================
 * Helper Functions - Initialization
 * ============================================================================ */

/**
 * @brief Get initial icon for percent-based animations with warm-up sampling
 * @param type Animation type
 * @return HICON for percent icon, or NULL for non-percent types
 * 
 * Takes two samples with delay to ensure accurate initial percentage reading.
 */
static HICON GetInitialPercentIcon(AnimationType type) {
    if (!IsPercentIcon(type)) return NULL;
    
    float cpu = 0.0f, mem = 0.0f;
    
    /* Take two samples with delay to avoid initial 0% */
    SystemMonitor_ForceRefresh();
    Sleep(PERCENT_ICON_WARMUP_MS);
    SystemMonitor_ForceRefresh();
    SystemMonitor_GetUsage(&cpu, &mem);
    
    int percent = (type == ANIM_TYPE_CPU) ? (int)(cpu + 0.5f) : (int)(mem + 0.5f);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    return CreatePercentIcon16(percent);
}

/* ============================================================================
 * Public Functions - Tray Icon Management
 * ============================================================================ */

/**
 * @brief Initialize and add tray icon to system notification area
 * @param hwnd Main window handle for tray icon callbacks
 * @param hInstance Application instance for icon resource loading
 * 
 * Sets up icon with appropriate initial state (logo, percent icon, or animation),
 * configures tooltip, and starts periodic updates.
 */
void InitTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    /* Initialize configuration and system monitoring */
    ReadPercentIconColorsConfig();
    SystemMonitor_Init();
    PreloadAnimationFromConfig();
    
    /* Get initial icon based on animation type */
    const char* animName = GetCurrentAnimationName();
    AnimationType type = GetAnimationType(animName);
    HICON hInitial = GetInitialPercentIcon(type);
    if (!hInitial) {
        hInitial = GetInitialAnimationHicon();
    }
    
    /* Configure tray icon data */
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = hInitial ? hInitial : LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CATIME));
    nid.hWnd = hwnd;
    nid.uCallbackMessage = CLOCK_WM_TRAYICON;
    wcscpy_s(nid.szTip, _countof(nid.szTip), L"CPU --.-%\nMemory --.-%\nUpload --.- ?/s\nDownload --.- ?/s");
    
    /* Add icon to system tray */
    Shell_NotifyIconW(NIM_ADD, &nid);
    
    /* Register for taskbar recreation events */
    if (WM_TASKBARCREATED == 0) {
        RegisterTaskbarCreatedMessage();
    }
    
    /* Start periodic tooltip updates */
    SetTimer(hwnd, TRAY_TIP_TIMER_ID, TOOLTIP_UPDATE_INTERVAL_MS, (TIMERPROC)TrayTipTimerProc);
}

/**
 * @brief Remove tray icon from system notification area
 * Cleanly removes icon when application exits or hides
 */
void RemoveTrayIcon(void) {
    if (nid.hWnd) {
        KillTimer(nid.hWnd, TRAY_TIP_TIMER_ID);
    }
    SystemMonitor_Shutdown();
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

/**
 * @brief Display balloon notification from system tray icon
 * @param hwnd Window handle associated with the tray icon
 * @param message UTF-8 encoded message text to display
 * Shows 3-second notification balloon without title or special icons
 */
void ShowTrayNotification(HWND hwnd, const char* message) {
    NOTIFYICONDATAW nid_notify = {0};
    nid_notify.cbSize = sizeof(NOTIFYICONDATAW);
    nid_notify.hWnd = hwnd;
    nid_notify.uID = CLOCK_ID_TRAY_APP_ICON;
    nid_notify.uFlags = NIF_INFO;
    nid_notify.dwInfoFlags = NIIF_NONE;        /**< No special icon in notification */
    nid_notify.uTimeout = 3000;                /**< 3-second display duration */
    
    /** Convert UTF-8 message to wide character format for Windows API */
    MultiByteToWideChar(CP_UTF8, 0, message, -1, nid_notify.szInfo, sizeof(nid_notify.szInfo)/sizeof(WCHAR));
    nid_notify.szInfoTitle[0] = L'\0';         /**< No title, message only */
    
    Shell_NotifyIconW(NIM_MODIFY, &nid_notify);
}

/**
 * @brief Recreate tray icon after taskbar restart or system changes
 * @param hwnd Main window handle
 * @param hInstance Application instance handle
 * Performs clean removal and re-initialization to restore tray icon
 */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance) {
    RemoveTrayIcon();
    InitTrayIcon(hwnd, hInstance);
}

/**
 * @brief Update tray icon by recreation with current window state
 * @param hwnd Main window handle to extract instance from
 * Convenience function that retrieves instance and recreates icon
 */
void UpdateTrayIcon(HWND hwnd) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    RecreateTaskbarIcon(hwnd, hInstance);
}