/**
 * @file system_monitor_state.c
 * @brief Shared system-monitor state and inexpensive timing helpers.
 */

#include "system_monitor_internal.h"

volatile LONG g_monitorInitialized = 0;
SystemMonitorState g_monitorState = {0};
SRWLOCK g_monitorLifecycleLock = SRWLOCK_INIT;
SRWLOCK g_monitorStateLock = SRWLOCK_INIT;
SRWLOCK g_networkApiLock = SRWLOCK_INIT;
HANDLE g_networkRefreshThread = NULL;
HANDLE g_networkRefreshEvent = NULL;
HANDLE g_retiredNetworkRefreshThread = NULL;
HANDLE g_retiredNetworkRefreshEvent = NULL;
volatile LONG g_networkRefreshGeneration = 0;
ULONGLONG g_networkRefreshStartFailureCooldownUntil = 0;

LONG Monitor_IsInitialized(void) {
    return InterlockedCompareExchange(&g_monitorInitialized, 0, 0);
}

ULONGLONG Monitor_GetTickMs(void) {
    return GetTickCount64();
}

BOOL Monitor_ShouldRefresh(ULONGLONG now, ULONGLONG lastUpdateTick) {
    return lastUpdateTick == 0 ||
           now - lastUpdateTick >= g_monitorState.updateIntervalMs;
}
