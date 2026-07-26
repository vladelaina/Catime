/**
 * @file plugin_data_redraw.c
 * @brief UI-thread redraw coalescing for plugin data.
 */

#include "plugin_data_internal.h"

BOOL IsValidPluginDataNotifyWindow(HWND hwnd) {
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

void StopPluginDataRedrawTimer(HWND fallbackHwnd) {
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
        !IsValidPluginDataNotifyWindow(hwnd)) {
        return;
    }

    KillTimer(hwnd, PLUGIN_DATA_REDRAW_TIMER_ID);
    g_pluginDataRedrawTimerHwnd = NULL;
    InterlockedExchange(&g_pluginDataRedrawTimerArmed, 0);
    g_lastPluginDataRedrawTick = GetTickCount();
    InvalidateRect(hwnd, NULL, FALSE);
    RecheckPluginDataTimerIfQueued(hwnd);
}

void RequestPluginDataRedraw(HWND hwnd) {
    if (!IsValidPluginDataNotifyWindow(hwnd)) return;

    if (InterlockedCompareExchange(&g_pluginDataRedrawTimerArmed, 0, 0) != 0) {
        if (g_pluginDataRedrawTimerHwnd &&
            IsValidPluginDataNotifyWindow(g_pluginDataRedrawTimerHwnd)) {
            return;
        }
        StopPluginDataRedrawTimer(hwnd);
    }

    if (InterlockedCompareExchange(&g_pluginDataRedrawQueued, 1, 0) == 0) {
        if (!PostMessage(hwnd, CLOCK_WM_PLUGIN_DATA_REDRAW, 0, 0)) {
            InterlockedExchange(&g_pluginDataRedrawQueued, 0);
        }
    }
}

void PluginData_HandleRedrawRequest(HWND hwnd) {
    if (!IsValidPluginDataNotifyWindow(hwnd)) {
        InterlockedExchange(&g_pluginDataRedrawQueued, 0);
        InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0);
        return;
    }
    if (!PluginData_BeginUse()) {
        InterlockedExchange(&g_pluginDataRedrawQueued, 0);
        InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0);
        return;
    }

    InterlockedExchange(&g_pluginDataRedrawQueued, 0);
    BOOL recheckTimer = InterlockedExchange(&g_pluginDataTimerRecheckQueued, 0) != 0;

    DWORD now = GetTickCount();
    DWORD elapsed = now - g_lastPluginDataRedrawTick;
    if (g_lastPluginDataRedrawTick == 0 || elapsed >= PLUGIN_DATA_REDRAW_MIN_INTERVAL_MS) {
        StopPluginDataRedrawTimer(hwnd);
        g_lastPluginDataRedrawTick = now;
        InvalidateRect(hwnd, NULL, FALSE);
        PluginData_EndUse();
        if (recheckTimer) {
            QueuePluginDataTimerRecheck();
            RecheckPluginDataTimerIfQueued(hwnd);
        }
        return;
    }

    if (!SetTimer(hwnd, PLUGIN_DATA_REDRAW_TIMER_ID,
                  PLUGIN_DATA_REDRAW_MIN_INTERVAL_MS - elapsed,
                  PluginDataRedrawTimerProc)) {
        g_pluginDataRedrawTimerHwnd = NULL;
        InterlockedExchange(&g_pluginDataRedrawTimerArmed, 0);
        g_lastPluginDataRedrawTick = now;
        InvalidateRect(hwnd, NULL, FALSE);
    } else {
        g_pluginDataRedrawTimerHwnd = hwnd;
        InterlockedExchange(&g_pluginDataRedrawTimerArmed, 1);
    }
    PluginData_EndUse();
    if (recheckTimer) {
        QueuePluginDataTimerRecheck();
        RecheckPluginDataTimerIfQueued(hwnd);
    }
}
