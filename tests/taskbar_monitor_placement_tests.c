#include "taskbar_monitor/taskbar_monitor_internal.h"

#include <stdio.h>

static int failures = 0;

static void Expect(BOOL condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static int RectWidth(const RECT* rect) {
    return rect->right - rect->left;
}

static int RectHeight(const RECT* rect) {
    return rect->bottom - rect->top;
}

static void TestHostSelection(void) {
    Expect(!TaskbarMonitor_ShouldUseClassicPlacement(TRUE, TRUE, TRUE),
           "modern taskbar used its centered classic compatibility window");
    Expect(TaskbarMonitor_ShouldUseClassicPlacement(FALSE, TRUE, TRUE),
           "classic taskbar did not reserve its task-list slot");
    Expect(!TaskbarMonitor_ShouldUseClassicPlacement(FALSE, FALSE, TRUE),
           "missing classic host was accepted");
    Expect(!TaskbarMonitor_ShouldUseClassicPlacement(FALSE, TRUE, FALSE),
           "missing classic task list was accepted");
    Expect(!TaskbarMonitor_IsWindows11OrLaterVersion(6, 7601),
           "Windows 7 was classified as a modern taskbar system");
    Expect(!TaskbarMonitor_IsWindows11OrLaterVersion(10, 19045),
           "Windows 10 was classified as Windows 11");
    Expect(TaskbarMonitor_IsWindows11OrLaterVersion(10, 22000),
           "first Windows 11 build was not recognized");
    Expect(TaskbarMonitor_IsWindows11OrLaterVersion(10, 26200),
           "current Windows 11 build was not recognized");
    Expect(TaskbarMonitor_IsWindows11OrLaterVersion(11, 0),
           "future Windows major version was not recognized");
}

static void TestModernHorizontalOptionSwitches(void) {
    const RECT bounds = {0, 0, 1536, 48};
    const RECT notification = {1258, 0, 1536, 48};
    const int widths[] = {145, 72, 65, 145, 65, 72};
    const int expectedRight = notification.left - 2;
    for (size_t i = 0; i < _countof(widths); ++i) {
        RECT monitor = {0};
        BOOL placed = TaskbarMonitor_CalculateModernPlacement(
            &bounds, &notification, TRUE, TRUE,
            widths[i], 32, 2, 100, &monitor);
        Expect(placed, "horizontal modern placement failed");
        Expect(monitor.right == expectedRight,
               "option switch moved the notification-area anchor");
        Expect(RectWidth(&monitor) == widths[i],
               "option switch changed the requested monitor width");
        Expect(monitor.top == 8 && monitor.bottom == 40,
               "horizontal monitor was not vertically centered");
    }
}

static void TestModernFallbackAndBounds(void) {
    const RECT bounds = {0, 0, 800, 40};
    const RECT invalidNotification = {0, 0, 0, 0};
    RECT monitor = {0};
    BOOL placed = TaskbarMonitor_CalculateModernPlacement(
        &bounds, &invalidNotification, TRUE, TRUE,
        140, 32, 2, 100, &monitor);
    Expect(placed, "notification fallback placement failed");
    Expect(monitor.right == 698,
           "notification fallback did not retain a fixed right inset");

    const RECT narrowBounds = {0, 0, 90, 40};
    placed = TaskbarMonitor_CalculateModernPlacement(
        &narrowBounds, NULL, FALSE, TRUE,
        140, 80, 2, 0, &monitor);
    Expect(placed, "narrow taskbar placement failed");
    Expect(monitor.left >= narrowBounds.left &&
           monitor.right <= narrowBounds.right,
           "narrow taskbar placement exceeded horizontal bounds");
    Expect(monitor.top >= narrowBounds.top &&
           monitor.bottom <= narrowBounds.bottom,
           "narrow taskbar placement exceeded vertical bounds");
}

static void TestModernVerticalOptionSwitches(void) {
    const RECT bounds = {0, 0, 48, 900};
    const RECT notification = {0, 720, 48, 900};
    const int heights[] = {72, 36, 72, 36};
    const int expectedBottom = notification.top - 2;
    for (size_t i = 0; i < _countof(heights); ++i) {
        RECT monitor = {0};
        BOOL placed = TaskbarMonitor_CalculateModernPlacement(
            &bounds, &notification, TRUE, FALSE,
            44, heights[i], 2, 56, &monitor);
        Expect(placed, "vertical modern placement failed");
        Expect(monitor.bottom == expectedBottom,
               "vertical option switch moved the notification anchor");
        Expect(RectHeight(&monitor) == heights[i],
               "vertical option switch changed monitor height");
        Expect(monitor.left == 2 && monitor.right == 46,
               "vertical monitor was not horizontally centered");
    }
}

static void TestClassicHorizontalOptionSwitches(void) {
    const RECT taskList = {40, 0, 700, 48};
    const int widths[] = {145, 72, 65, 145};
    for (size_t i = 0; i < _countof(widths); ++i) {
        RECT reserved = {0};
        RECT monitor = {0};
        BOOL placed = TaskbarMonitor_CalculateClassicPlacement(
            &taskList, TRUE, widths[i], 32, 2, 96,
            &reserved, &monitor);
        Expect(placed, "horizontal classic placement failed");
        Expect(monitor.right == taskList.right,
               "classic option switch moved the task-list anchor");
        Expect(reserved.right + 2 == monitor.left,
               "classic task-list reservation gap changed");
        Expect(RectWidth(&reserved) ==
                   RectWidth(&taskList) - widths[i] - 2,
               "classic task-list reservation used cumulative width");
    }
}

static void TestClassicVerticalAndMinimum(void) {
    const RECT taskList = {0, 30, 48, 850};
    RECT reserved = {0};
    RECT monitor = {0};
    BOOL placed = TaskbarMonitor_CalculateClassicPlacement(
        &taskList, FALSE, 44, 72, 2, 96,
        &reserved, &monitor);
    Expect(placed, "vertical classic placement failed");
    Expect(monitor.bottom == taskList.bottom,
           "vertical classic monitor lost its bottom anchor");
    Expect(reserved.bottom + 2 == monitor.top,
           "vertical classic reservation gap changed");

    const RECT smallTaskList = {0, 0, 200, 48};
    placed = TaskbarMonitor_CalculateClassicPlacement(
        &smallTaskList, TRUE, 110, 32, 2, 96,
        &reserved, &monitor);
    Expect(!placed, "classic placement violated minimum task-list space");
}

static void TestMonitorSizes(void) {
    int width = 0;
    int height = 0;
    Expect(TaskbarMonitor_CalculateMonitorSize(
               1920, 40, TRUE, 96, 72, 64,
               FALSE, TRUE, &width, &height),
           "horizontal network monitor size calculation failed");
    Expect(width == 72 && height == 32,
           "horizontal network monitor size was incorrect");

    Expect(TaskbarMonitor_CalculateMonitorSize(
               1920, 40, TRUE, 96, 72, 64,
               TRUE, FALSE, &width, &height),
           "horizontal resource monitor size calculation failed");
    Expect(width == 64 && height == 32,
           "horizontal resource monitor size was incorrect");

    Expect(TaskbarMonitor_CalculateMonitorSize(
               1920, 40, TRUE, 96, 72, 64,
               TRUE, TRUE, &width, &height),
           "horizontal combined monitor size calculation failed");
    Expect(width == 138 && height == 32,
           "horizontal combined monitor size was incorrect");

    Expect(TaskbarMonitor_CalculateMonitorSize(
               1920, 40, TRUE, 96, 72, 64,
               FALSE, FALSE, &width, &height),
           "empty preview monitor size calculation failed");
    Expect(width == 64 && height == 32,
           "empty preview retained combined monitor width");

    Expect(TaskbarMonitor_CalculateMonitorSize(
               2880, 60, TRUE, 144, 108, 96,
               TRUE, TRUE, &width, &height),
           "high-DPI combined monitor size calculation failed");
    Expect(width == 207 && height == 48,
           "high-DPI combined monitor size was incorrect");

    Expect(TaskbarMonitor_CalculateMonitorSize(
               48, 1080, FALSE, 96, 72, 64,
               TRUE, TRUE, &width, &height),
           "vertical combined monitor size calculation failed");
    Expect(width == 44 && height == 72,
           "vertical combined monitor size was incorrect");

    Expect(TaskbarMonitor_CalculateMonitorSize(
               48, 1080, FALSE, 96, 72, 64,
               FALSE, FALSE, &width, &height),
           "vertical empty preview monitor size calculation failed");
    Expect(width == 44 && height == 36,
           "vertical empty preview retained combined monitor height");
}

static void TestMetricRowHeights(void) {
    Expect(TaskbarMonitor_CalculateMetricRowHeight(
               32, TRUE, TRUE, FALSE) == 16,
           "normal horizontal metric row height was incorrect");
    Expect(TaskbarMonitor_CalculateMetricRowHeight(
               24, TRUE, TRUE, TRUE) == 12,
           "short horizontal metric row height was incorrect");
    Expect(TaskbarMonitor_CalculateMetricRowHeight(
               36, FALSE, TRUE, FALSE) == 18,
           "single-group vertical metric row height was incorrect");
    Expect(TaskbarMonitor_CalculateMetricRowHeight(
               72, FALSE, TRUE, TRUE) == 18,
           "combined vertical metric row height was incorrect");
    Expect(TaskbarMonitor_CalculateMetricRowHeight(
               48, FALSE, TRUE, TRUE) == 12,
           "compressed vertical metric row height was incorrect");
    Expect(TaskbarMonitor_CalculateMetricRowHeight(
               0, TRUE, TRUE, FALSE) == 0,
           "invalid monitor height produced a metric row");
}

static void TestPreviewPlacement(void) {
    RECT original = {100, 4, 164, 36};
    RECT preview = {0};
    Expect(TaskbarMonitor_CalculatePreviewPlacement(
               &original, TRUE, 138, 32, &preview),
           "horizontal preview placement calculation failed");
    Expect(preview.left == 26 && preview.top == 4 &&
           preview.right == 164 && preview.bottom == 36,
           "horizontal preview did not preserve its right edge");

    original = preview;
    Expect(TaskbarMonitor_CalculatePreviewPlacement(
               &original, TRUE, 72, 32, &preview),
           "horizontal preview shrink calculation failed");
    Expect(preview.left == 92 && preview.top == 4 &&
           preview.right == 164 && preview.bottom == 36,
           "horizontal preview shrink moved its right edge");

    const RECT verticalOriginal = {4, 100, 48, 136};
    Expect(TaskbarMonitor_CalculatePreviewPlacement(
               &verticalOriginal, FALSE, 44, 72, &preview),
           "vertical preview placement calculation failed");
    Expect(preview.left == 4 && preview.top == 64 &&
           preview.right == 48 && preview.bottom == 136,
           "vertical preview did not preserve its bottom edge");
}

int main(void) {
    TestHostSelection();
    TestModernHorizontalOptionSwitches();
    TestModernFallbackAndBounds();
    TestModernVerticalOptionSwitches();
    TestClassicHorizontalOptionSwitches();
    TestClassicVerticalAndMinimum();
    TestMonitorSizes();
    TestMetricRowHeights();
    TestPreviewPlacement();
    if (failures == 0) {
        puts("All taskbar monitor placement tests passed.");
    }
    return failures == 0 ? 0 : 1;
}
