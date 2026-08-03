/**
 * @file tray_background.c
 * @brief Tray animation/system-monitor requirements and background timers.
 */

#include "tray_internal.h"
#include "config.h"
#include "system_monitor.h"
#include "taskbar_monitor.h"
#include "tray/tray_animation_core.h"
#include "log.h"

AnimationType GetAnimationType(const char* animName) {
    if (!animName) return ANIM_TYPE_CUSTOM;
    if (_stricmp(animName, "__logo__") == 0) return ANIM_TYPE_LOGO;
    if (_stricmp(animName, "__cpu__") == 0) return ANIM_TYPE_CPU;
    if (_stricmp(animName, "__mem__") == 0) return ANIM_TYPE_MEMORY;
    if (_stricmp(animName, "__battery__") == 0) return ANIM_TYPE_BATTERY;
    if (_stricmp(animName, "__capslock__") == 0) return ANIM_TYPE_CAPSLOCK;
    if (_stricmp(animName, "__none__") == 0) return ANIM_TYPE_NONE;
    return ANIM_TYPE_CUSTOM;
}

BOOL IsPercentIcon(AnimationType type) {
    return type == ANIM_TYPE_CPU || type == ANIM_TYPE_MEMORY ||
           type == ANIM_TYPE_BATTERY;
}

BOOL IsStaticImageFile(const char* filename) {
    if (!filename) return FALSE;
    static const char* staticExtensions[] = {
        ".ico", ".png", ".bmp", ".jpg", ".jpeg", ".tif", ".tiff", NULL
    };

    size_t len = strlen(filename);
    for (int i = 0; staticExtensions[i]; i++) {
        size_t extLen = strlen(staticExtensions[i]);
        if (len >= extLen &&
            _stricmp(filename + len - extLen, staticExtensions[i]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CurrentTrayIconNeedsBackgroundRefresh(void) {
    return TrayAnimation_GetBuiltinRefreshNeeds(NULL);
}

BOOL CurrentTrayIconNeedsSystemMonitor(void) {
    BOOL needsSystemMonitor = FALSE;
    TrayAnimation_GetBuiltinRefreshNeeds(&needsSystemMonitor);
    return needsSystemMonitor;
}

BOOL CurrentAnimationSpeedNeedsSystemMonitor(void) {
    AnimationSpeedMetric metric = GetAnimationSpeedMetric();
    const char* animName = GetCurrentAnimationName();
    return TrayAnimation_IsRunning() &&
           (metric == ANIMATION_SPEED_CPU ||
            metric == ANIMATION_SPEED_MEMORY) &&
           GetAnimationType(animName) == ANIM_TYPE_CUSTOM &&
           !IsStaticImageFile(animName);
}

BOOL ShouldKeepSystemMonitorActive(void) {
    HWND hwnd = GetValidTrayMainWindow();
    if (!g_trayBackgroundWorkEnabled || !hwnd ||
        !IsTrayIconActiveForWindow(hwnd)) {
        return FALSE;
    }
    if (TaskbarMonitor_NeedsSystemMonitor()) return TRUE;
    if (IsTrayInteractionSuspended()) {
        return CurrentTrayIconNeedsSystemMonitor() ||
               CurrentAnimationSpeedNeedsSystemMonitor();
    }
    return IsTrayTooltipActive() || CurrentTrayIconNeedsSystemMonitor() ||
           CurrentAnimationSpeedNeedsSystemMonitor();
}

void EnsureTraySystemMonitorActive(void) {
    if (!g_traySystemMonitorActive) {
        SystemMonitor_Init();
        g_traySystemMonitorActive = TRUE;
    }
}

void ReleaseIdleTraySystemMonitor(void) {
    if (!ShouldKeepSystemMonitorActive() &&
        (g_traySystemMonitorActive || SystemMonitor_IsInitialized())) {
        SystemMonitor_Shutdown();
        g_traySystemMonitorActive = FALSE;
    }
}

BOOL ShouldRunTrayBackgroundTimer(HWND hwnd) {
    return g_trayBackgroundWorkEnabled &&
           IsTrayIconActiveForWindow(hwnd) &&
           (TaskbarMonitor_NeedsSystemMonitor() ||
            (!IsTrayInteractionSuspended() && IsTrayTooltipActive()) ||
            CurrentTrayIconNeedsBackgroundRefresh() ||
            TrayAnimation_HasDeferredIconUpdate());
}

void RefreshTrayBackgroundWorkState(void) {
    HWND hwnd = GetValidTrayMainWindow();
    BOOL shouldRun = hwnd && ShouldRunTrayBackgroundTimer(hwnd);
    BOOL keepSystemMonitor = ShouldKeepSystemMonitorActive();
    if (keepSystemMonitor) {
        EnsureTraySystemMonitorActive();
    }

    if (shouldRun) {
        if (!g_trayTipTimerActive) {
            if (SetTimer(hwnd, TRAY_TIP_TIMER_ID,
                         TOOLTIP_UPDATE_INTERVAL_MS,
                         (TIMERPROC)TrayTipTimerProc)) {
                g_trayTipTimerActive = TRUE;
            } else {
                LOG_WARNING("Failed to start tray background refresh timer (error=%lu)",
                            GetLastError());
            }
        }
        if (!keepSystemMonitor) {
            ReleaseIdleTraySystemMonitor();
        }
        return;
    }

    if (hwnd) {
        KillTimer(hwnd, TRAY_TIP_TIMER_ID);
    }
    g_trayTipTimerActive = FALSE;
    ReleaseIdleTraySystemMonitor();
}

void SetTrayTooltipActive(BOOL active) {
    HWND hwnd = GetValidTrayMainWindow();
    BOOL normalized = active && hwnd && IsTrayIconActiveForWindow(hwnd);
    BOOL wasActive = IsTrayTooltipActive();
    if (wasActive == normalized) {
        return;
    }

    InterlockedExchange(&g_trayTooltipActive, normalized ? 1L : 0L);
    if (normalized) {
        EnsureTraySystemMonitorActive();
        SystemMonitor_ForceRefresh();
    }
    RefreshTrayBackgroundWorkState();
    if (IsTrayTooltipActive() && hwnd) {
        TrayTipTimerProc(hwnd, WM_TIMER, TRAY_TIP_TIMER_ID, 0);
    } else if (wasActive && hwnd && IsTrayIconActiveForWindow(hwnd) &&
               !IsTrayInteractionSuspended()) {
        TrayAnimation_RefreshCurrentIcon();
        RefreshTrayBackgroundWorkState();
    }
}

BOOL IsTrayTooltipActive(void) {
    return InterlockedCompareExchange(&g_trayTooltipActive, 0, 0) != 0;
}

BOOL GetSystemMetricsSnapshot(DWORD fields,
                              SystemMonitorSnapshot* snapshot) {
    if (!snapshot) return FALSE;
    EnsureTraySystemMonitorActive();
    return SystemMonitor_GetSnapshot(fields, snapshot);
}
