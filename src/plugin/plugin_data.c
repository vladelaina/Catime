/**
 * @file plugin_data.c
 * @brief Plugin data management using file monitoring
 */

#include "plugin/plugin_data.h"
#include "plugin/plugin_exit.h"
#include "config.h"
#include "notification.h"
#include "../resource/resource.h"
#include "log.h"
#include "utils/string_convert.h"
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define PLUGIN_DISPLAY_RETAIN_WCHARS 4096
#define PLUGIN_PREVIEW_MAX_EXIT_COUNTDOWN_SECONDS 3600

/* ============================================================================
 * Shared State (exported for plugin_exit.c)
 * ============================================================================ */

wchar_t* g_pluginDisplayText = NULL;
size_t g_pluginDisplayTextLen = 0;
BOOL g_hasPluginData = FALSE;

static void ClearPluginDisplayTextLocked(void) {
    if (!g_pluginDisplayText) return;

    if (g_pluginDisplayTextLen > PLUGIN_DISPLAY_RETAIN_WCHARS) {
        free(g_pluginDisplayText);
        g_pluginDisplayText = NULL;
        g_pluginDisplayTextLen = 0;
        return;
    }

    g_pluginDisplayText[0] = L'\0';
}

static BOOL EnsurePluginDisplayTextCapacityLocked(size_t requiredChars) {
    if (requiredChars == 0) return FALSE;
    if (requiredChars > SIZE_MAX / sizeof(wchar_t)) {
        LOG_ERROR("PluginData: Display buffer size overflow (%zu chars)", requiredChars);
        return FALSE;
    }

    if (g_pluginDisplayText &&
        g_pluginDisplayTextLen > PLUGIN_DISPLAY_RETAIN_WCHARS &&
        requiredChars <= PLUGIN_DISPLAY_RETAIN_WCHARS) {
        wchar_t* resized = (wchar_t*)realloc(
            g_pluginDisplayText,
            PLUGIN_DISPLAY_RETAIN_WCHARS * sizeof(wchar_t));
        if (resized) {
            g_pluginDisplayText = resized;
            g_pluginDisplayTextLen = PLUGIN_DISPLAY_RETAIN_WCHARS;
            g_pluginDisplayText[0] = L'\0';
        }
    }

    if (g_pluginDisplayText && g_pluginDisplayTextLen >= requiredChars) {
        return TRUE;
    }

    wchar_t* newBuf = (wchar_t*)realloc(g_pluginDisplayText, requiredChars * sizeof(wchar_t));
    if (!newBuf) {
        LOG_ERROR("PluginData: Failed to allocate %zu bytes", requiredChars * sizeof(wchar_t));
        return FALSE;
    }

    g_pluginDisplayText = newBuf;
    g_pluginDisplayTextLen = requiredChars;
    return TRUE;
}

/* ============================================================================
 * Internal State
 * ============================================================================ */

static BOOL g_pluginModeActive = FALSE;
static volatile LONG g_forceNextUpdate = FALSE;
static CRITICAL_SECTION g_dataCS;
static SRWLOCK g_pluginDataLifecycleLock = SRWLOCK_INIT;
static BOOL g_pluginDataInitialized = FALSE;
static BOOL g_pluginDataLocksInitialized = FALSE;
static BOOL g_pluginDataResourcesRetained = FALSE;

/* Watcher thread */
static HANDLE g_hWatchThread = NULL;
static HANDLE g_hWatchStopEvent = NULL;
static HANDLE g_hWatchWakeEvent = NULL;
static HWND g_hNotifyWnd = NULL;
static CRITICAL_SECTION g_watchCS;
static CONDITION_VARIABLE g_watchStopCompleted = CONDITION_VARIABLE_INIT;
static BOOL g_watchStopInProgress = FALSE;
static volatile LONG g_isRunning = FALSE;

#define PLUGIN_DATA_REDRAW_TIMER_ID 42424
#define PLUGIN_DATA_REDRAW_MIN_INTERVAL_MS 100
#define PLUGIN_DATA_WATCHER_SHUTDOWN_WAIT_MS 2000
#define PLUGIN_DATA_WATCHER_UI_STOP_WAIT_MS 1000
#define PLUGIN_DATA_WATCHER_STOP_GATE_WAIT_MS 1000
#define PLUGIN_DATA_WATCHER_START_FAILURE_COOLDOWN_MS 2000

static DWORD g_lastPluginDataRedrawTick = 0;
static DWORD g_watchStartFailureCooldownUntil = 0;
static volatile LONG g_pluginDataRedrawQueued = 0;
static volatile LONG g_pluginDataRedrawTimerArmed = 0;
static volatile LONG g_pluginDataTimerRecheckQueued = 0;
static HWND g_pluginDataRedrawTimerHwnd = NULL;

static BOOL PluginTextHasCatimeTagW(const wchar_t* text) {
    if (!text) return FALSE;

    const wchar_t* start = wcsstr(text, L"<catime>");
    const wchar_t* end = wcsstr(text, L"</catime>");
    return start && end && end > start;
}

static BOOL PluginDisplayHasCatimeTagLocked(void) {
    return g_pluginModeActive &&
           g_hasPluginData &&
           g_pluginDisplayText &&
           PluginTextHasCatimeTagW(g_pluginDisplayText);
}

static void QueuePluginDataTimerRecheck(void) {
    InterlockedExchange(&g_pluginDataTimerRecheckQueued, 1);
}

static BOOL PluginData_BeginUse(void) {
    AcquireSRWLockShared(&g_pluginDataLifecycleLock);
    if (!g_pluginDataInitialized) {
        ReleaseSRWLockShared(&g_pluginDataLifecycleLock);
        return FALSE;
    }
    return TRUE;
}

static void PluginData_EndUse(void) {
    ReleaseSRWLockShared(&g_pluginDataLifecycleLock);
}

static BOOL IsValidPluginDataNotifyWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static void RecheckPluginDataTimerIfQueued(HWND hwnd) {
    if (InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0) != 0 &&
        IsValidPluginDataNotifyWindow(hwnd)) {
        ResetTimerWithInterval(hwnd);
    }
}

static void StopPluginDataRedrawTimer(HWND fallbackHwnd) {
    HWND timerHwnd = g_pluginDataRedrawTimerHwnd ? g_pluginDataRedrawTimerHwnd : fallbackHwnd;
    if (InterlockedCompareExchange(&g_pluginDataRedrawTimerArmed, 0, 0) != 0 &&
        IsValidPluginDataNotifyWindow(timerHwnd)) {
        KillTimer(timerHwnd, PLUGIN_DATA_REDRAW_TIMER_ID);
    }
    g_pluginDataRedrawTimerHwnd = NULL;
    InterlockedExchange(&g_pluginDataRedrawTimerArmed, 0);
}

static void CALLBACK PluginDataRedrawTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)time;

    if (msg != WM_TIMER ||
        id != PLUGIN_DATA_REDRAW_TIMER_ID ||
        hwnd != g_pluginDataRedrawTimerHwnd ||
#include "plugin_data_part01.inc"
#include "plugin_data_part02.inc"
#include "plugin_data_part03.inc"
#include "plugin_data_part04.inc"
#include "plugin_data_part05.inc"
#include "plugin_data_part06.inc"
#include "plugin_data_part07.inc"
#include "plugin_data_part08.inc"
