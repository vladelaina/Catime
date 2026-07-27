/**
 * @file taskbar_monitor_platform.c
 * @brief Win7-safe taskbar generation detection.
 */

#include "taskbar_monitor_internal.h"

#include "utils/win32_dynamic_loader.h"

typedef LONG (WINAPI* RtlGetVersionFn)(OSVERSIONINFOW* versionInfo);

static BOOL IsWindows11OrLater(void) {
    static int cachedResult = -1;
    if (cachedResult >= 0) return cachedResult != 0;
    cachedResult = 0;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn rtlGetVersion = NULL;
    if (!ntdll) return FALSE;
    CATIME_LOAD_PROC_ADDRESS(ntdll, "RtlGetVersion", rtlGetVersion);
    if (!rtlGetVersion) return FALSE;

    OSVERSIONINFOW version = {0};
    version.dwOSVersionInfoSize = sizeof(version);
    if (rtlGetVersion(&version) == 0) {
        cachedResult = TaskbarMonitor_IsWindows11OrLaterVersion(
            version.dwMajorVersion, version.dwBuildNumber);
    }
    return cachedResult != 0;
}

BOOL TaskbarMonitor_IsModernTaskbar(HWND taskbar) {
    if (!taskbar) return FALSE;
    if (FindWindowExW(
            taskbar, NULL,
            L"Windows.UI.Composition.DesktopWindowContentBridge",
            NULL) != NULL) return TRUE;
    return IsWindows11OrLater();
}
