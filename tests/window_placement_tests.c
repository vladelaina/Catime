#include "window/window_placement.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int g_failures = 0;

static void ExpectNear(const char* name, int actual, int expected, int tolerance) {
    if (abs(actual - expected) > tolerance) {
        fprintf(stderr, "%s: expected %d (+/-%d), got %d\n",
                name, expected, tolerance, actual);
        g_failures++;
    }
}

static void TestRoundTrip(const char* name,
                          const RECT* monitor,
                          const RECT* taskbar,
                          const RECT* window) {
    int ratio = 0;
    int cross = 0;
    int x = 0;
    int y = 0;
    if (!WindowPlacement_CaptureTaskbarAnchor(window, taskbar, monitor,
                                              &ratio, &cross) ||
        !WindowPlacement_ResolveTaskbarAnchor(
            taskbar, monitor,
            window->right - window->left,
            window->bottom - window->top,
            ratio, cross, &x, &y)) {
        fprintf(stderr, "%s: capture/resolve failed\n", name);
        g_failures++;
        return;
    }
    ExpectNear(name, x, window->left, 1);
    ExpectNear(name, y, window->top, 1);
}

static void TestManualTopLeftRestore(void) {
    const RECT manualRect = {1339, 1025, 1539, 1125};
    const RECT taskbarAdjustedRect = {1389, 1050, 1489, 1100};
    const RECT unchangedRect = {1339, 1025, 1439, 1075};
    const RECT negativeManualRect = {-1500, -40, -1300, 60};
    const RECT negativeAdjustedRect = {-1450, -15, -1350, 35};
    POINT restore = {0};

    if (!WindowPlacement_GetManualTopLeftRestore(
            &manualRect, &taskbarAdjustedRect, &restore)) {
        fprintf(stderr, "taskbar-adjusted edit exit was not restored\n");
        g_failures++;
    } else {
        ExpectNear("manual restore x", restore.x, manualRect.left, 0);
        ExpectNear("manual restore y", restore.y, manualRect.top, 0);
    }

    if (WindowPlacement_GetManualTopLeftRestore(
            &manualRect, &unchangedRect, &restore)) {
        fprintf(stderr, "unchanged edit exit requested an unnecessary restore\n");
        g_failures++;
    }

    if (!WindowPlacement_GetManualTopLeftRestore(
            &negativeManualRect, &negativeAdjustedRect, &restore)) {
        fprintf(stderr, "negative-coordinate edit exit was not restored\n");
        g_failures++;
    } else {
        ExpectNear("negative manual restore x", restore.x,
                   negativeManualRect.left, 0);
        ExpectNear("negative manual restore y", restore.y,
                   negativeManualRect.top, 0);
    }
}

int main(void) {
    const RECT monitor = {0, 0, 1920, 1080};
    const RECT bottom = {0, 1040, 1920, 1080};
    const RECT top = {0, 0, 1920, 40};
    const RECT left = {0, 0, 40, 1080};
    const RECT right = {1880, 0, 1920, 1080};
    const RECT bottomWindow = {1339, 1025, 1539, 1125};
    const RECT topWindow = {381, -45, 581, 55};
    const RECT leftWindow = {-45, 300, 55, 500};
    const RECT rightWindow = {1865, 700, 1965, 900};
    const RECT leftMonitor = {-1920, 0, 0, 1080};
    const RECT leftMonitorTaskbar = {-1920, 1040, 0, 1080};
    const RECT negativeWindow = {-1500, 1025, -1300, 1125};

    TestRoundTrip("bottom round-trip", &monitor, &bottom, &bottomWindow);
    TestRoundTrip("top round-trip", &monitor, &top, &topWindow);
    TestRoundTrip("left round-trip", &monitor, &left, &leftWindow);
    TestRoundTrip("right round-trip", &monitor, &right, &rightWindow);
    TestRoundTrip("negative-monitor round-trip", &leftMonitor,
                  &leftMonitorTaskbar, &negativeWindow);
    TestManualTopLeftRestore();

    int ratio = 0;
    int cross = 0;
    int x = 0;
    int y = 0;
    WindowPlacement_CaptureTaskbarAnchor(&bottomWindow, &bottom, &monitor,
                                         &ratio, &cross);
    WindowPlacement_ResolveTaskbarAnchor(&top, &monitor, 200, 100,
                                         ratio, cross, &x, &y);
    ExpectNear("bottom-to-top mirrored depth", y, -45, 0);

    WindowPlacement_ResolveTaskbarAnchor(&left, &monitor, 200, 100,
                                         ratio, cross, &x, &y);
    ExpectNear("bottom-to-left mirrored depth", x, -95, 0);

    WindowPlacement_ResolveTaskbarAnchor(&bottom, &monitor, 200, 100,
                                         INT_MAX, INT_MAX, &x, &y);
    ExpectNear("malformed ratio clamps right", x, 1820, 0);
    ExpectNear("malformed cross offset saturates", y, INT_MAX - 50, 0);

    RECT invalid = {0, 0, 0, 0};
    if (WindowPlacement_CaptureTaskbarAnchor(&invalid, &bottom, &monitor,
                                             &ratio, &cross)) {
        fprintf(stderr, "invalid rectangle was accepted\n");
        g_failures++;
    }

    if (g_failures != 0) return 1;
    puts("window placement tests passed");
    return 0;
}
