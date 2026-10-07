#include "window_core_internal.h"
#include "multi_window.h"

#define WINDOW_INSTANCE_PLACEMENT_SECTION "WindowPositions"

static const char* GetPrimaryPlacementKey(const char* suffix) {
    if (strcmp(suffix, "X") == 0) return "CLOCK_WINDOW_POS_X";
    if (strcmp(suffix, "Y") == 0) return "CLOCK_WINDOW_POS_Y";
    if (strcmp(suffix, "Manual") == 0) return WINDOW_POSITION_MANUAL_KEY;
    if (strcmp(suffix, "MonitorId") == 0) return WINDOW_MONITOR_ID_KEY;
    if (strcmp(suffix, "MonitorOffsetX") == 0) return WINDOW_MONITOR_OFFSET_X_KEY;
    if (strcmp(suffix, "MonitorOffsetY") == 0) return WINDOW_MONITOR_OFFSET_Y_KEY;
    if (strcmp(suffix, "TaskbarAnchored") == 0) return WINDOW_TASKBAR_ANCHORED_KEY;
    if (strcmp(suffix, "TaskbarAxisRatio") == 0) return WINDOW_TASKBAR_AXIS_RATIO_KEY;
    if (strcmp(suffix, "TaskbarCrossOffset") == 0) return WINDOW_TASKBAR_CROSS_OFFSET_KEY;
    return suffix;
}

static void GetPlacementStorage(const char* suffix, const char** section,
                                char* key, size_t keySize) {
    if (!section || !key || keySize == 0) return;
    if (!MultiWindow_IsSecondary()) {
        *section = INI_SECTION_DISPLAY;
        _snprintf_s(key, keySize, _TRUNCATE, "%s",
                    GetPrimaryPlacementKey(suffix));
        return;
    }
    *section = WINDOW_INSTANCE_PLACEMENT_SECTION;
    _snprintf_s(key, keySize, _TRUNCATE, "Window%d_%s",
                MultiWindow_GetPlacementSlot(), suffix);
}

static int ReadPlacementInt(const char* configPath, const char* suffix,
                            int defaultValue) {
    const char* section = NULL;
    char key[64] = {0};
    GetPlacementStorage(suffix, &section, key, sizeof(key));
    return ReadIniInt(section, key, defaultValue, configPath);
}

static BOOL ReadPlacementBool(const char* configPath, const char* suffix,
                              BOOL defaultValue) {
    const char* section = NULL;
    char key[64] = {0};
    GetPlacementStorage(suffix, &section, key, sizeof(key));
    return ReadIniBool(section, key, defaultValue, configPath);
}

static void ReadPlacementString(const char* configPath, const char* suffix,
                                const char* defaultValue, char* value,
                                size_t valueSize) {
    const char* section = NULL;
    char key[64] = {0};
    GetPlacementStorage(suffix, &section, key, sizeof(key));
    ReadIniString(section, key, defaultValue, value, (DWORD)valueSize,
                  configPath);
}

static BOOL HasDedicatedSecondaryPlacement(const char* configPath) {
    if (!MultiWindow_IsSecondary()) return FALSE;
    char x[32] = {0};
    char y[32] = {0};
    ReadPlacementString(configPath, "X", "", x, sizeof(x));
    ReadPlacementString(configPath, "Y", "", y, sizeof(y));
    return x[0] != '\0' && y[0] != '\0';
}

BOOL WindowCore_GetMonitorPlacementData(
    const RECT* windowRect, char* monitorId, size_t monitorIdSize,
    int* monitorOffsetX, int* monitorOffsetY,
    BOOL* taskbarAvailable, BOOL* taskbarAnchored,
    int* taskbarAxisRatio, int* taskbarCrossOffset) {
    if (!windowRect || !monitorId || monitorIdSize == 0 ||
        !monitorOffsetX || !monitorOffsetY || !taskbarAvailable ||
        !taskbarAnchored || !taskbarAxisRatio || !taskbarCrossOffset) {
        return FALSE;
    }
    HMONITOR monitor = MonitorFromRect(
        windowRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW info = {0};
    info.cbSize = sizeof(info);
    wchar_t wideId[256] = {0};
    if (!monitor || !GetMonitorInfoW(
            monitor, (MONITORINFO*)&info) ||
        !WindowCore_GetPersistentMonitorIdW(
            info.szDevice, wideId, _countof(wideId)) ||
        monitorIdSize > INT_MAX ||
        WideCharToMultiByte(
            CP_UTF8, 0, wideId, -1,
            monitorId, (int)monitorIdSize, NULL, NULL) <= 0) {
        monitorId[0] = '\0';
        return FALSE;
    }

    *monitorOffsetX = windowRect->left - info.rcMonitor.left;
    *monitorOffsetY = windowRect->top - info.rcMonitor.top;
    *taskbarAvailable = FALSE;
    *taskbarAnchored = FALSE;
    *taskbarAxisRatio = 0;
    *taskbarCrossOffset = 0;
    RECT taskbarRect = {0};
    RECT intersection = {0};
    if (!GetTaskbarRectForMonitor(monitor, &taskbarRect)) return TRUE;
    *taskbarAvailable = TRUE;
    if (!IntersectRect(&intersection, windowRect, &taskbarRect)) return TRUE;
    if (WindowPlacement_CaptureTaskbarAnchor(
            windowRect, &taskbarRect, &info.rcMonitor,
            taskbarAxisRatio, taskbarCrossOffset)) {
        *taskbarAnchored = TRUE;
    }
    return TRUE;
}

BOOL WindowCore_TryResolvePlacementMetadata(
    const char* configPath, int width, int height,
    int* posX, int* posY, BOOL* placementUnavailable) {
    if (!configPath || !posX || !posY || !placementUnavailable ||
        !CLOCK_WINDOW_POSITION_MANUAL) return FALSE;
    char monitorId[256] = {0};
    ReadPlacementString(configPath, "MonitorId", "",
                        monitorId, sizeof(monitorId));
    if (monitorId[0] == '\0') return FALSE;
    HMONITOR monitor = NULL;
    MONITORINFOEXW info = {0};
    if (!WindowCore_FindMonitorByIdUtf8(monitorId, &monitor, &info)) {
        *placementUnavailable = TRUE;
        return FALSE;
    }
    int offsetX = ReadPlacementInt(configPath, "MonitorOffsetX", 0);
    int offsetY = ReadPlacementInt(configPath, "MonitorOffsetY", 0);
    *posX = WindowCore_AddIntsClamped(info.rcMonitor.left, offsetX);
    *posY = WindowCore_AddIntsClamped(info.rcMonitor.top, offsetY);
    if (!ReadPlacementBool(configPath, "TaskbarAnchored", FALSE)) return TRUE;
    RECT taskbarRect = {0};
    if (!GetTaskbarRectForMonitor(monitor, &taskbarRect)) {
        *placementUnavailable = TRUE;
        g_placementRetryNeeded = TRUE;
        return TRUE;
    }
    int ratio = ReadPlacementInt(configPath, "TaskbarAxisRatio", 0);
    int crossOffset = ReadPlacementInt(configPath, "TaskbarCrossOffset", 0);
    if (!WindowPlacement_ResolveTaskbarAnchor(
            &taskbarRect, &info.rcMonitor, width, height,
            ratio, crossOffset, posX, posY)) {
        *placementUnavailable = TRUE;
        return FALSE;
    }
    return TRUE;
}

void ClampWindowPositionToVisibleMonitor(
    int width, int height, int* x, int* y) {
    if (!x || !y || width <= 0 || height <= 0) return;
    int requiredWidth = WindowPlacement_GetMinimumVisibleLength(
        width, WINDOW_VISIBLE_MARGIN);
    int requiredHeight = WindowPlacement_GetMinimumVisibleLength(
        height, WINDOW_VISIBLE_MARGIN);
    RECT rect = {*x, *y,
                 WindowCore_AddIntsClamped(*x, width),
                 WindowCore_AddIntsClamped(*y, height)};
    HMONITOR monitor = MonitorFromRect(
        &rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfo(monitor, &info)) WindowCore_GetPrimaryMonitorInfo(&info);
    if ((long long)*x + requiredWidth > info.rcMonitor.right) {
        *x = info.rcMonitor.right - requiredWidth;
    }
    if ((long long)*x + width - requiredWidth < info.rcMonitor.left) {
        *x = WindowCore_ClampInt64ToInt(
            (long long)info.rcMonitor.left - width + requiredWidth);
    }
    if ((long long)*y + requiredHeight > info.rcMonitor.bottom) {
        *y = info.rcMonitor.bottom - requiredHeight;
    }
    if ((long long)*y + height - requiredHeight < info.rcMonitor.top) {
        *y = WindowCore_ClampInt64ToInt(
            (long long)info.rcMonitor.top - height + requiredHeight);
    }
}

static BOOL IsSpecialWindowPositionX(int x) {
    return x == DEFAULT_WINDOW_POS_X || x == -1;
}

void ResolveConfiguredWindowPosition(
    int width, int height, int* outX, int* outY) {
    if (!outX || !outY) return;
    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, MAX_PATH);
    BOOL hasDedicatedPlacement = HasDedicatedSecondaryPlacement(configPath);
    int configX;
    int configY;
    if (MultiWindow_IsSecondary() && !hasDedicatedPlacement) {
        /* Preserve the existing first-launch behavior: start beside the
           primary placement, then create a dedicated secondary slot below. */
        configX = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X",
                             CLOCK_WINDOW_POS_X, configPath);
        configY = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y",
                             CLOCK_WINDOW_POS_Y, configPath);
        CLOCK_WINDOW_POSITION_MANUAL = ReadIniBool(
            INI_SECTION_DISPLAY, WINDOW_POSITION_MANUAL_KEY, FALSE, configPath);
        CLOCK_WINDOW_TASKBAR_ANCHORED = CLOCK_WINDOW_POSITION_MANUAL &&
            ReadIniBool(INI_SECTION_DISPLAY, WINDOW_TASKBAR_ANCHORED_KEY,
                        FALSE, configPath);
    } else {
        configX = ReadPlacementInt(configPath, "X", CLOCK_WINDOW_POS_X);
        configY = ReadPlacementInt(configPath, "Y", CLOCK_WINDOW_POS_Y);
        CLOCK_WINDOW_POSITION_MANUAL = ReadPlacementBool(
            configPath, "Manual", FALSE);
        CLOCK_WINDOW_TASKBAR_ANCHORED = CLOCK_WINDOW_POSITION_MANUAL &&
            ReadPlacementBool(configPath, "TaskbarAnchored", FALSE);
    }
    g_placementRetryNeeded = FALSE;
    MONITORINFO info = {0};
    WindowCore_GetPrimaryMonitorInfo(&info);
    int posX;
    int posY;
    if (!CLOCK_WINDOW_POSITION_MANUAL && IsSpecialWindowPositionX(configX)) {
        int screenWidth = info.rcMonitor.right - info.rcMonitor.left;
        if (configX == -1) {
            posX = info.rcMonitor.left + (screenWidth - width) / 2;
        } else {
            posX = info.rcMonitor.left +
                   (int)(screenWidth * 0.618f) - width / 2;
            if (posX + width > info.rcMonitor.right) {
                posX = info.rcMonitor.right - width - WINDOW_VISIBLE_MARGIN;
            }
        }
    } else {
        posX = configX;
    }
    posY = (!CLOCK_WINDOW_POSITION_MANUAL &&
            configY == DEFAULT_WINDOW_POS_Y) ? info.rcMonitor.top : configY;
    BOOL unavailable = FALSE;
    WindowCore_TryResolvePlacementMetadata(
        configPath, width, height, &posX, &posY, &unavailable);
    RECT configuredRect = {
        posX, posY,
        WindowCore_AddIntsClamped(posX, width),
        WindowCore_AddIntsClamped(posY, height)};
    BOOL visible = WindowCore_IsWindowRectVisibleOnAnyMonitor(&configuredRect);
    ClampWindowPositionToVisibleMonitor(width, height, &posX, &posY);
    g_positionTemporarilyRelocatedForDisplay =
        CLOCK_WINDOW_POSITION_MANUAL && (unavailable || !visible);
    if (g_placementRetryNeeded) g_pendingSystemPositionRestore = TRUE;
    /* A new secondary instance has no stored slot yet. Offset it once before
       its first save; later restores read that slot and do not apply this
       offset again. */
    if (MultiWindow_IsSecondary() && !hasDedicatedPlacement) {
        MultiWindow_OffsetInitialPosition(&posX, &posY);
        CLOCK_WINDOW_POSITION_MANUAL = TRUE;
    }
    *outX = posX;
    *outY = posY;
}
