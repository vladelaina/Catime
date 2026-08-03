/**
 * @file taskbar_monitor_render.c
 * @brief Metric sampling, formatting, painting, and window messages.
 */

#include "taskbar_monitor_internal.h"

#include "system_monitor.h"
#include "log.h"
#include "tray/tray_events.h"

#include <stdio.h>
#include <wchar.h>

void TaskbarMonitor_UpdateSnapshot(
    const SystemMonitorSnapshot* snapshot) {
    if (!snapshot) return;
    SystemMonitorSnapshot next = *snapshot;
    if (g_taskbarMonitor.menuPreviewSessionActive) {
        if (!next.cpuAvailable &&
            g_taskbarMonitor.metrics.cpuAvailable) {
            next.cpuPercent = g_taskbarMonitor.metrics.cpuPercent;
            next.cpuAvailable = TRUE;
            next.basicSampleTick =
                g_taskbarMonitor.metrics.basicSampleTick;
        }
        if (!next.memoryAvailable &&
            g_taskbarMonitor.metrics.memoryAvailable) {
            next.memoryPercent = g_taskbarMonitor.metrics.memoryPercent;
            next.memoryAvailable = TRUE;
            next.basicSampleTick =
                g_taskbarMonitor.metrics.basicSampleTick;
        }
        if (!next.networkAvailable &&
            g_taskbarMonitor.metrics.networkAvailable) {
            next.uploadBytesPerSecond =
                g_taskbarMonitor.metrics.uploadBytesPerSecond;
            next.downloadBytesPerSecond =
                g_taskbarMonitor.metrics.downloadBytesPerSecond;
            next.networkAvailable = TRUE;
            next.networkSampleTick =
                g_taskbarMonitor.metrics.networkSampleTick;
        }
    }
    if (IsWindow(g_taskbarMonitor.window) &&
        !g_taskbarMonitor.presentTimerActive) {
        TaskbarMonitor_RefreshAttachment();
    }
    if (TaskbarMonitor_SnapshotsEqual(
            &next, &g_taskbarMonitor.metrics)) return;
    g_taskbarMonitor.metrics = next;
    if (IsWindow(g_taskbarMonitor.window)) {
        InvalidateRect(g_taskbarMonitor.window, NULL, FALSE);
    }
}

void TaskbarMonitor_PrefetchSnapshot(DWORD fields, BOOL forceRefresh) {
    if (fields == 0) return;
    SystemMonitor_Init();
    if (forceRefresh) SystemMonitor_ForceRefresh();
    SystemMonitorSnapshot snapshot = {0};
    if (SystemMonitor_GetSnapshot(fields, &snapshot)) {
        TaskbarMonitor_UpdateSnapshot(&snapshot);
    }
}

static void FormatRate(float bytesPerSecond, BOOL available,
                       wchar_t* output, size_t outputCount) {
    double value = bytesPerSecond;
    const wchar_t* unit = L"K/s";
    if (!output || outputCount == 0) return;
    if (!available) {
        wcsncpy_s(output, outputCount, L"0.0K/s", _TRUNCATE);
        return;
    }
    if (value < 0.0) value = 0.0;
    value /= 1024.0;
    if (value >= 1024.0) {
        value /= 1024.0;
        unit = L"M/s";
    }
    if (value >= 1024.0) {
        value /= 1024.0;
        unit = L"G/s";
    }
    _snwprintf_s(output, outputCount, _TRUNCATE,
                 value >= 100.0 ? L"%.0f%ls" : L"%.1f%ls",
                 value, unit);
}

static void SetMetricText(
    TaskbarMetricText* metric, TaskbarMetricGroup group, int row,
    const wchar_t* label, const wchar_t* value) {
    if (!metric) return;
    metric->group = group;
    metric->row = row;
    wcsncpy_s(metric->label, _countof(metric->label), label, _TRUNCATE);
    wcsncpy_s(metric->value, _countof(metric->value), value, _TRUNCATE);
}

static int RoundPercent(float percent) {
    if (percent < 0.0f) return 0;
    if (percent > 100.0f) return 100;
    return (int)(percent + 0.5f);
}

static int BuildMetricTexts(TaskbarMetricText* metrics) {
    int metricCount = 0;
    if (g_taskbarMonitor.networkEnabled) {
        wchar_t upload[32] = {0};
        wchar_t download[32] = {0};
        FormatRate(g_taskbarMonitor.metrics.uploadBytesPerSecond,
                   g_taskbarMonitor.metrics.networkAvailable,
                   upload, _countof(upload));
        FormatRate(g_taskbarMonitor.metrics.downloadBytesPerSecond,
                   g_taskbarMonitor.metrics.networkAvailable,
                   download, _countof(download));
        SetMetricText(&metrics[metricCount], TASKBAR_METRIC_GROUP_NETWORK,
                      0, L"\x2191:", upload);
        ++metricCount;
        SetMetricText(&metrics[metricCount], TASKBAR_METRIC_GROUP_NETWORK,
                      1, L"\x2193:", download);
        ++metricCount;
    }
    if (g_taskbarMonitor.cpuMemoryEnabled) {
        wchar_t cpu[TASKBAR_MONITOR_VALUE_LENGTH] = {0};
        wchar_t memory[TASKBAR_MONITOR_VALUE_LENGTH] = {0};
        if (g_taskbarMonitor.metrics.cpuAvailable) {
            _snwprintf_s(cpu, _countof(cpu), _TRUNCATE, L"%d%%",
                         RoundPercent(g_taskbarMonitor.metrics.cpuPercent));
        } else {
            wcsncpy_s(cpu, _countof(cpu), L"0%", _TRUNCATE);
        }
        if (g_taskbarMonitor.metrics.memoryAvailable) {
            _snwprintf_s(
                memory, _countof(memory), _TRUNCATE, L"%d%%",
                RoundPercent(g_taskbarMonitor.metrics.memoryPercent));
        } else {
            wcsncpy_s(memory, _countof(memory), L"0%", _TRUNCATE);
        }
        SetMetricText(&metrics[metricCount], TASKBAR_METRIC_GROUP_RESOURCE,
                      0, g_taskbarMonitor.cpuLabel, cpu);
        ++metricCount;
        SetMetricText(&metrics[metricCount], TASKBAR_METRIC_GROUP_RESOURCE,
                      1, g_taskbarMonitor.memoryLabel, memory);
        ++metricCount;
    }
    return metricCount;
}

static void PaintMonitor(HWND window) {
    PAINTSTRUCT paint = {0};
    HDC target = BeginPaint(window, &paint);
    if (!target) return;
    TaskbarMetricText metrics[TASKBAR_MONITOR_MAX_METRICS] = {0};
    int metricCount = BuildMetricTexts(metrics);
    (void)TaskbarMonitor_Present(
        window, target, metrics, metricCount);
    EndPaint(window, &paint);
}

LRESULT CALLBACK TaskbarMonitorWindowProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    switch (message) {
        case WM_CREATE:
            g_taskbarMonitor.presentTimerActive = SetTimer(
                window, TASKBAR_MONITOR_PRESENT_TIMER_ID,
                TASKBAR_MONITOR_PRESENT_MS, NULL) != 0;
            if (!g_taskbarMonitor.presentTimerActive) {
                LOG_WARNING("Failed to start taskbar monitor placement timer (error=%lu)",
                            GetLastError());
            }
            {
                DWORD fields = TaskbarMonitor_GetSnapshotFields(
                    g_taskbarMonitor.cpuMemoryEnabled,
                    g_taskbarMonitor.networkEnabled);
                if (fields != 0) {
                    SystemMonitorSnapshot snapshot = {0};
                    if (SystemMonitor_GetSnapshot(fields, &snapshot)) {
                        TaskbarMonitor_UpdateSnapshot(&snapshot);
                    }
                }
            }
            return 0;
        case WM_TIMER:
            if (wParam == TASKBAR_MONITOR_THEME_RECHECK_TIMER_ID) {
                if ((LONG)(GetTickCount() -
                           g_taskbarMonitor.themeRecheckDueTick) < 0) {
                    return 0;
                }
                KillTimer(window,
                          TASKBAR_MONITOR_THEME_RECHECK_TIMER_ID);
                g_taskbarMonitor.themeRecheckDueTick = 0;
                TaskbarMonitor_UpdateThemeState();
                InvalidateRect(window, NULL, FALSE);
                return 0;
            }
            if (wParam == TASKBAR_MONITOR_PRESENT_TIMER_ID) {
                TaskbarMonitor_RefreshAttachment();
                if (g_taskbarMonitor.mode == TASKBAR_HOST_MODERN &&
                    !g_taskbarMonitor.menuPreviewSessionActive) {
                    (void)TaskbarMonitor_EnsureWindowAtTop();
                }
                return 0;
            }
            break;
        case WM_PAINT:
            PaintMonitor(window);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_NCHITTEST:
            return g_taskbarMonitor.menuPreviewSessionActive
                ? HTTRANSPARENT : HTCLIENT;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_LBUTTONUP:
            if (IsWindow(g_taskbarMonitor.owner)) {
                (void)HandleTrayMenuClick(
                    g_taskbarMonitor.owner, WM_LBUTTONUP);
            }
            return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            if (IsWindow(g_taskbarMonitor.owner)) {
                (void)HandleTrayMenuClick(
                    g_taskbarMonitor.owner, WM_RBUTTONUP);
            }
            return 0;
        case WM_NCDESTROY:
            KillTimer(window, TASKBAR_MONITOR_PRESENT_TIMER_ID);
            g_taskbarMonitor.presentTimerActive = FALSE;
            KillTimer(window,
                      TASKBAR_MONITOR_THEME_RECHECK_TIMER_ID);
            g_taskbarMonitor.themeRecheckDueTick = 0;
            TaskbarMonitor_OnMonitorWindowDestroyed(window);
            break;
        default:
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
