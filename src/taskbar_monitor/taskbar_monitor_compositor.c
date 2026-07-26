/**
 * @file taskbar_monitor_compositor.c
 * @brief Antialiased taskbar text composition with a Win7-safe fallback.
 */

#include "taskbar_monitor_internal.h"

#include "log.h"
#include "tray/tray_menu_theme.h"

static COLORREF GetMonitorTextColor(void) {
    HIGHCONTRASTW contrast = {0};
    contrast.cbSize = sizeof(contrast);
    BOOL highContrast = SystemParametersInfoW(
        SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
        (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    if (highContrast) return GetSysColor(COLOR_WINDOWTEXT);
    return IsApplicationDarkModeActive() ? RGB(255, 255, 255)
                                         : RGB(0, 0, 0);
}

static COLORREF GetMonitorMatteColor(COLORREF textColor) {
    unsigned int luminance = 299u * GetRValue(textColor) +
                             587u * GetGValue(textColor) +
                             114u * GetBValue(textColor);
    return luminance < 128000u ? RGB(210, 210, 211)
                               : RGB(32, 32, 32);
}

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

static void DrawMetricGrid(
    HDC dc, int width, int height,
    const TaskbarMetricText* metrics,
    int metricCount) {
    if (metricCount <= 0) return;
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
        return;
    }
    for (int i = 0; i < metricCount; ++i) {
        RECT cell = {0, height * i / metricCount,
                     width, height * (i + 1) / metricCount};
        DrawMetricRow(dc, &cell, &metrics[i]);
    }
}

static DWORD DibPixelFromColor(COLORREF color) {
    return ((DWORD)GetRValue(color) << 16) |
           ((DWORD)GetGValue(color) << 8) |
           (DWORD)GetBValue(color);
}

static void FillPixels(DWORD* pixels, size_t count, DWORD color) {
    for (size_t i = 0; i < count; ++i) pixels[i] = color;
}

static BYTE PixelCoverage(DWORD pixel) {
    unsigned int blue = pixel & 0xffu;
    unsigned int green = (pixel >> 8) & 0xffu;
    unsigned int red = (pixel >> 16) & 0xffu;
    unsigned int luminance = (77u * red + 150u * green +
                              29u * blue + 128u) >> 8;
    return (BYTE)(255u - luminance);
}

static void ApplyPerPixelAlpha(DWORD* pixels, size_t count,
                               COLORREF textColor) {
    BYTE red = GetRValue(textColor);
    BYTE green = GetGValue(textColor);
    BYTE blue = GetBValue(textColor);
    for (size_t i = 0; i < count; ++i) {
        BYTE alpha = PixelCoverage(pixels[i]);
        DWORD premultipliedRed = (red * alpha + 127u) / 255u;
        DWORD premultipliedGreen = (green * alpha + 127u) / 255u;
        DWORD premultipliedBlue = (blue * alpha + 127u) / 255u;
        pixels[i] = ((DWORD)alpha << 24) |
                    (premultipliedRed << 16) |
                    (premultipliedGreen << 8) |
                    premultipliedBlue;
    }
}

static BOOL PresentPerPixel(HWND window, HDC screenDc, HDC sourceDc,
                            int width, int height) {
    POINT source = {0, 0};
    SIZE size = {width, height};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    return UpdateLayeredWindow(window, screenDc, NULL, &size,
                               sourceDc, &source, 0, &blend, ULW_ALPHA);
}

BOOL TaskbarMonitor_Present(
    HWND window, HDC fallbackTarget,
    const TaskbarMetricText* metrics,
    int metricCount) {
    RECT client = {0};
    if (!GetClientRect(window, &client)) return FALSE;
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return FALSE;

    HDC screenDc = GetDC(NULL);
    HDC sourceDc = screenDc ? CreateCompatibleDC(screenDc) : NULL;
    BITMAPINFO bitmapInfo = {0};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    DWORD* pixels = NULL;
    HBITMAP bitmap = sourceDc ? CreateDIBSection(
        sourceDc, &bitmapInfo, DIB_RGB_COLORS, (void**)&pixels, NULL, 0) : NULL;
    if (!bitmap || !pixels) {
        if (bitmap) DeleteObject(bitmap);
        if (sourceDc) DeleteDC(sourceDc);
        if (screenDc) ReleaseDC(NULL, screenDc);
        return FALSE;
    }

    size_t pixelCount = (size_t)width * (size_t)height;
    HGDIOBJ oldBitmap = SelectObject(sourceDc, bitmap);
    HFONT font = g_taskbarMonitor.font
        ? g_taskbarMonitor.font : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HGDIOBJ oldFont = SelectObject(sourceDc, font);
    SetBkMode(sourceDc, TRANSPARENT);

    COLORREF textColor = GetMonitorTextColor();
    BOOL presented = FALSE;
    if (g_taskbarMonitor.compositionMode != TASKBAR_COMPOSITION_PER_PIXEL &&
        fallbackTarget) {
        COLORREF matteColor = GetMonitorMatteColor(textColor);
        FillPixels(pixels, pixelCount, DibPixelFromColor(matteColor));
        SetTextColor(sourceDc, textColor);
        DrawMetricGrid(sourceDc, width, height, metrics, metricCount);
        GdiFlush();
        if (SetLayeredWindowAttributes(
                window, matteColor, 255, LWA_COLORKEY)) {
            presented = BitBlt(fallbackTarget, 0, 0, width, height,
                               sourceDc, 0, 0, SRCCOPY);
            if (presented) {
                g_taskbarMonitor.compositionMode =
                    TASKBAR_COMPOSITION_COLOR_KEY;
            }
        } else {
            LOG_WARNING("Taskbar color-key composition unavailable; using per-pixel alpha (error=%lu)",
                        GetLastError());
            g_taskbarMonitor.compositionMode =
                TASKBAR_COMPOSITION_PER_PIXEL;
        }
    }
    if (!presented &&
        g_taskbarMonitor.compositionMode == TASKBAR_COMPOSITION_PER_PIXEL) {
        FillPixels(pixels, pixelCount, 0x00ffffffu);
        SetTextColor(sourceDc, RGB(0, 0, 0));
        DrawMetricGrid(sourceDc, width, height, metrics, metricCount);
        GdiFlush();
        ApplyPerPixelAlpha(pixels, pixelCount, textColor);
        presented = PresentPerPixel(window, screenDc, sourceDc, width, height);
    }

    SelectObject(sourceDc, oldFont);
    SelectObject(sourceDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(sourceDc);
    ReleaseDC(NULL, screenDc);
    return presented;
}
