/**
 * @file window_multimonitor.c
 * @brief Multi-monitor detection and window positioning
 */

#include "../../include/window/window_multimonitor.h"
#include "../../include/window/window_core.h"
#include "../../include/log.h"

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

/**
 * @brief Verify monitor is active with non-zero work area
 * @param hMonitor Monitor handle to check
 * @return TRUE if usable, FALSE otherwise
 */
static BOOL IsMonitorActive(HMONITOR hMonitor) {
    if (!hMonitor) return FALSE;
    
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    
    if (!GetMonitorInfo(hMonitor, &mi)) return FALSE;
    
    return (mi.rcWork.right > mi.rcWork.left && mi.rcWork.bottom > mi.rcWork.top);
}

/**
 * @brief Find best available monitor, preferring primary display
 * @return Handle to active monitor
 */
static HMONITOR FindBestActiveMonitor(void) {
    HMONITOR hPrimary = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    if (IsMonitorActive(hPrimary)) {
        return hPrimary;
    }
    
    LOG_WARNING("Primary monitor inactive, searching for active display");
    
    DISPLAY_DEVICEW dispDevice = {0};
    dispDevice.cb = sizeof(DISPLAY_DEVICEW);
    
    for (DWORD iDevNum = 0; EnumDisplayDevicesW(NULL, iDevNum, &dispDevice, 0); iDevNum++) {
        if (!(dispDevice.StateFlags & DISPLAY_DEVICE_ACTIVE)) continue;
        
        DEVMODEW devMode = {0};
        devMode.dmSize = sizeof(DEVMODEW);
        
        if (EnumDisplaySettingsW(dispDevice.DeviceName, ENUM_CURRENT_SETTINGS, &devMode)) {
            POINT pt = {devMode.dmPosition.x + 1, devMode.dmPosition.y + 1};
            HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);
            
            if (hMon && IsMonitorActive(hMon)) {
                LOG_INFO("Found active monitor: %d", iDevNum);
                return hMon;
            }
        }
    }
    
    LOG_WARNING("No active monitor found, falling back to primary");
    return hPrimary;
}

/**
 * @brief Check if any part of window overlaps with monitor work area
 * @param hwnd Window handle
 * @param hMonitor Monitor handle
 * @return TRUE if window intersects monitor
 */
static BOOL IsWindowVisibleOnMonitor(HWND hwnd, HMONITOR hMonitor) {
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) return FALSE;
    
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMonitor, &mi)) return FALSE;
    
    RECT intersection;
    return IntersectRect(&intersection, &rect, &mi.rcWork);
}

/**
 * @brief Center window on specified monitor with boundary clamping
 * @param hwnd Window handle
 * @param hMonitor Target monitor
 */
static void CenterWindowOnMonitor(HWND hwnd, HMONITOR hMonitor) {
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) return;
    
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMonitor, &mi)) return;
    
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    int newX = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - width) / 2;
    int newY = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - height) / 2;
    
    /* Clamp to work area bounds */
    if (newX < mi.rcWork.left) newX = mi.rcWork.left;
    if (newY < mi.rcWork.top) newY = mi.rcWork.top;
    if (newX + width > mi.rcWork.right) newX = mi.rcWork.right - width;
    if (newY + height > mi.rcWork.bottom) newY = mi.rcWork.bottom - height;
    
    SetWindowPos(hwnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    LOG_INFO("Window centered on monitor at (%d, %d)", newX, newY);
    
    SaveWindowSettings(hwnd);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void AdjustWindowPosition(HWND hwnd, BOOL forceOnScreen) {
    if (!forceOnScreen) return;
    
    HMONITOR hCurrentMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    BOOL needsReposition = FALSE;
    
    if (!hCurrentMonitor || !IsMonitorActive(hCurrentMonitor)) {
        LOG_WARNING("Window on invalid/inactive monitor, repositioning needed");
        needsReposition = TRUE;
    } 
    else if (!IsWindowVisibleOnMonitor(hwnd, hCurrentMonitor)) {
        LOG_WARNING("Window not visible on current monitor, repositioning needed");
        needsReposition = TRUE;
    }
    
    if (needsReposition) {
        HMONITOR hTargetMonitor = FindBestActiveMonitor();
        CenterWindowOnMonitor(hwnd, hTargetMonitor);
    }
}

