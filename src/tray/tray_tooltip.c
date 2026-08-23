/**
 * @file tray_tooltip.c
 * @brief Tray tooltip formatting and periodic dynamic-icon refresh.
 */

#include "tray_internal.h"
#include "language.h"
#include "config.h"
#include "system_monitor.h"
#include "timer/timer.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_animation_loader.h"
#include "utils/network_rate.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

static void BuildBasicTooltip(
    wchar_t* tip, size_t tipSize,
    const SystemMonitorSnapshot* snapshot) {
    const wchar_t* cpuLabel = GetLocalizedString(NULL, L"Tray Tooltip CPU");
    const wchar_t* memoryLabel = GetLocalizedString(NULL, L"Tray Tooltip Memory");
    const wchar_t* uploadLabel = GetLocalizedString(NULL, L"Tray Tooltip Upload");
    const wchar_t* downloadLabel = GetLocalizedString(NULL, L"Tray Tooltip Download");

    wchar_t cpu[32] = L"0.0%";
    wchar_t memory[32] = L"0.0%";
    wchar_t upload[48] = L"0.0 KB/s";
    wchar_t download[48] = L"0.0 KB/s";
    if (snapshot && snapshot->cpuAvailable) {
        _snwprintf_s(cpu, _countof(cpu), _TRUNCATE,
                     L"%.1f%%", snapshot->cpuPercent);
    }
    if (snapshot && snapshot->memoryAvailable) {
        _snwprintf_s(memory, _countof(memory), _TRUNCATE,
                     L"%.1f%%", snapshot->memoryPercent);
    }
    if (snapshot && snapshot->networkAvailable) {
        FormattedNetworkRate uploadRate = FormatNetworkBytesPerSecond(
            (double)snapshot->uploadBytesPerSecond);
        FormattedNetworkRate downloadRate = FormatNetworkBytesPerSecond(
            (double)snapshot->downloadBytesPerSecond);
        _snwprintf_s(upload, _countof(upload), _TRUNCATE,
                     L"%.1f %ls", uploadRate.value, uploadRate.unit);
        _snwprintf_s(download, _countof(download), _TRUNCATE,
                     L"%.1f %ls", downloadRate.value, downloadRate.unit);
    }

    _snwprintf_s(tip, tipSize, _TRUNCATE,
                 L"%s %s\n%s %s\n%s %s\n%s %s",
                 cpuLabel, cpu, memoryLabel, memory,
                 uploadLabel, upload, downloadLabel, download);
}

static BOOL ShouldShowAnimationSpeed(const char* animName) {
    if (!animName || IsBuiltinAnimationName(animName) ||
        IsStaticImageFile(animName)) {
        return FALSE;
    }
    return TRUE;
}

static double CalculateSpeedMetricPercent(AnimationSpeedMetric metric,
                                          float cpu, float mem) {
    if (metric == ANIMATION_SPEED_CPU) {
        return (double)cpu;
    }
    if (metric == ANIMATION_SPEED_TIMER) {
        if (!CLOCK_SHOW_CURRENT_TIME && !CLOCK_COUNT_UP &&
            CLOCK_TOTAL_TIME > 0) {
            double percent = (double)countdown_elapsed_time /
                             (double)CLOCK_TOTAL_TIME;
            if (percent < 0.0) percent = 0.0;
            if (percent > 1.0) percent = 1.0;
            return percent * 100.0;
        }
        return 0.0;
    }
    if (metric == ANIMATION_SPEED_FIXED) {
        return 0.0;
    }
    return (double)mem;
}

static void AppendSpeedLine(wchar_t* tip, size_t tipSize,
                            AnimationSpeedMetric metric,
                            float cpu, float mem) {
    if (metric == ANIMATION_SPEED_ORIGINAL) {
        return;
    }
    if (metric == ANIMATION_SPEED_FIXED) {
        wchar_t extra[128];
        _snwprintf_s(extra, _countof(extra), _TRUNCATE,
                     L"\n%s · %s %.3gx",
                     GetLocalizedString(NULL, L"Tray Tooltip Animation Speed"),
                     GetLocalizedString(NULL, L"Tray Tooltip Fixed Metric"),
                     GetAnimationFixedSpeedMultiplier());
        wcsncat_s(tip, tipSize, extra, _TRUNCATE);
        return;
    }

    double percent = CalculateSpeedMetricPercent(metric, cpu, mem);
    BOOL applyScaling = TRUE;
    if (metric == ANIMATION_SPEED_TIMER &&
        (CLOCK_SHOW_CURRENT_TIME || CLOCK_COUNT_UP ||
         CLOCK_TOTAL_TIME <= 0 || percent >= 100.0)) {
        applyScaling = FALSE;
    }
    double scalePercent = GetAnimationSpeedScaleForPercent(
        applyScaling ? percent : 0.0);
    if (scalePercent <= 0.0) scalePercent = 100.0;

    const wchar_t* metricLabel =
        GetLocalizedString(NULL, L"Tray Tooltip Memory Metric");
    if (metric == ANIMATION_SPEED_CPU) {
        metricLabel = GetLocalizedString(NULL, L"Tray Tooltip CPU Metric");
    } else if (metric == ANIMATION_SPEED_TIMER) {
        metricLabel = GetLocalizedString(NULL, L"Tray Tooltip Timer Metric");
    }

    wchar_t extra[128];
    _snwprintf_s(extra, _countof(extra), _TRUNCATE,
                 L"\n%s · %s: %.0f%%",
                 GetLocalizedString(NULL, L"Tray Tooltip Animation Speed"),
                 metricLabel, scalePercent);
    wcsncat_s(tip, tipSize, extra, _TRUNCATE);
}

static void AppendUptimeLine(wchar_t* tip, size_t tipSize) {
    ULONGLONG totalMinutes = GetTickCount64() / 60000ULL;
    ULONGLONG days = totalMinutes / (24ULL * 60ULL);
    ULONGLONG hours = (totalMinutes / 60ULL) % 24ULL;
    ULONGLONG minutes = totalMinutes % 60ULL;
    wchar_t extra[64];
    if (days > 0ULL) {
        _snwprintf_s(extra, _countof(extra), _TRUNCATE,
                     GetLocalizedString(NULL, L"Tray Tooltip Uptime Days"),
                     days, hours, minutes);
    } else if (hours > 0ULL) {
        _snwprintf_s(extra, _countof(extra), _TRUNCATE,
                     GetLocalizedString(NULL, L"Tray Tooltip Uptime Hours"),
                     hours, minutes);
    } else {
        _snwprintf_s(extra, _countof(extra), _TRUNCATE,
                     GetLocalizedString(NULL, L"Tray Tooltip Uptime Minutes"),
                     minutes);
    }
    wcsncat_s(tip, tipSize, extra, _TRUNCATE);
}

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
        ReportTrayIconModifySuccess(owner);
        wcscpy_s(g_lastTrayTooltip, _countof(g_lastTrayTooltip), n.szTip);
    } else {
        ReportTrayIconModifyFailure(owner);
    }
}

void CALLBACK TrayTipTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)time;
    if (msg != WM_TIMER || id != TRAY_TIP_TIMER_ID) {
        return;
    }
    if (!IsTrayIconActiveForWindow(hwnd)) {
        if (hwnd) KillTimer(hwnd, TRAY_TIP_TIMER_ID);
        g_trayTipTimerActive = FALSE;
        return;
    }

    BOOL interactionSuspended = IsTrayInteractionSuspended();
    BOOL dynamicIcon = CurrentTrayIconNeedsBackgroundRefresh();
    BOOL tooltipActive = IsTrayTooltipActive();
    BOOL iconNeedsSystemMonitor =
        CurrentTrayIconNeedsSystemMonitor();
    SystemMonitorSnapshot snapshot = {0};
    const SystemMonitorSnapshot* sharedSnapshot =
        TrayMetricSync_GetSnapshot(
            tooltipActive, iconNeedsSystemMonitor, &snapshot);
    if (interactionSuspended || g_showingOpacityTip) {
        TrayMetricSync_UpdateIcon(
            dynamicIcon, iconNeedsSystemMonitor, sharedSnapshot);
        RefreshTrayBackgroundWorkState();
        return;
    }

    if (!tooltipActive) {
        BOOL flushedDeferredIcon = TrayAnimation_HasDeferredIconUpdate();
        if (flushedDeferredIcon) {
            TrayAnimation_RefreshCurrentIcon();
        }
        if (dynamicIcon && !flushedDeferredIcon) {
            TrayMetricSync_UpdateIcon(
                dynamicIcon, iconNeedsSystemMonitor, sharedSnapshot);
        }
        RefreshTrayBackgroundWorkState();
        return;
    }

    wchar_t tip[256] = {0};
    BuildBasicTooltip(tip, _countof(tip), sharedSnapshot);

    const char* animName = GetCurrentAnimationName();
    AnimationSpeedMetric metric = ANIMATION_SPEED_ORIGINAL;
    BOOL showAnimationSpeed = FALSE;
    if (ShouldShowAnimationSpeed(animName)) {
        metric = GetAnimationSpeedMetric();
        showAnimationSpeed = metric != ANIMATION_SPEED_ORIGINAL;
    }
    AppendUptimeLine(tip, _countof(tip));
    if (showAnimationSpeed) {
        float cpu = sharedSnapshot ? sharedSnapshot->cpuPercent : 0.0f;
        float memory = sharedSnapshot
            ? sharedSnapshot->memoryPercent : 0.0f;
        AppendSpeedLine(tip, _countof(tip), metric, cpu, memory);
    }

    if (!TrayMetricSync_UpdateIconAndTooltip(
            dynamicIcon, iconNeedsSystemMonitor,
            sharedSnapshot, tip)) {
        UpdateTrayTooltip(tip);
    }
    RefreshTrayBackgroundWorkState();
}
