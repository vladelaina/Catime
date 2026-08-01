#include "tray/tray_menu_tracking.h"

#include <assert.h>
#include <stdio.h>

#if !defined(_WIN32_WINNT) || _WIN32_WINNT != 0x0601
#error "Tray menu tracking tests must target Windows 7"
#endif

static LONG_PTR ReadStyle(HWND window) {
    SetLastError(ERROR_SUCCESS);
    LONG_PTR style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    assert(style != 0 || GetLastError() == ERROR_SUCCESS);
    return style;
}

static void WriteStyle(HWND window, LONG_PTR style) {
    SetLastError(ERROR_SUCCESS);
    LONG_PTR previous = SetWindowLongPtrW(window, GWL_EXSTYLE, style);
    assert(previous != 0 || GetLastError() == ERROR_SUCCESS);
}

static void TestInvalidArguments(void) {
    TrayMenuTrackingState state = {0};
    assert(!TrayMenuTracking_Begin(NULL, &state));
    assert(!state.initialized);
    assert(!TrayMenuTracking_Begin(NULL, NULL));
    assert(!TrayMenuTracking_ReassertForeground(NULL));
    TrayMenuTracking_End(NULL);
}

static void TestNoActivateLifecycle(void) {
    HWND window = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"STATIC", L"Catime tray menu tracking test", WS_POPUP,
        0, 0, 1, 1, NULL, NULL, GetModuleHandleW(NULL), NULL);
    assert(window != NULL);

    TrayMenuTrackingState state = {0};
    (void)TrayMenuTracking_Begin(window, &state);
    assert(state.initialized);
    assert(state.owner == window);
    assert(state.restoreNoActivate);
    assert((ReadStyle(window) & WS_EX_NOACTIVATE) == 0);

    LONG_PTR duringTracking = ReadStyle(window) | WS_EX_TRANSPARENT;
    WriteStyle(window, duringTracking);
    TrayMenuTracking_End(&state);
    LONG_PTR restored = ReadStyle(window);
    assert(restored & WS_EX_NOACTIVATE);
    assert(restored & WS_EX_TRANSPARENT);
    assert(!state.initialized);

    TrayMenuTracking_End(&state);
    assert(DestroyWindow(window));
}

static void TestExistingActivationStyleIsPreserved(void) {
    HWND window = CreateWindowExW(
        WS_EX_TOOLWINDOW, L"STATIC", L"Catime active menu owner test",
        WS_POPUP, 0, 0, 1, 1, NULL, NULL, GetModuleHandleW(NULL), NULL);
    assert(window != NULL);

    TrayMenuTrackingState state = {0};
    (void)TrayMenuTracking_Begin(window, &state);
    assert(state.initialized);
    assert(!state.restoreNoActivate);
    TrayMenuTracking_End(&state);
    assert((ReadStyle(window) & WS_EX_NOACTIVATE) == 0);
    assert(DestroyWindow(window));
}

int main(void) {
    TestInvalidArguments();
    TestNoActivateLifecycle();
    TestExistingActivationStyleIsPreserved();
    puts("tray menu tracking tests passed");
    return 0;
}
