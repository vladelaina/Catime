#ifndef WINDOW_CORE_INTERNAL_H
#define WINDOW_CORE_INTERNAL_H

#include "window/window_core.h"
#include "window.h"
#include "window/window_visual_effects.h"
#include "window/window_desktop_integration.h"
#include "window/window_placement.h"
#include "window_procedure/window_procedure.h"
#include "tray/tray.h"
#include "tray/tray_animation_core.h"
#include "config.h"
#include "log.h"
#include "../../resource/resource.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define WINDOW_CLASS_NAME L"CatimeWindowClass"
#define WINDOW_TITLE L"Catime"
#define COLOR_KEY_BLACK RGB(0, 0, 0)
#define ALPHA_OPAQUE 255
#define DEFAULT_TRAY_ANIMATION_SPEED_MS 150
#define SYSTEM_POSITION_GUARD_MS 3000
#define DISPLAY_RESTORE_DELAY_MS 750
#define FULLSCREEN_RESTORE_RETRY_MS 1000
#define FULLSCREEN_RECT_TOLERANCE_PX 2
#define WINDOW_VISIBLE_MARGIN 20

extern DWORD g_systemPositionGuardUntil;
extern BOOL g_pendingSystemPositionRestore;
extern BOOL g_displayRestoreDeferredForFullscreen;
extern BOOL g_positionTemporarilyRelocatedForDisplay;
extern BOOL g_placementRetryNeeded;

BOOL WindowCore_IsCurrentProcessWindow(HWND hwnd);
int WindowCore_ClampInt64ToInt(long long value);
int WindowCore_AddIntsClamped(int first, int second);
void WindowCore_GetPrimaryMonitorInfo(MONITORINFO* info);
BOOL WindowCore_IsWindowRectVisibleOnAnyMonitor(const RECT* rect);
BOOL WindowCore_GetPersistentMonitorIdW(
    const wchar_t* displayName, wchar_t* monitorId, size_t monitorIdSize);
BOOL WindowCore_FindMonitorByIdUtf8(
    const char* monitorId, HMONITOR* outMonitor, MONITORINFOEXW* outInfo);
BOOL WindowCore_GetMonitorPlacementData(
    const RECT* windowRect, char* monitorId, size_t monitorIdSize,
    int* monitorOffsetX, int* monitorOffsetY,
    BOOL* taskbarAvailable, BOOL* taskbarAnchored,
    int* taskbarAxisRatio, int* taskbarCrossOffset);
BOOL WindowCore_TryResolvePlacementMetadata(
    const char* configPath, int width, int height,
    int* posX, int* posY, BOOL* placementUnavailable);
BOOL WindowCore_IsFullscreenForegroundWindowActive(HWND hwnd);
BOOL WindowCore_ScheduleDisplayRestoreTimer(HWND hwnd, UINT delayMs);

#endif
