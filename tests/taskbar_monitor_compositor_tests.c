#include "log.h"
#include "taskbar_monitor/taskbar_monitor_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

TaskbarMonitorState g_taskbarMonitor = {0};

static int g_failures = 0;

static void Expect(BOOL condition, const char* message) {
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    ++g_failures;
}

void WriteLog(LogLevel level, const char* format, ...) {
    (void)level;
    (void)format;
}

int TaskbarMonitor_ScaleForDpi(int value, UINT dpi) {
    if (dpi == 0) dpi = 96;
    return MulDiv(value, (int)dpi, 96);
}

static void ExpectNoColorKey(HWND window) {
    COLORREF color = 0;
    BYTE alpha = 0;
    DWORD flags = 0;
    if (GetLayeredWindowAttributes(window, &color, &alpha, &flags)) {
        Expect((flags & LWA_COLORKEY) == 0,
               "per-pixel composition retained a color key");
    }
}

static void CheckSyntheticMaskPixels(void) {
    DWORD lightPixels[] = {0x00ffffffu, 0x00000000u, 0x00ff0000u};
    TaskbarMonitor_ColorizeTextMask(
        lightPixels, _countof(lightPixels), RGB(255, 255, 255));
    Expect(lightPixels[0] == 0,
           "a white mask background was not transparent");
    Expect(lightPixels[1] == 0xffffffffu,
           "a solid glyph pixel did not remain opaque white");
    Expect(lightPixels[2] == 0xaaaaaaaau,
           "a colored ClearType edge was not neutralized");

    DWORD darkPixel = 0x00ff0000u;
    TaskbarMonitor_ColorizeTextMask(&darkPixel, 1, RGB(0, 0, 0));
    Expect(darkPixel == 0xaa000000u,
           "a dark glyph edge was not premultiplied correctly");
}

static void CheckRenderedMaskPixels(void) {
    const int width = 180;
    const int height = 20;
    const wchar_t text[] =
        L"\x2191:78% \x5185\x5b58:68%";
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    DWORD* pixels = NULL;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bitmap = CreateDIBSection(
        NULL, &info, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
    HFONT font = CreateFontW(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    Expect(dc && bitmap && pixels && font,
           "failed to create the rendered mask test surface");
    if (!dc || !bitmap || !pixels || !font) goto cleanup;

    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    HGDIOBJ oldFont = SelectObject(dc, font);
    for (int i = 0; i < width * height; ++i) {
        pixels[i] = 0x00ffffffu;
    }
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));
    TextOutW(dc, 1, 1, text, (int)wcslen(text));
    GdiFlush();
    TaskbarMonitor_ColorizeTextMask(
        pixels, (size_t)width * height, RGB(255, 255, 255));

    BOOL neutral = TRUE;
    int coveredPixels = 0;
    for (int i = 0; i < width * height; ++i) {
        BYTE alpha = (BYTE)(pixels[i] >> 24);
        BYTE red = (BYTE)(pixels[i] >> 16);
        BYTE green = (BYTE)(pixels[i] >> 8);
        BYTE blue = (BYTE)pixels[i];
        if (alpha == 0) {
            if (pixels[i] != 0) neutral = FALSE;
            continue;
        }
        ++coveredPixels;
        if (red != alpha || green != alpha || blue != alpha) {
            neutral = FALSE;
        }
    }
    Expect(coveredPixels > 0, "the rendered glyph mask was empty");
    Expect(neutral, "the rendered glyph mask retained colored fringes");

    for (int i = 0; i < width * height; ++i) {
        pixels[i] = 0x00ffffffu;
    }
    TextOutW(dc, 1, 1, text, (int)wcslen(text));
    GdiFlush();
    TaskbarMonitor_ColorizeTextMask(
        pixels, (size_t)width * height, RGB(0, 0, 0));
    neutral = TRUE;
    coveredPixels = 0;
    for (int i = 0; i < width * height; ++i) {
        BYTE alpha = (BYTE)(pixels[i] >> 24);
        if (alpha == 0) {
            if (pixels[i] != 0) neutral = FALSE;
            continue;
        }
        ++coveredPixels;
        if ((pixels[i] & 0x00ffffffu) != 0) neutral = FALSE;
    }
    Expect(coveredPixels > 0, "the dark rendered glyph mask was empty");
    Expect(neutral, "the dark rendered glyph mask was not premultiplied");
    SelectObject(dc, oldFont);
    SelectObject(dc, oldBitmap);

cleanup:
    if (font) DeleteObject(font);
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
}

static void SetMetric(TaskbarMetricText* metric, int row,
                      const wchar_t* label, const wchar_t* value) {
    metric->group = TASKBAR_METRIC_GROUP_RESOURCE;
    metric->row = row;
    wcsncpy_s(metric->label, _countof(metric->label), label, _TRUNCATE);
    wcsncpy_s(metric->value, _countof(metric->value), value, _TRUNCATE);
}

static BOOL PresentTheme(HWND window, HDC target,
                         const TaskbarMetricText* metrics,
                         BOOL darkMode) {
    g_taskbarMonitor.textColor = darkMode
        ? RGB(255, 255, 255) : RGB(0, 0, 0);
    BOOL presented = TaskbarMonitor_Present(window, target, metrics, 2);
    Expect(presented, "taskbar monitor frame presentation failed");
    Expect(g_taskbarMonitor.compositionMode ==
               TASKBAR_COMPOSITION_PER_PIXEL,
           "taskbar monitor did not use per-pixel alpha composition");
    if (presented) ExpectNoColorKey(window);
    return presented;
}

int main(void) {
    CheckSyntheticMaskPixels();
    CheckRenderedMaskPixels();

    const wchar_t className[] = L"CatimeTaskbarCompositorTest";
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW windowClass = {0};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    Expect(RegisterClassW(&windowClass) != 0,
           "failed to register the compositor test window");

    HWND window = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        className, L"", WS_POPUP,
        -32000, -32000, 180, 36,
        NULL, NULL, instance, NULL);
    Expect(window != NULL, "failed to create the compositor test window");
    if (!window) return 1;

    ShowWindow(window, SW_SHOWNOACTIVATE);
    HDC target = GetDC(window);
    Expect(target != NULL, "failed to acquire the compositor test DC");

    HFONT font = CreateFontW(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    Expect(font != NULL, "failed to create the compositor test font");

    g_taskbarMonitor.window = window;
    g_taskbarMonitor.font = font;
    g_taskbarMonitor.dpi = 96;
    g_taskbarMonitor.horizontal = TRUE;
    g_taskbarMonitor.cpuMemoryEnabled = TRUE;
    g_taskbarMonitor.resourceLabelWidth = 48;
    g_taskbarMonitor.resourceGroupWidth = 180;
    g_taskbarMonitor.compositionMode = TASKBAR_COMPOSITION_UNKNOWN;

    TaskbarMetricText metrics[2] = {0};
    SetMetric(&metrics[0], 0, L"CPU:", L"42%");
    SetMetric(&metrics[1], 1, L"MEM:", L"68%");

    if (target && font) {
        for (int i = 0; i < 8; ++i) {
            (void)PresentTheme(window, target, metrics, (i & 1) != 0);
        }
    }

    if (target) ReleaseDC(window, target);
    if (font) DeleteObject(font);
    DestroyWindow(window);
    UnregisterClassW(className, instance);

    if (g_failures) {
        fprintf(stderr, "%d taskbar compositor test(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
