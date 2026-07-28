/**
 * @file taskbar_monitor_layout.c
 * @brief Fixed metric-row layout for the taskbar monitor.
 */

#include "taskbar_monitor_internal.h"

static void DrawAlignedText(HDC dc, const RECT* textRect,
                            const wchar_t* text, UINT alignment) {
    RECT drawRect = *textRect;
    DrawTextW(dc, text, -1, &drawRect,
              alignment | DT_VCENTER | DT_SINGLELINE |
              DT_NOPREFIX | DT_END_ELLIPSIS);
}

static int GetMetricLabelWidth(TaskbarMetricGroup group) {
    return group == TASKBAR_METRIC_GROUP_NETWORK
        ? g_taskbarMonitor.networkLabelWidth
        : g_taskbarMonitor.resourceLabelWidth;
}

static void DrawMetricRow(HDC dc, const RECT* cell,
                          const TaskbarMetricText* metric) {
    RECT content = *cell;
    int padding = TaskbarMonitor_ScaleForDpi(
        TASKBAR_MONITOR_CELL_PADDING, g_taskbarMonitor.dpi);
    int columnGap = TaskbarMonitor_ScaleForDpi(
        TASKBAR_MONITOR_COLUMN_GAP, g_taskbarMonitor.dpi);
    content.left += padding;
    content.right -= padding;
    if (content.right <= content.left) return;

    int available = content.right - content.left;
    int labelWidth = GetMetricLabelWidth(metric->group);
    if (labelWidth > available - columnGap - 1) {
        labelWidth = available - columnGap - 1;
    }
    if (labelWidth < 1) labelWidth = 1;
    RECT labelRect = content;
    labelRect.right = labelRect.left + labelWidth;
    RECT valueRect = content;
    valueRect.left = labelRect.right + columnGap;
    DrawAlignedText(dc, &labelRect, metric->label, DT_LEFT);
    if (valueRect.right > valueRect.left) {
        UINT valueAlignment =
            metric->group == TASKBAR_METRIC_GROUP_NETWORK
                ? DT_RIGHT : DT_LEFT;
        DrawAlignedText(dc, &valueRect, metric->value, valueAlignment);
    }
}

static void GetHorizontalGroupBounds(TaskbarMetricGroup group, int width,
                                     int* left, int* right) {
    *left = 0;
    *right = width;
    if (!g_taskbarMonitor.networkEnabled ||
        !g_taskbarMonitor.cpuMemoryEnabled) return;
    int split = g_taskbarMonitor.networkGroupWidth;
    int groupGap = TaskbarMonitor_ScaleForDpi(
        TASKBAR_MONITOR_GROUP_GAP, g_taskbarMonitor.dpi);
    if (split <= 0 || split + groupGap >= width) split = width / 2;
    if (group == TASKBAR_METRIC_GROUP_NETWORK) {
        *right = split;
    } else {
        *left = split + groupGap;
    }
}

static void GetPreviewContentBounds(int width, int height, RECT* bounds) {
    SetRect(bounds, 0, 0, width, height);
    if (!g_taskbarMonitor.menuPreviewSessionActive) return;

    if (g_taskbarMonitor.horizontal) {
        int contentWidth = 0;
        if (g_taskbarMonitor.networkEnabled) {
            contentWidth += g_taskbarMonitor.networkGroupWidth;
        }
        if (g_taskbarMonitor.cpuMemoryEnabled) {
            contentWidth += g_taskbarMonitor.resourceGroupWidth;
        }
        if (g_taskbarMonitor.networkEnabled &&
            g_taskbarMonitor.cpuMemoryEnabled) {
            contentWidth += TaskbarMonitor_ScaleForDpi(
                TASKBAR_MONITOR_GROUP_GAP, g_taskbarMonitor.dpi);
        }
        if (contentWidth > 0 && contentWidth < width) {
            bounds->left = width - contentWidth;
        }
        return;
    }

    int groupCount = (g_taskbarMonitor.networkEnabled ? 1 : 0) +
                     (g_taskbarMonitor.cpuMemoryEnabled ? 1 : 0);
    int contentHeight = TaskbarMonitor_ScaleForDpi(
        TASKBAR_MONITOR_GROUP_HEIGHT * groupCount,
        g_taskbarMonitor.dpi);
    if (contentHeight > 0 && contentHeight < height) {
        bounds->top = height - contentHeight;
    }
}

void TaskbarMonitor_DrawMetricGrid(
    HDC dc, int width, int height,
    const TaskbarMetricText* metrics, int metricCount) {
    if (!dc || !metrics || metricCount <= 0) return;
    RECT content = {0};
    GetPreviewContentBounds(width, height, &content);
    int savedDc = 0;
    if (content.left != 0 || content.top != 0) {
        savedDc = SaveDC(dc);
    }
    if (savedDc != 0) {
        SetViewportOrgEx(dc, content.left, content.top, NULL);
        width = content.right - content.left;
        height = content.bottom - content.top;
    }
    if (g_taskbarMonitor.horizontal) {
        for (int i = 0; i < metricCount; ++i) {
            int left;
            int right;
            GetHorizontalGroupBounds(
                metrics[i].group, width, &left, &right);
            int row = metrics[i].row;
            RECT cell = {
                left,
                height * row / 2,
                right,
                height * (row + 1) / 2
            };
            DrawMetricRow(dc, &cell, &metrics[i]);
        }
    } else {
        for (int i = 0; i < metricCount; ++i) {
            RECT cell = {0, height * i / metricCount,
                         width, height * (i + 1) / metricCount};
            DrawMetricRow(dc, &cell, &metrics[i]);
        }
    }
    if (savedDc != 0) RestoreDC(dc, savedDc);
}
