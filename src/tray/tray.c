/**
 * @file tray.c
 * @brief Shared state for system tray icon management.
 */

#include "tray_internal.h"

NOTIFYICONDATAW nid;
UINT WM_TASKBARCREATED = 0;

HHOOK g_mouseHook = NULL;
HWND g_mainHwnd = NULL;
HINSTANCE g_hInstance = NULL;
BOOL g_trayIconActive = FALSE;
BOOL g_trayBackgroundWorkEnabled = FALSE;
volatile LONG g_trayTooltipActive = FALSE;
BOOL g_trayTipTimerActive = FALSE;
BOOL g_traySystemMonitorActive = FALSE;
DWORD g_lastMouseHookInstallAttemptTick = 0;
DWORD g_lastMouseHookInstallWarningTick = 0;
DWORD g_lastMouseHookReleaseWarningTick = 0;
UINT g_trayRecreateRetryCount = 0;
DWORD g_lastTrayRecreateRetryTick = 0;
BOOL g_trayRecreateRetryLimitLogged = FALSE;
BOOL g_trayShuttingDown = FALSE;
BOOL g_showingOpacityTip = FALSE;
TrayHoverRectCache g_trayIconRectCache = {0};
volatile LONG g_trayInteractionSuspended = FALSE;
int g_pendingOpacityToSave = -1;
int g_opacityRollbackValue = -1;
int g_pendingOpacitySaveRetryCount = 0;
wchar_t g_lastTrayTooltip[256] = {0};
