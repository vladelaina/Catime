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

static void TestMinimumVisibleLength(void) {
    ExpectNear("invalid visible length",
               WindowPlacement_GetMinimumVisibleLength(0, 20), 0, 0);
    ExpectNear("small window remains fully visible",
               WindowPlacement_GetMinimumVisibleLength(10, 20), 10, 0);
    ExpectNear("odd window keeps a majority visible",
               WindowPlacement_GetMinimumVisibleLength(75, 20), 38, 0);
    ExpectNear("large window keeps half visible",
               WindowPlacement_GetMinimumVisibleLength(412, 20), 206, 0);
}

static void TestFullyVisibleClamp(void) {
    const RECT primaryWorkArea = {0, 0, 1920, 1040};
    const RECT leftWorkArea = {-1920, 0, 0, 1040};
    int x = 3000;
    int y = 2000;

    if (!WindowPlacement_ClampFullyVisible(
            &primaryWorkArea, 400, 200, &x, &y)) {
        fprintf(stderr, "valid visibility clamp failed\n");
        g_failures++;
    } else {
        ExpectNear("off-screen clamp x", x, 1520, 0);
        ExpectNear("off-screen clamp y", y, 840, 0);
    }

    x = -1800;
    y = 120;
    WindowPlacement_ClampFullyVisible(&leftWorkArea, 400, 200, &x, &y);
    ExpectNear("negative monitor x preserved", x, -1800, 0);
    ExpectNear("negative monitor y preserved", y, 120, 0);

    x = 500;
    y = 500;
    WindowPlacement_ClampFullyVisible(
        &primaryWorkArea, 4000, 2000, &x, &y);
    ExpectNear("oversized window aligns left", x, 0, 0);
    ExpectNear("oversized window aligns top", y, 0, 0);

    RECT invalid = {0, 0, 0, 0};
    if (WindowPlacement_ClampFullyVisible(&invalid, 400, 200, &x, &y)) {
        fprintf(stderr, "invalid visibility bounds were accepted\n");
        g_failures++;
    }
}

static void TestTaskbarAnchorPolicy(void) {
    const RECT taskbar = {0, 1000, 1920, 1080};
    const RECT overlappingWindow = {1600, 930, 2200, 1200};
    const RECT separateWindow = {1600, 800, 1800, 900};
    const RECT touchingWindow = {1600, 900, 1800, 1000};

    if (WindowPlacement_ShouldPreserveTaskbarAnchor(
            FALSE, &overlappingWindow, &taskbar)) {
        fprintf(stderr, "incidental taskbar overlap was treated as an anchor\n");
        g_failures++;
    }
    if (!WindowPlacement_ShouldPreserveTaskbarAnchor(
            TRUE, &overlappingWindow, &taskbar)) {
        fprintf(stderr, "configured taskbar anchor was not preserved\n");
        g_failures++;
    }
    if (WindowPlacement_ShouldPreserveTaskbarAnchor(
            TRUE, &separateWindow, &taskbar) ||
        WindowPlacement_ShouldPreserveTaskbarAnchor(
            TRUE, &touchingWindow, &taskbar)) {
        fprintf(stderr, "non-overlapping window used a taskbar anchor\n");
        g_failures++;
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
    TestMinimumVisibleLength();
    TestFullyVisibleClamp();
    TestTaskbarAnchorPolicy();

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
