/**
 * @file taskbar_monitor_render.c
 * @brief Metric sampling, formatting, painting, and window messages.
 */

#include "taskbar_monitor_internal.h"

#include "system_monitor.h"
#include "tray/tray_menu.h"

#include <stdio.h>
#include <wchar.h>

static void UpdateMetricSnapshot(void) {
    if (g_taskbarMonitor.cpuMemoryEnabled) {
        float cpu = g_taskbarMonitor.cpuPercent;
        float memory = g_taskbarMonitor.memoryPercent;
        if (SystemMonitor_GetUsage(&cpu, &memory)) {
            g_taskbarMonitor.cpuPercent = cpu;
            g_taskbarMonitor.memoryPercent = memory;
        }
    }
    if (g_taskbarMonitor.networkEnabled) {
        float upload = 0.0f;
        float download = 0.0f;
        g_taskbarMonitor.networkAvailable = SystemMonitor_GetNetSpeed(
            &upload, &download);
        if (g_taskbarMonitor.networkAvailable) {
            g_taskbarMonitor.uploadBytesPerSecond = upload;
            g_taskbarMonitor.downloadBytesPerSecond = download;
        }
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

static int BuildMetricTexts(TaskbarMetricText* metrics) {
    int metricCount = 0;
    if (g_taskbarMonitor.networkEnabled) {
        wchar_t upload[32] = {0};
        wchar_t download[32] = {0};
        FormatRate(g_taskbarMonitor.uploadBytesPerSecond,
                   g_taskbarMonitor.networkAvailable,
                   upload, _countof(upload));
        FormatRate(g_taskbarMonitor.downloadBytesPerSecond,
                   g_taskbarMonitor.networkAvailable,
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
        _snwprintf_s(cpu, _countof(cpu), _TRUNCATE, L"%.0f%%",
                     g_taskbarMonitor.cpuPercent);
        _snwprintf_s(memory, _countof(memory), _TRUNCATE, L"%.0f%%",
                     g_taskbarMonitor.memoryPercent);
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
            SetTimer(window, TASKBAR_MONITOR_METRIC_TIMER_ID,
                     TASKBAR_MONITOR_REFRESH_MS, NULL);
            SetTimer(window, TASKBAR_MONITOR_PRESENT_TIMER_ID,
                     TASKBAR_MONITOR_PRESENT_MS, NULL);
            UpdateMetricSnapshot();
            return 0;
        case WM_TIMER:
            if (wParam == TASKBAR_MONITOR_PRESENT_TIMER_ID) {
                if (g_taskbarMonitor.mode == TASKBAR_HOST_MODERN) {
                    SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
                return 0;
            }
            if (wParam == TASKBAR_MONITOR_METRIC_TIMER_ID) {
                TaskbarMonitor_RefreshAttachment();
                UpdateMetricSnapshot();
                InvalidateRect(window, NULL, FALSE);
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
            KillTimer(window, TASKBAR_MONITOR_METRIC_TIMER_ID);
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
