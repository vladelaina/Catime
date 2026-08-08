#include "log.h"
#include "taskbar_monitor/taskbar_monitor_internal.h"

#include <stdarg.h>
#include <stdio.h>

TaskbarMonitorState g_taskbarMonitor = {0};

static int g_failures = 0;
static int g_frameMessages = 0;

static void Expect(BOOL condition, const char* message) {
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    ++g_failures;
}

void WriteLog(LogLevel level, const char* format, ...) {
    (void)level;
    (void)format;
}

static LRESULT CALLBACK ParentTestWindowProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCALCSIZE || message == WM_WINDOWPOSCHANGED) {
        ++g_frameMessages;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static HWND CreateTestWindow(
    HINSTANCE instance, const wchar_t* className) {
    return CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        className, L"", WS_POPUP,
        0, 0, 32, 32, NULL, NULL, instance, NULL);
}

int main(void) {
    const wchar_t className[] = L"CatimeTaskbarParentTest";
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW windowClass = {0};
    windowClass.lpfnWndProc = ParentTestWindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    Expect(RegisterClassW(&windowClass) != 0,
           "failed to register taskbar parent test class");

    HWND firstParent = CreateTestWindow(instance, className);
    HWND secondParent = CreateTestWindow(instance, className);
    HWND monitor = CreateTestWindow(instance, className);
    Expect(firstParent && secondParent && monitor,
           "failed to create taskbar parent test windows");
    if (!firstParent || !secondParent || !monitor) goto cleanup;

    g_taskbarMonitor.window = monitor;
    Expect(TaskbarMonitor_SetWindowParent(firstParent, FALSE),
           "initial taskbar parent transition failed");
    Expect(GetAncestor(monitor, GA_PARENT) == firstParent,
           "initial taskbar parent was not applied");
    LONG_PTR style = GetWindowLongPtrW(monitor, GWL_STYLE);
    Expect((style & WS_POPUP) != 0 && (style & WS_CHILD) == 0,
           "taskbar popup style was not applied");
    Expect((GetWindowLongPtrW(monitor, GWL_EXSTYLE) &
            WS_EX_TRANSPARENT) != 0,
           "taskbar monitor did not enable mouse pass-through");

    g_frameMessages = 0;
    Expect(TaskbarMonitor_SetWindowParent(firstParent, FALSE),
           "idempotent taskbar parent update failed");
    Expect(g_frameMessages == 0,
           "idempotent taskbar parent update refreshed the frame");

    g_frameMessages = 0;
    Expect(TaskbarMonitor_SetWindowParent(secondParent, FALSE),
           "changed taskbar parent transition failed");
    Expect(GetAncestor(monitor, GA_PARENT) == secondParent,
           "changed taskbar parent was not applied");
    Expect(g_frameMessages > 0,
           "changed taskbar parent did not synchronize the frame");

    Expect(TaskbarMonitor_SetWindowParent(secondParent, TRUE),
           "failed to prepare child z-order test window");
    Expect((GetWindowLongPtrW(monitor, GWL_EXSTYLE) &
            WS_EX_TRANSPARENT) != 0,
           "child transition disabled mouse pass-through");
    HWND sibling = CreateWindowExW(
        0, className, L"", WS_CHILD,
        0, 0, 16, 16, secondParent, NULL, instance, NULL);
    Expect(sibling != NULL, "failed to create z-order test sibling");
    if (sibling) {
        SetWindowPos(sibling, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        Expect(GetWindow(monitor, GW_HWNDPREV) != NULL,
               "z-order test sibling was not above the monitor");
        Expect(TaskbarMonitor_EnsureWindowAtTop(),
               "failed to restore taskbar monitor z-order");
        Expect(GetWindow(monitor, GW_HWNDPREV) == NULL,
               "taskbar monitor did not move to the top");
        g_frameMessages = 0;
        Expect(TaskbarMonitor_EnsureWindowAtTop(),
               "idempotent taskbar z-order check failed");
        Expect(g_frameMessages == 0,
               "idempotent taskbar z-order check moved the window");
        DestroyWindow(sibling);
    }

cleanup:
    if (monitor) DestroyWindow(monitor);
    if (secondParent) DestroyWindow(secondParent);
    if (firstParent) DestroyWindow(firstParent);
    UnregisterClassW(className, instance);
    return g_failures ? 1 : 0;
}
