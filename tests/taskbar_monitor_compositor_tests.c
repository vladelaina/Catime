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

static BOOL IsHighContrastActive(void) {
    HIGHCONTRASTW contrast = {0};
    contrast.cbSize = sizeof(contrast);
    return SystemParametersInfoW(
               SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
           (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

static void ExpectColorKey(HWND window, COLORREF expectedColor) {
    COLORREF color = 0;
    BYTE alpha = 0;
    DWORD flags = 0;
    Expect(GetLayeredWindowAttributes(window, &color, &alpha, &flags),
           "layered-window attributes were unavailable");
    Expect((flags & LWA_COLORKEY) != 0,
           "color-key composition was not active");
    Expect(color == expectedColor,
           "the active taskbar color key did not match the theme");
    if ((flags & LWA_ALPHA) != 0) {
        Expect(alpha == 255,
               "the taskbar monitor remained hidden after a color-key swap");
    }
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
                         BOOL darkMode, COLORREF expectedColor) {
    g_taskbarMonitor.darkMode = darkMode;
    BOOL presented = TaskbarMonitor_Present(window, target, metrics, 2);
    Expect(presented, "taskbar monitor frame presentation failed");
    Expect(g_taskbarMonitor.compositionMode ==
               TASKBAR_COMPOSITION_COLOR_KEY,
           "taskbar monitor unexpectedly abandoned color-key composition");
    if (presented) ExpectColorKey(window, expectedColor);
    return presented;
}

int main(void) {
    if (IsHighContrastActive()) {
        puts("taskbar compositor theme transition skipped in high contrast");
        return 0;
    }

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

    TaskbarMetricText metrics[2] = {0};
    SetMetric(&metrics[0], 0, L"CPU:", L"42%");
    SetMetric(&metrics[1], 1, L"MEM:", L"68%");

    if (target && font) {
        (void)PresentTheme(window, target, metrics, FALSE,
                           RGB(210, 210, 211));
        (void)PresentTheme(window, target, metrics, TRUE,
                           RGB(32, 32, 32));
        (void)PresentTheme(window, target, metrics, FALSE,
                           RGB(210, 210, 211));
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
