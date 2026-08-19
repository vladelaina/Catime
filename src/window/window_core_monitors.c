#include "window_core_internal.h"

int WindowCore_ClampInt64ToInt(long long value) {
    if (value < INT_MIN) return INT_MIN;
    if (value > INT_MAX) return INT_MAX;
    return (int)value;
}

int WindowCore_AddIntsClamped(int first, int second) {
    return WindowCore_ClampInt64ToInt((long long)first + second);
}

static BOOL IsShellOrDesktopWindow(HWND hwnd) {
    wchar_t className[64] = {0};
    if (!hwnd || GetClassNameW(
            hwnd, className, _countof(className)) == 0) return FALSE;
    return wcscmp(className, L"Progman") == 0 ||
           wcscmp(className, L"WorkerW") == 0 ||
           wcscmp(className, L"SHELLDLL_DefView") == 0 ||
           wcscmp(className, L"Shell_TrayWnd") == 0 ||
           wcscmp(className, L"Shell_SecondaryTrayWnd") == 0;
}

BOOL WindowCore_IsFullscreenForegroundWindowActive(HWND hwnd) {
    HWND foreground = GetForegroundWindow();
    if (!foreground || foreground == hwnd ||
        foreground == GetDesktopWindow() || !IsWindow(foreground) ||
        !IsWindowVisible(foreground) || IsIconic(foreground) ||
        IsShellOrDesktopWindow(foreground)) return FALSE;
    DWORD processId = 0;
    GetWindowThreadProcessId(foreground, &processId);
    if (processId == GetCurrentProcessId()) return FALSE;
    SetLastError(0);
    LONG_PTR style = GetWindowLongPtr(foreground, GWL_STYLE);
    if (style == 0 && GetLastError() != 0) return FALSE;
    if ((style & WS_CHILD) != 0 ||
        ((style & (WS_CAPTION | WS_THICKFRAME)) != 0 &&
         IsZoomed(foreground))) return FALSE;
    RECT foregroundRect;
    if (!GetWindowRect(foreground, &foregroundRect) ||
        foregroundRect.right <= foregroundRect.left ||
        foregroundRect.bottom <= foregroundRect.top) return FALSE;
    HMONITOR monitor = MonitorFromWindow(
        foreground, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfo(monitor, &info)) return FALSE;
    return foregroundRect.left <= info.rcMonitor.left + FULLSCREEN_RECT_TOLERANCE_PX &&
           foregroundRect.top <= info.rcMonitor.top + FULLSCREEN_RECT_TOLERANCE_PX &&
           foregroundRect.right >= info.rcMonitor.right - FULLSCREEN_RECT_TOLERANCE_PX &&
           foregroundRect.bottom >= info.rcMonitor.bottom - FULLSCREEN_RECT_TOLERANCE_PX;
}

void WindowCore_GetPrimaryMonitorInfo(MONITORINFO* info) {
    if (!info) return;
    POINT point = {0, 0};
    HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTOPRIMARY);
    info->cbSize = sizeof(*info);
    if (!GetMonitorInfo(monitor, info)) {
        info->rcMonitor.left = 0;
        info->rcMonitor.top = 0;
        info->rcMonitor.right = GetSystemMetrics(SM_CXSCREEN);
        info->rcMonitor.bottom = GetSystemMetrics(SM_CYSCREEN);
        info->rcWork = info->rcMonitor;
    }
}

typedef struct {
    RECT windowRect;
    int requiredWidth;
    int requiredHeight;
    BOOL visible;
} MonitorVisibilityCheck;

static BOOL CALLBACK CheckWindowVisibilityOnMonitor(
    HMONITOR monitor, HDC dc, LPRECT monitorRect, LPARAM parameter) {
    (void)monitor;
    (void)dc;
    MonitorVisibilityCheck* check = (MonitorVisibilityCheck*)parameter;
    if (!check || !monitorRect) return FALSE;
    RECT intersection = {0};
    if (IntersectRect(&intersection, &check->windowRect, monitorRect) &&
        intersection.right - intersection.left >= check->requiredWidth &&
        intersection.bottom - intersection.top >= check->requiredHeight) {
        check->visible = TRUE;
        return FALSE;
    }
    return TRUE;
}

BOOL WindowCore_IsWindowRectVisibleOnAnyMonitor(const RECT* rect) {
    if (!rect || rect->right <= rect->left || rect->bottom <= rect->top) {
        return FALSE;
    }
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    MonitorVisibilityCheck check = {
        *rect,
        WindowPlacement_GetMinimumVisibleLength(
            width, WINDOW_VISIBLE_MARGIN),
        WindowPlacement_GetMinimumVisibleLength(
            height, WINDOW_VISIBLE_MARGIN),
        FALSE
    };
    EnumDisplayMonitors(
        NULL, NULL, CheckWindowVisibilityOnMonitor, (LPARAM)&check);
    return check.visible;
}

BOOL WindowCore_GetPersistentMonitorIdW(
    const wchar_t* displayName, wchar_t* monitorId, size_t monitorIdSize) {
    if (!displayName || !*displayName || !monitorId || monitorIdSize == 0) {
        return FALSE;
    }
    DISPLAY_DEVICEW device = {0};
    device.cb = sizeof(device);
    const wchar_t* value = displayName;
    if (EnumDisplayDevicesW(
            displayName, 0, &device,
            EDD_GET_DEVICE_INTERFACE_NAME) && device.DeviceID[0] != L'\0') {
        value = device.DeviceID;
    }
    if (wcslen(value) >= monitorIdSize) return FALSE;
    wcscpy_s(monitorId, monitorIdSize, value);
    return TRUE;
}

typedef struct {
    const wchar_t* monitorId;
    HMONITOR monitor;
    MONITORINFOEXW info;
} MonitorDeviceSearch;

static BOOL CALLBACK FindMonitorByDeviceCallback(
    HMONITOR monitor, HDC dc, LPRECT rect, LPARAM parameter) {
    (void)dc;
    (void)rect;
    MonitorDeviceSearch* search = (MonitorDeviceSearch*)parameter;
    if (!search || !search->monitorId) return FALSE;
    MONITORINFOEXW info = {0};
    info.cbSize = sizeof(info);
    wchar_t id[256] = {0};
    if (GetMonitorInfoW(monitor, (MONITORINFO*)&info) &&
        WindowCore_GetPersistentMonitorIdW(
            info.szDevice, id, _countof(id)) &&
        (_wcsicmp(id, search->monitorId) == 0 ||
         _wcsicmp(info.szDevice, search->monitorId) == 0)) {
        search->monitor = monitor;
        search->info = info;
        return FALSE;
    }
    return TRUE;
}

BOOL WindowCore_FindMonitorByIdUtf8(
    const char* monitorId, HMONITOR* outMonitor, MONITORINFOEXW* outInfo) {
    if (!monitorId || !*monitorId || !outMonitor || !outInfo) return FALSE;
    wchar_t wideId[256] = {0};
    if (MultiByteToWideChar(
            CP_UTF8, 0, monitorId, -1,
            wideId, _countof(wideId)) <= 0) return FALSE;
    MonitorDeviceSearch search = {0};
    search.monitorId = wideId;
    EnumDisplayMonitors(
        NULL, NULL, FindMonitorByDeviceCallback, (LPARAM)&search);
    if (!search.monitor) return FALSE;
    *outMonitor = search.monitor;
    *outInfo = search.info;
    return TRUE;
}
