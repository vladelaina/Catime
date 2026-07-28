/**
 * @file taskbar_monitor_compositor.c
 * @brief Antialiased taskbar text composition with a Win7-safe fallback.
 */

#include "taskbar_monitor_internal.h"

#include "drawing/system_ui_font.h"
#include "log.h"

static COLORREF GetMonitorTextColor(void) {
    return g_taskbarMonitor.textColor;
}

static COLORREF GetMonitorMatteColor(COLORREF textColor) {
    unsigned int luminance = 299u * GetRValue(textColor) +
                             587u * GetGValue(textColor) +
                             114u * GetBValue(textColor);
    return luminance < 128000u ? RGB(210, 210, 211)
                               : RGB(32, 32, 32);
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
    unsigned int average = (red + green + blue + 1u) / 3u;
    return (BYTE)(255u - average);
}

void TaskbarMonitor_ColorizeTextMask(
    DWORD* pixels, size_t count, COLORREF textColor) {
    if (!pixels) return;
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

void TaskbarMonitor_EnsureInteractiveAlpha(
    DWORD* pixels, size_t count, COLORREF textColor) {
    if (!pixels) return;
    COLORREF background = GetMonitorMatteColor(textColor);
    DWORD hitPixel = 0x01000000u;
    if (GetRValue(background) >= 128) hitPixel |= 0x00010000u;
    if (GetGValue(background) >= 128) hitPixel |= 0x00000100u;
    if (GetBValue(background) >= 128) hitPixel |= 0x00000001u;
    for (size_t i = 0; i < count; ++i) {
        if ((pixels[i] >> 24) == 0) pixels[i] = hitPixel;
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

static BOOL PresentColorKey(HWND window, HDC target, HDC source,
                            int width, int height, COLORREF matteColor) {
    COLORREF activeColor = 0;
    BYTE activeAlpha = 255;
    DWORD activeFlags = 0;
    BOOL hasActiveColor =
        g_taskbarMonitor.compositionMode == TASKBAR_COMPOSITION_COLOR_KEY &&
        GetLayeredWindowAttributes(
            window, &activeColor, &activeAlpha, &activeFlags) &&
        (activeFlags & LWA_COLORKEY) != 0;

    if (!hasActiveColor) {
        if (!SetLayeredWindowAttributes(
                window, matteColor, 255, LWA_COLORKEY)) return FALSE;
        return BitBlt(target, 0, 0, width, height,
                      source, 0, 0, SRCCOPY);
    }
    if (activeColor == matteColor) {
        return BitBlt(target, 0, 0, width, height,
                      source, 0, 0, SRCCOPY);
    }

    if (!SetLayeredWindowAttributes(
            window, activeColor, 0, LWA_COLORKEY | LWA_ALPHA)) {
        return FALSE;
    }
    if (!BitBlt(target, 0, 0, width, height,
                source, 0, 0, SRCCOPY)) {
        DWORD error = GetLastError();
        (void)SetLayeredWindowAttributes(
            window, activeColor, 255, LWA_COLORKEY | LWA_ALPHA);
        SetLastError(error);
        return FALSE;
    }
    return SetLayeredWindowAttributes(
        window, matteColor, 255, LWA_COLORKEY | LWA_ALPHA);
}

static void ResetLayeredComposition(HWND window) {
    LONG_PTR extendedStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    SetWindowLongPtrW(window, GWL_EXSTYLE,
                      extendedStyle & ~WS_EX_LAYERED);
    SetWindowLongPtrW(window, GWL_EXSTYLE,
                      extendedStyle | WS_EX_LAYERED);
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
    if (g_taskbarMonitor.compositionMode != TASKBAR_COMPOSITION_COLOR_KEY) {
        FillPixels(pixels, pixelCount, 0x00ffffffu);
        SetTextColor(sourceDc, RGB(0, 0, 0));
        TaskbarMonitor_DrawMetricGrid(
            sourceDc, width, height, metrics, metricCount);
        BOOL frameReady = GdiFlush();
        if (frameReady) {
            TaskbarMonitor_ColorizeTextMask(
                pixels, pixelCount, textColor);
            if (!g_taskbarMonitor.menuPreviewSessionActive) {
                TaskbarMonitor_EnsureInteractiveAlpha(
                    pixels, pixelCount, textColor);
            }
            presented = PresentPerPixel(
                window, screenDc, sourceDc, width, height);
        }
        if (presented) {
            g_taskbarMonitor.compositionMode =
                TASKBAR_COMPOSITION_PER_PIXEL;
        } else if (fallbackTarget) {
            LOG_WARNING("Taskbar per-pixel composition unavailable; using color-key fallback (error=%lu)",
                        GetLastError());
            ResetLayeredComposition(window);
            g_taskbarMonitor.compositionMode =
                TASKBAR_COMPOSITION_COLOR_KEY;
        }
    }
    if (!presented && fallbackTarget) {
        HFONT fallbackFont = CreateNonAntialiasedFontCopy(font);
        HGDIOBJ previousFont = fallbackFont
            ? SelectObject(sourceDc, fallbackFont) : NULL;
        BOOL fallbackReady = previousFont && previousFont != HGDI_ERROR;
        if (fallbackReady) {
            COLORREF matteColor = GetMonitorMatteColor(textColor);
            FillPixels(pixels, pixelCount, DibPixelFromColor(matteColor));
            SetTextColor(sourceDc, textColor);
            TaskbarMonitor_DrawMetricGrid(
                sourceDc, width, height, metrics, metricCount);
            BOOL frameReady = GdiFlush();
            if (frameReady) {
                presented = PresentColorKey(
                    window, fallbackTarget, sourceDc,
                    width, height, matteColor);
            }
            if (presented) {
                g_taskbarMonitor.compositionMode =
                    TASKBAR_COMPOSITION_COLOR_KEY;
            } else {
                LOG_WARNING("Taskbar color-key fallback unavailable (error=%lu)",
                            GetLastError());
                ResetLayeredComposition(window);
                g_taskbarMonitor.compositionMode =
                    TASKBAR_COMPOSITION_UNKNOWN;
            }
        } else {
            LOG_WARNING("Taskbar color-key fallback font unavailable");
            ResetLayeredComposition(window);
            g_taskbarMonitor.compositionMode =
                TASKBAR_COMPOSITION_UNKNOWN;
        }
        if (fallbackReady) SelectObject(sourceDc, previousFont);
        if (fallbackFont) DeleteObject(fallbackFont);
    }

    SelectObject(sourceDc, oldFont);
    SelectObject(sourceDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(sourceDc);
    ReleaseDC(NULL, screenDc);
    return presented;
}
