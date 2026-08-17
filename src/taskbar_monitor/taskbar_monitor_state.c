/**
 * @file taskbar_monitor_state.c
 * @brief Shared taskbar-monitor state, DPI, font, and geometry helpers.
 */

#include "taskbar_monitor.h"
#include "taskbar_monitor_internal.h"

#include "drawing/system_ui_font.h"
#include "language.h"
#include "tray/tray_theme_state.h"
#include "utils/win32_dynamic_loader.h"

#include <stdlib.h>
#include <wchar.h>

TaskbarMonitorState g_taskbarMonitor = {0};

typedef UINT (WINAPI* TaskbarGetDpiForWindowFunc)(HWND);
static TaskbarGetDpiForWindowFunc s_getDpiForWindow = NULL;
static BOOL s_getDpiForWindowResolved = FALSE;

int TaskbarMonitor_ScaleForDpi(int value, UINT dpi) {
    if (dpi == 0) dpi = 96;
    return MulDiv(value, (int)dpi, 96);
}

UINT TaskbarMonitor_GetWindowDpi(HWND window) {
    if (!s_getDpiForWindowResolved) {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32) {
            CATIME_LOAD_PROC_ADDRESS(
                user32, "GetDpiForWindow", s_getDpiForWindow);
        }
        s_getDpiForWindowResolved = TRUE;
    }
    if (window && s_getDpiForWindow) {
        UINT dpi = s_getDpiForWindow(window);
        if (dpi > 0) return dpi;
    }

    UINT dpi = 96;
    HDC dc = GetDC(window);
    if (dc) {
        int value = GetDeviceCaps(dc, LOGPIXELSX);
        if (value > 0) dpi = (UINT)value;
        ReleaseDC(window, dc);
    }
    return dpi;
}

BOOL TaskbarMonitor_GetWindowRectInParent(
    HWND window, HWND parent, RECT* output) {
    if (!window || !parent || !output || !GetWindowRect(window, output)) {
        return FALSE;
    }
    MapWindowPoints(HWND_DESKTOP, parent, (POINT*)output, 2);
    return TRUE;
}

BOOL TaskbarMonitor_RectsNearEqual(
    const RECT* first, const RECT* second) {
    const int tolerance = 2;
    if (!first || !second) return FALSE;
    return abs(first->left - second->left) <= tolerance &&
           abs(first->top - second->top) <= tolerance &&
           abs(first->right - second->right) <= tolerance &&
           abs(first->bottom - second->bottom) <= tolerance;
}

BOOL TaskbarMonitor_NeedsSystemMonitor(void) {
    return TaskbarMonitor_GetRequiredSnapshotFields() != 0 &&
           TaskbarMonitor_ShouldKeepSystemMonitorActive(
        g_taskbarMonitor.cpuMemoryEnabled,
        g_taskbarMonitor.networkEnabled,
        g_taskbarMonitor.menuPreviewSessionActive,
        g_taskbarMonitor.menuPreviewOriginalCpuMemoryEnabled,
        g_taskbarMonitor.menuPreviewOriginalNetworkEnabled);
}

DWORD TaskbarMonitor_GetRequiredSnapshotFields(void) {
    DWORD fields = TaskbarMonitor_GetSnapshotFields(
        g_taskbarMonitor.cpuMemoryEnabled,
        g_taskbarMonitor.networkEnabled);
    if (g_taskbarMonitor.menuPreviewSessionActive) {
        fields |= SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY |
                  SYSTEM_MONITOR_SNAPSHOT_NETWORK;
    }
    return fields;
}

void TaskbarMonitor_DeleteFont(void) {
    if (g_taskbarMonitor.font) {
        DeleteObject(g_taskbarMonitor.font);
        g_taskbarMonitor.font = NULL;
    }
}

void TaskbarMonitor_RecreateFont(void) {
    UINT dpi = g_taskbarMonitor.dpi ? g_taskbarMonitor.dpi : 96;
    BOOL cpuMemoryLayoutEnabled =
        g_taskbarMonitor.cpuMemoryEnabled ||
        g_taskbarMonitor.menuPreviewSessionActive;
    BOOL networkLayoutEnabled =
        g_taskbarMonitor.networkEnabled ||
        g_taskbarMonitor.menuPreviewSessionActive;
    int rowHeight = TaskbarMonitor_CalculateMetricRowHeight(
        g_taskbarMonitor.height,
        g_taskbarMonitor.horizontal,
        cpuMemoryLayoutEnabled, networkLayoutEnabled);
    TaskbarMonitor_DeleteFont();
    g_taskbarMonitor.font = CreateFittedSystemUiMetricTextFont(
        dpi, CLEARTYPE_QUALITY, rowHeight);
}

void TaskbarMonitor_UpdateThemeState(void) {
    g_taskbarMonitor.textColor = GetSystemMetricTextColor();
}

static int MeasureTextWidth(HDC dc, const wchar_t* text) {
    SIZE size = {0};
    if (!dc || !text || !text[0] ||
        !GetTextExtentPoint32W(dc, text, (int)wcslen(text), &size)) {
        return 0;
    }
    return size.cx;
}

static int MeasureWidestText(
    HDC dc, const wchar_t* const* texts, size_t textCount) {
    int widest = 0;
    for (size_t i = 0; i < textCount; ++i) {
        int width = MeasureTextWidth(dc, texts[i]);
        if (width > widest) widest = width;
    }
    return widest;
}

static void CopyLocalizedMetricLabel(
    wchar_t* output, size_t outputCount, const wchar_t* key) {
    const wchar_t* localized = GetLocalizedString(NULL, key);
    _snwprintf_s(output, outputCount, _TRUNCATE, L"%ls:", localized);
}

void TaskbarMonitor_RefreshTextLayout(void) {
    CopyLocalizedMetricLabel(
        g_taskbarMonitor.cpuLabel, _countof(g_taskbarMonitor.cpuLabel),
        L"Tray Tooltip CPU");
    CopyLocalizedMetricLabel(
        g_taskbarMonitor.memoryLabel,
        _countof(g_taskbarMonitor.memoryLabel),
        L"Tray Tooltip Memory");

    int padding = TaskbarMonitor_ScaleForDpi(
        TASKBAR_MONITOR_CELL_PADDING, g_taskbarMonitor.dpi);
    int columnGap = TaskbarMonitor_ScaleForDpi(
        TASKBAR_MONITOR_COLUMN_GAP, g_taskbarMonitor.dpi);
    HDC dc = GetDC(NULL);
    HFONT font = g_taskbarMonitor.font
        ? g_taskbarMonitor.font : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HGDIOBJ oldFont = dc ? SelectObject(dc, font) : NULL;

    const wchar_t* networkLabels[] = {L"\x2191:", L"\x2193:"};
    const wchar_t* networkValues[] = {
        L"99.9MB/s", L"1023KB/s", L"1023MB/s", L"1023GB/s"
    };
    const wchar_t* resourceLabels[] = {
        g_taskbarMonitor.cpuLabel, g_taskbarMonitor.memoryLabel
    };
    int networkLabelWidth = MeasureWidestText(
        dc, networkLabels, _countof(networkLabels));
    int networkValueWidth = MeasureWidestText(
        dc, networkValues, _countof(networkValues));
    int resourceLabelWidth = MeasureWidestText(
        dc, resourceLabels, _countof(resourceLabels));
    int resourceValueWidth = MeasureTextWidth(dc, L"100%");

    if (dc && oldFont) SelectObject(dc, oldFont);
    if (dc) ReleaseDC(NULL, dc);

    if (networkLabelWidth <= 0 || networkValueWidth <= 0) {
        networkLabelWidth = TaskbarMonitor_ScaleForDpi(
            10, g_taskbarMonitor.dpi);
        g_taskbarMonitor.networkGroupWidth = TaskbarMonitor_ScaleForDpi(
            TASKBAR_MONITOR_FALLBACK_NETWORK_WIDTH,
            g_taskbarMonitor.dpi);
    } else {
        g_taskbarMonitor.networkGroupWidth = padding * 2 +
            networkLabelWidth + columnGap + networkValueWidth;
    }
    if (resourceLabelWidth <= 0 || resourceValueWidth <= 0) {
        resourceLabelWidth = TaskbarMonitor_ScaleForDpi(
            36, g_taskbarMonitor.dpi);
        g_taskbarMonitor.resourceGroupWidth = TaskbarMonitor_ScaleForDpi(
            TASKBAR_MONITOR_FALLBACK_RESOURCE_WIDTH,
            g_taskbarMonitor.dpi);
    } else {
        g_taskbarMonitor.resourceGroupWidth = padding * 2 +
            resourceLabelWidth + columnGap + resourceValueWidth;
    }
    g_taskbarMonitor.networkLabelWidth = networkLabelWidth;
    g_taskbarMonitor.resourceLabelWidth = resourceLabelWidth;
}

void TaskbarMonitor_UpdateDimensions(const RECT* taskbarRect) {
    int taskbarWidth = taskbarRect->right - taskbarRect->left;
    int taskbarHeight = taskbarRect->bottom - taskbarRect->top;
    BOOL cpuMemoryLayoutEnabled =
        g_taskbarMonitor.cpuMemoryEnabled ||
        g_taskbarMonitor.menuPreviewSessionActive;
    BOOL networkLayoutEnabled =
        g_taskbarMonitor.networkEnabled ||
        g_taskbarMonitor.menuPreviewSessionActive;
    g_taskbarMonitor.taskbarWidth = taskbarWidth;
    g_taskbarMonitor.taskbarHeight = taskbarHeight;
    g_taskbarMonitor.horizontal = taskbarWidth >= taskbarHeight;
    if (!TaskbarMonitor_CalculateMonitorSize(
            taskbarWidth, taskbarHeight, g_taskbarMonitor.horizontal,
            g_taskbarMonitor.dpi, g_taskbarMonitor.networkGroupWidth,
            g_taskbarMonitor.resourceGroupWidth,
            cpuMemoryLayoutEnabled, networkLayoutEnabled,
            &g_taskbarMonitor.width, &g_taskbarMonitor.height)) {
        g_taskbarMonitor.width = taskbarWidth > 0 ? taskbarWidth : 1;
        g_taskbarMonitor.height = taskbarHeight > 0 ? taskbarHeight : 1;
    }
}
