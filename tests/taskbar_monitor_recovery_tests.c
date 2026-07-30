#include "log.h"
#include "taskbar_monitor.h"
#include "taskbar_monitor/taskbar_monitor_internal.h"

#include <stdarg.h>
#include <stdio.h>

TaskbarMonitorState g_taskbarMonitor = {0};

static int g_failures = 0;
static int g_attachCalls = 0;
static int g_createCalls = 0;
static int g_restoreCalls = 0;
static BOOL g_attachSucceeds = FALSE;

static void Expect(BOOL condition, const char* message) {
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    ++g_failures;
}

void WriteLog(LogLevel level, const char* format, ...) {
    (void)level;
    (void)format;
}

BOOL TaskbarMonitor_IsEnabled(void) {
    return g_taskbarMonitor.cpuMemoryEnabled ||
           g_taskbarMonitor.networkEnabled;
}

BOOL TaskbarMonitor_AttachToTaskbar(void) {
    ++g_attachCalls;
    if (!g_attachSucceeds || !IsWindow(g_taskbarMonitor.window)) {
        return FALSE;
    }
    g_taskbarMonitor.modernTaskbar = FALSE;
    if (!TaskbarMonitor_SetWindowParent(
            g_taskbarMonitor.owner, FALSE)) return FALSE;
    g_taskbarMonitor.taskbar = g_taskbarMonitor.owner;
    g_taskbarMonitor.host = g_taskbarMonitor.owner;
    g_taskbarMonitor.taskList = NULL;
    g_taskbarMonitor.taskListReserved = FALSE;
    g_taskbarMonitor.mode = TASKBAR_HOST_MODERN;
    ShowWindow(g_taskbarMonitor.window, SW_SHOWNOACTIVATE);
    return TRUE;
}

BOOL TaskbarMonitor_CreateWindow(void) {
    ++g_createCalls;
    return FALSE;
}

void TaskbarMonitor_RestoreClassicTaskList(void) {
    ++g_restoreCalls;
    g_taskbarMonitor.taskListReserved = FALSE;
}

static void PumpMessagesFor(DWORD durationMs) {
    DWORD startedAt = GetTickCount();
    do {
        MSG message;
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(10);
    } while ((DWORD)(GetTickCount() - startedAt) < durationMs);
}

static HWND CreateTestWindow(
    HINSTANCE instance, const wchar_t* className) {
    return CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        className, L"", WS_POPUP,
        0, 0, 32, 32, NULL, NULL, instance, NULL);
}

int main(void) {
    const wchar_t className[] = L"CatimeTaskbarRecoveryTest";
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW windowClass = {0};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    Expect(RegisterClassW(&windowClass) != 0,
           "failed to register taskbar recovery test class");

    HWND owner = CreateTestWindow(instance, className);
    HWND monitor = CreateTestWindow(instance, className);
    Expect(owner && monitor,
           "failed to create taskbar recovery test windows");
    if (!owner || !monitor) goto cleanup;

    g_taskbarMonitor.initialized = TRUE;
    g_taskbarMonitor.owner = owner;
    g_taskbarMonitor.window = monitor;
    g_taskbarMonitor.cpuMemoryEnabled = TRUE;
    g_taskbarMonitor.taskbar = owner;
    g_taskbarMonitor.host = owner;
    g_taskbarMonitor.mode = TASKBAR_HOST_MODERN;

    Expect(!TaskbarMonitor_HasUsableWindow(),
           "hidden taskbar monitor was treated as usable");
    ShowWindow(monitor, SW_SHOWNOACTIVATE);
    Expect(!TaskbarMonitor_HasUsableWindow(),
           "detached taskbar monitor was treated as usable");
    Expect(TaskbarMonitor_SetWindowParent(owner, FALSE),
           "failed to attach popup-style recovery test window");
    Expect(TaskbarMonitor_HasUsableWindow(),
           "visible modern taskbar monitor was not usable");

    g_taskbarMonitor.modernTaskbar = TRUE;
    Expect(!TaskbarMonitor_HasUsableWindow(),
           "modern taskbar accepted a popup-style monitor");
    Expect(TaskbarMonitor_SetWindowParent(owner, TRUE),
           "failed to attach child-style recovery test window");
    Expect(TaskbarMonitor_HasUsableWindow(),
           "modern child-style taskbar monitor was not usable");
    TaskbarMonitor_ScheduleWindowRecovery();
    Expect(g_taskbarMonitor.recoveryTimerId == 0,
           "hidden taskbar ancestor caused a false recovery retry");

    g_taskbarMonitor.mode = TASKBAR_HOST_CLASSIC;
    g_taskbarMonitor.modernTaskbar = FALSE;
    Expect(TaskbarMonitor_SetWindowParent(owner, FALSE),
           "failed to restore classic popup-style test window");
    Expect(!TaskbarMonitor_HasUsableWindow(),
           "classic taskbar monitor without task list was usable");
    g_taskbarMonitor.taskList = owner;
    Expect(!TaskbarMonitor_HasUsableWindow(),
           "classic taskbar monitor without a reserved slot was usable");
    g_taskbarMonitor.taskListReserved = TRUE;
    Expect(TaskbarMonitor_HasUsableWindow(),
           "complete classic taskbar monitor was not usable");

    g_taskbarMonitor.mode = TASKBAR_HOST_MODERN;
    g_taskbarMonitor.taskList = NULL;
    g_taskbarMonitor.taskListReserved = FALSE;
    SetParent(monitor, NULL);
    g_attachCalls = 0;
    g_attachSucceeds = TRUE;
    TaskbarMonitor_ScheduleWindowRecovery();
    Expect(g_taskbarMonitor.recoveryTimerId != 0,
           "detached monitor recovery was not scheduled");
    PumpMessagesFor(TASKBAR_MONITOR_RECOVERY_MS + 150);
    Expect(g_attachCalls == 1,
           "detached monitor recovery did not reattach exactly once");
    Expect(TaskbarMonitor_HasUsableWindow(),
           "detached monitor recovery did not restore attachment");

    ShowWindow(monitor, SW_HIDE);
    g_taskbarMonitor.taskbar = NULL;
    g_taskbarMonitor.host = NULL;
    g_taskbarMonitor.taskList = NULL;
    g_taskbarMonitor.mode = TASKBAR_HOST_NONE;
    g_attachCalls = 0;
    g_attachSucceeds = TRUE;
    TaskbarMonitor_ScheduleWindowRecovery();
    Expect(g_taskbarMonitor.recoveryTimerId != 0,
           "hidden monitor recovery was not scheduled");
    PumpMessagesFor(TASKBAR_MONITOR_RECOVERY_MS + 150);
    Expect(g_attachCalls == 1,
           "hidden monitor recovery did not attach exactly once");
    Expect(TaskbarMonitor_HasUsableWindow(),
           "hidden monitor recovery did not restore a usable window");
    Expect(g_taskbarMonitor.recoveryTimerId == 0,
           "successful recovery left a retry timer active");

    ShowWindow(monitor, SW_HIDE);
    g_taskbarMonitor.taskbar = NULL;
    g_taskbarMonitor.host = NULL;
    g_taskbarMonitor.mode = TASKBAR_HOST_NONE;
    g_attachCalls = 0;
    g_attachSucceeds = FALSE;
    TaskbarMonitor_ScheduleWindowRecovery();
    PumpMessagesFor(TASKBAR_MONITOR_RECOVERY_MS * 2 + 150);
    Expect(g_attachCalls >= 2,
           "failed taskbar attachment was not retried");
    Expect(g_taskbarMonitor.recoveryTimerId != 0,
           "failed taskbar attachment stopped retrying");
    TaskbarMonitor_CancelWindowRecovery();

    g_taskbarMonitor.window = monitor;
    g_taskbarMonitor.taskbar = owner;
    g_taskbarMonitor.host = owner;
    g_taskbarMonitor.taskList = owner;
    g_taskbarMonitor.taskListReserved = TRUE;
    g_taskbarMonitor.mode = TASKBAR_HOST_CLASSIC;
    g_restoreCalls = 0;
    TaskbarMonitor_OnMonitorWindowDestroyed(monitor);
    Expect(g_restoreCalls == 1,
           "unexpected monitor destruction did not restore classic space");
    Expect(g_taskbarMonitor.window == NULL &&
           g_taskbarMonitor.taskbar == NULL &&
           g_taskbarMonitor.host == NULL &&
           g_taskbarMonitor.taskList == NULL &&
           g_taskbarMonitor.mode == TASKBAR_HOST_NONE,
           "unexpected monitor destruction retained stale attachment state");
    Expect(g_taskbarMonitor.recoveryTimerId != 0,
           "unexpected monitor destruction did not schedule recovery");
    TaskbarMonitor_CancelWindowRecovery();
    g_taskbarMonitor.window = monitor;

    g_taskbarMonitor.menuPreviewSessionActive = TRUE;
    TaskbarMonitor_ScheduleWindowRecovery();
    Expect(g_taskbarMonitor.recoveryTimerId == 0,
           "menu preview scheduled taskbar recovery");
    g_taskbarMonitor.menuPreviewSessionActive = FALSE;
    g_taskbarMonitor.cpuMemoryEnabled = FALSE;
    TaskbarMonitor_ScheduleWindowRecovery();
    Expect(g_taskbarMonitor.recoveryTimerId == 0,
           "disabled taskbar monitor scheduled recovery");

cleanup:
    TaskbarMonitor_CancelWindowRecovery();
    if (monitor) DestroyWindow(monitor);
    if (owner) DestroyWindow(owner);
    UnregisterClassW(className, instance);
    return g_failures ? 1 : 0;
}
