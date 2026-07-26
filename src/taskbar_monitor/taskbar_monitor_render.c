/**
 * @file taskbar_monitor_render.c
 * @brief Metric sampling, formatting, painting, and window messages.
 */

#include "taskbar_monitor_internal.h"

#include "system_monitor.h"
#include "tray/tray_menu.h"

#include <stdio.h>
#include <wchar.h>

void TaskbarMonitor_UpdateSnapshot(
    const SystemMonitorSnapshot* snapshot) {
    if (!snapshot) return;
    g_taskbarMonitor.metrics = *snapshot;
    if (IsWindow(g_taskbarMonitor.window)) {
        if (IsWindow(g_taskbarMonitor.taskbar)) {
            TaskbarMonitor_RefreshAttachment();
        }
        InvalidateRect(g_taskbarMonitor.window, NULL, FALSE);
    }
}

static void FormatRate(float bytesPerSecond, BOOL available,
                       wchar_t* output, size_t outputCount) {
    double value = bytesPerSecond;
    const wchar_t* unit = L"K/s";
    if (!output || outputCount == 0) return;
    if (!available) {
        wcsncpy_s(output, outputCount, L"--", _TRUNCATE);
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
            wcsncpy_s(cpu, _countof(cpu), L"--", _TRUNCATE);
        }
        if (g_taskbarMonitor.metrics.memoryAvailable) {
            _snwprintf_s(
                memory, _countof(memory), _TRUNCATE, L"%d%%",
                RoundPercent(g_taskbarMonitor.metrics.memoryPercent));
        } else {
            wcsncpy_s(memory, _countof(memory), L"--", _TRUNCATE);
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
    TaskbarMonitor_Present(window, target, metrics, metricCount);
    EndPaint(window, &paint);
}

LRESULT CALLBACK TaskbarMonitorWindowProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    switch (message) {
        case WM_CREATE:
            SetTimer(window, TASKBAR_MONITOR_PRESENT_TIMER_ID,
                     TASKBAR_MONITOR_PRESENT_MS, NULL);
            {
                DWORD fields = 0;
                if (g_taskbarMonitor.cpuMemoryEnabled) {
                    fields |= SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY;
                }
                if (g_taskbarMonitor.networkEnabled) {
                    fields |= SYSTEM_MONITOR_SNAPSHOT_NETWORK;
                }
                SystemMonitorSnapshot snapshot = {0};
                if (SystemMonitor_GetSnapshot(fields, &snapshot)) {
                    TaskbarMonitor_UpdateSnapshot(&snapshot);
                }
            }
            return 0;
        case WM_TIMER:
            if (wParam == TASKBAR_MONITOR_PRESENT_TIMER_ID) {
                if (g_taskbarMonitor.mode == TASKBAR_HOST_MODERN) {
                    SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
                return 0;
            }
            break;
        case WM_PAINT:
            PaintMonitor(window);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            if (IsWindow(g_taskbarMonitor.owner)) {
                ShowColorMenu(g_taskbarMonitor.owner);
            }
            return 0;
        case WM_NCDESTROY:
            KillTimer(window, TASKBAR_MONITOR_PRESENT_TIMER_ID);
            if (g_taskbarMonitor.window == window) {
                g_taskbarMonitor.window = NULL;
            }
            break;
        default:
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
