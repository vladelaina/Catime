#ifndef CATIME_TASKBAR_MONITOR_INTERNAL_H
#define CATIME_TASKBAR_MONITOR_INTERNAL_H

#include <windows.h>

#include "system_monitor.h"

#define TASKBAR_MONITOR_CLASS L"CatimeTaskbarMonitorWindow"
#define TASKBAR_MONITOR_PRESENT_TIMER_ID 2u
#define TASKBAR_MONITOR_PRESENT_MS 100u
#define TASKBAR_MONITOR_THEME_RECHECK_TIMER_ID 3u
#define TASKBAR_MONITOR_FALLBACK_NETWORK_WIDTH 72
#define TASKBAR_MONITOR_FALLBACK_RESOURCE_WIDTH 64
#define TASKBAR_MONITOR_HORIZONTAL_HEIGHT 32
#define TASKBAR_MONITOR_GROUP_HEIGHT 36
#define TASKBAR_MONITOR_GAP 2
#define TASKBAR_MONITOR_MIN_TASK_LIST 96
#define TASKBAR_MONITOR_MAX_METRICS 4
#define TASKBAR_MONITOR_LABEL_LENGTH 32
#define TASKBAR_MONITOR_VALUE_LENGTH 32
#define TASKBAR_MONITOR_CELL_PADDING 1
#define TASKBAR_MONITOR_COLUMN_GAP 2
#define TASKBAR_MONITOR_GROUP_GAP 2

typedef enum {
    TASKBAR_HOST_NONE = 0,
    TASKBAR_HOST_CLASSIC,
    TASKBAR_HOST_MODERN
} TaskbarHostMode;

typedef enum {
    TASKBAR_COMPOSITION_UNKNOWN = 0,
    TASKBAR_COMPOSITION_PER_PIXEL,
    TASKBAR_COMPOSITION_COLOR_KEY
} TaskbarCompositionMode;

typedef enum {
    TASKBAR_METRIC_GROUP_NETWORK = 0,
    TASKBAR_METRIC_GROUP_RESOURCE
} TaskbarMetricGroup;

typedef struct {
    TaskbarMetricGroup group;
    int row;
    wchar_t label[TASKBAR_MONITOR_LABEL_LENGTH];
    wchar_t value[TASKBAR_MONITOR_VALUE_LENGTH];
} TaskbarMetricText;

typedef struct {
    HINSTANCE instance;
    HWND owner;
    HWND window;
    HWND taskbar;
    HWND host;
    HWND taskList;
    HFONT font;
    RECT originalTaskList;
    RECT reservedTaskList;
    UINT dpi;
    int taskbarWidth;
    int taskbarHeight;
    int width;
    int height;
    int networkLabelWidth;
    int resourceLabelWidth;
    int networkGroupWidth;
    int resourceGroupWidth;
    BOOL horizontal;
    BOOL initialized;
    BOOL classRegistered;
    COLORREF textColor;
    BOOL cpuMemoryEnabled;
    BOOL networkEnabled;
    BOOL taskListReserved;
    BOOL modernTaskbar;
    TaskbarHostMode mode;
    SystemMonitorSnapshot metrics;
    TaskbarCompositionMode compositionMode;
    DWORD themeRecheckDueTick;
    wchar_t cpuLabel[TASKBAR_MONITOR_LABEL_LENGTH];
    wchar_t memoryLabel[TASKBAR_MONITOR_LABEL_LENGTH];
} TaskbarMonitorState;

extern TaskbarMonitorState g_taskbarMonitor;

int TaskbarMonitor_ScaleForDpi(int value, UINT dpi);
UINT TaskbarMonitor_GetWindowDpi(HWND window);
BOOL TaskbarMonitor_GetWindowRectInParent(
    HWND window, HWND parent, RECT* output);
BOOL TaskbarMonitor_RectsNearEqual(const RECT* first, const RECT* second);
void TaskbarMonitor_DeleteFont(void);
void TaskbarMonitor_RecreateFont(void);
void TaskbarMonitor_UpdateThemeState(void);
void TaskbarMonitor_RefreshTextLayout(void);
void TaskbarMonitor_UpdateDimensions(const RECT* taskbarRect);
BOOL TaskbarMonitor_ShouldUseClassicPlacement(
    BOOL modernTaskbar, BOOL classicHostAvailable,
    BOOL taskListAvailable);
BOOL TaskbarMonitor_IsWindows11OrLaterVersion(
    DWORD majorVersion, DWORD buildNumber);
BOOL TaskbarMonitor_IsModernTaskbar(HWND taskbar);
BOOL TaskbarMonitor_CalculateClassicPlacement(
    const RECT* taskListRect, BOOL horizontal,
    int monitorWidth, int monitorHeight, int gap,
    int minimumTaskList, RECT* reservedTaskList,
    RECT* monitorRect);
BOOL TaskbarMonitor_CalculateModernPlacement(
    const RECT* taskbarBounds, const RECT* notificationArea,
    BOOL hasNotificationArea, BOOL horizontal,
    int monitorWidth, int monitorHeight, int gap,
    int fallbackInset, RECT* monitorRect);
BOOL TaskbarMonitor_Present(
    HWND window, HDC fallbackTarget,
    const TaskbarMetricText* metrics,
    int metricCount);
void TaskbarMonitor_ColorizeTextMask(
    DWORD* pixels, size_t count, COLORREF textColor);
void TaskbarMonitor_EnsureInteractiveAlpha(
    DWORD* pixels, size_t count, COLORREF textColor);
void TaskbarMonitor_DrawMetricGrid(
    HDC dc, int width, int height,
    const TaskbarMetricText* metrics, int metricCount);

void TaskbarMonitor_RestoreClassicTaskList(void);
BOOL TaskbarMonitor_AttachToTaskbar(void);
void TaskbarMonitor_RefreshAttachment(void);

LRESULT CALLBACK TaskbarMonitorWindowProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);

#endif /* CATIME_TASKBAR_MONITOR_INTERNAL_H */
