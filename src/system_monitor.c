/**
 * @file system_monitor.c
 * @brief Thread-safe public system-monitor lifecycle and query API.
 */

#include "system_monitor_internal.h"

static BOOL BeginMonitorUse(void) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (Monitor_IsInitialized() == 0) SystemMonitor_Init();
        AcquireSRWLockShared(&g_monitorLifecycleLock);
        if (Monitor_IsInitialized() != 0) return TRUE;
        ReleaseSRWLockShared(&g_monitorLifecycleLock);
    }
    return FALSE;
}

static void EndMonitorUse(void) {
    ReleaseSRWLockShared(&g_monitorLifecycleLock);
}

void SystemMonitor_Init(void) {
    AcquireSRWLockExclusive(&g_monitorLifecycleLock);
    if (!Monitor_CleanupRetiredWorkerLocked(
            MONITOR_NETWORK_SHUTDOWN_WAIT_MS)) {
        OutputDebugStringW(
            L"SystemMonitor: previous network refresh worker is still retiring\n");
        ReleaseSRWLockExclusive(&g_monitorLifecycleLock);
        return;
    }
    if (InterlockedCompareExchange(&g_monitorInitialized, 1, 0) != 0) {
        ReleaseSRWLockExclusive(&g_monitorLifecycleLock);
        return;
    }

    InterlockedIncrement(&g_networkRefreshGeneration);
    AcquireSRWLockExclusive(&g_monitorStateLock);
    ZeroMemory(&g_monitorState, sizeof(g_monitorState));
    g_monitorState.updateIntervalMs = MONITOR_DEFAULT_UPDATE_INTERVAL_MS;
    g_networkRefreshStartFailureCooldownUntil = 0;

    FILETIME idle;
    FILETIME kernel;
    FILETIME user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        g_monitorState.cpu.timesState.lastIdle = idle;
        g_monitorState.cpu.timesState.lastKernel = kernel;
        g_monitorState.cpu.timesState.lastUser = user;
        g_monitorState.cpu.timesState.hasBaseline = TRUE;
    }
    ULONGLONG initializedAt = Monitor_GetTickMs();
    g_monitorState.cpu.lastUpdateTick = initializedAt;
    g_monitorState.memory.lastUpdateTick = initializedAt;
    ReleaseSRWLockExclusive(&g_monitorStateLock);
    ReleaseSRWLockExclusive(&g_monitorLifecycleLock);
}

void SystemMonitor_Shutdown(void) {
    HANDLE refreshThread = NULL;
    HANDLE refreshEvent = NULL;
    BOOL refreshThreadExited = TRUE;

    AcquireSRWLockExclusive(&g_monitorLifecycleLock);
    Monitor_CleanupRetiredWorkerLocked(0);

    AcquireSRWLockExclusive(&g_monitorStateLock);
    InterlockedExchange(&g_monitorInitialized, 0);
    InterlockedIncrement(&g_networkRefreshGeneration);
    refreshThread = g_networkRefreshThread;
    refreshEvent = g_networkRefreshEvent;
    g_networkRefreshThread = NULL;
    g_networkRefreshEvent = NULL;
    ZeroMemory(&g_monitorState, sizeof(g_monitorState));
    ReleaseSRWLockExclusive(&g_monitorStateLock);

    if (refreshEvent) SetEvent(refreshEvent);
    if (refreshThread) {
        DWORD waitResult = WaitForSingleObject(
            refreshThread, MONITOR_NETWORK_SHUTDOWN_WAIT_MS);
        refreshThreadExited = waitResult == WAIT_OBJECT_0;
        if (!refreshThreadExited) {
            if (waitResult == WAIT_TIMEOUT) {
                OutputDebugStringW(
                    L"SystemMonitor: network refresh worker did not exit before shutdown timeout\n");
            }
            if (Monitor_CleanupRetiredWorkerLocked(0)) {
                g_retiredNetworkRefreshThread = refreshThread;
                g_retiredNetworkRefreshEvent = refreshEvent;
            } else {
                CloseHandle(refreshThread);
                if (refreshEvent) CloseHandle(refreshEvent);
            }
            refreshThread = NULL;
            refreshEvent = NULL;
        }
    }
    if (refreshThread) CloseHandle(refreshThread);
    if (refreshEvent) CloseHandle(refreshEvent);

    if (refreshThreadExited && !g_retiredNetworkRefreshThread) {
        Monitor_ReleaseNetworkApiResources();
    }
    ReleaseSRWLockExclusive(&g_monitorLifecycleLock);
}

BOOL SystemMonitor_IsInitialized(void) {
    return Monitor_IsInitialized() != 0;
}

void SystemMonitor_SetUpdateIntervalMs(DWORD intervalMs) {
    if (!BeginMonitorUse()) return;
    AcquireSRWLockExclusive(&g_monitorStateLock);
    if (Monitor_IsInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_monitorStateLock);
        EndMonitorUse();
        return;
    }

    if (intervalMs == 0) {
        g_monitorState.updateIntervalMs = MONITOR_DEFAULT_UPDATE_INTERVAL_MS;
    } else if (intervalMs < MONITOR_MIN_UPDATE_INTERVAL_MS) {
        g_monitorState.updateIntervalMs = MONITOR_MIN_UPDATE_INTERVAL_MS;
    } else {
        g_monitorState.updateIntervalMs = intervalMs;
    }
    ReleaseSRWLockExclusive(&g_monitorStateLock);
    EndMonitorUse();
}

void SystemMonitor_ForceRefresh(void) {
    if (!BeginMonitorUse()) return;
    AcquireSRWLockExclusive(&g_monitorStateLock);
    if (Monitor_IsInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_monitorStateLock);
        EndMonitorUse();
        return;
    }

    g_monitorState.cpu.lastUpdateTick = 0;
    g_monitorState.memory.lastUpdateTick = 0;
    Monitor_RefreshBasicCacheIfNeeded();
    if (Monitor_EnsureNetworkWorkerStarted()) {
        g_monitorState.network.lastPollAttemptTick = 0;
        Monitor_StartNetworkRefreshIfNeeded();
    }
    ReleaseSRWLockExclusive(&g_monitorStateLock);
    EndMonitorUse();
}

BOOL SystemMonitor_GetCpuUsage(float* outPercent) {
    if (!outPercent) return FALSE;
    *outPercent = 0.0f;
    if (!BeginMonitorUse()) return FALSE;

    AcquireSRWLockExclusive(&g_monitorStateLock);
    BOOL available = Monitor_IsInitialized() != 0;
    if (available) {
        Monitor_RefreshBasicCacheIfNeeded();
        *outPercent = g_monitorState.cpu.cachedPercent;
    }
    ReleaseSRWLockExclusive(&g_monitorStateLock);
    EndMonitorUse();
    return available;
}

BOOL SystemMonitor_GetMemoryUsage(float* outPercent) {
    if (!outPercent) return FALSE;
    *outPercent = 0.0f;
    if (!BeginMonitorUse()) return FALSE;

    AcquireSRWLockExclusive(&g_monitorStateLock);
    BOOL available = Monitor_IsInitialized() != 0;
    if (available) {
        Monitor_RefreshBasicCacheIfNeeded();
        *outPercent = g_monitorState.memory.cachedPercent;
    }
    ReleaseSRWLockExclusive(&g_monitorStateLock);
    EndMonitorUse();
    return available;
}

BOOL SystemMonitor_GetSnapshot(DWORD fields,
                               SystemMonitorSnapshot* outSnapshot) {
    if (!outSnapshot) return FALSE;
    ZeroMemory(outSnapshot, sizeof(*outSnapshot));
    if (!BeginMonitorUse()) return FALSE;

    AcquireSRWLockExclusive(&g_monitorStateLock);
    BOOL available = Monitor_IsInitialized() != 0;
    if (available) {
        if (fields & SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY) {
            Monitor_RefreshBasicCacheIfNeeded();
        }
        if ((fields & SYSTEM_MONITOR_SNAPSHOT_NETWORK) &&
            Monitor_EnsureNetworkWorkerStarted()) {
            Monitor_StartNetworkRefreshIfNeeded();
        }

        outSnapshot->cpuPercent = g_monitorState.cpu.cachedPercent;
        outSnapshot->memoryPercent = g_monitorState.memory.cachedPercent;
        outSnapshot->uploadBytesPerSecond =
            g_monitorState.network.cachedUpBps;
        outSnapshot->downloadBytesPerSecond =
            g_monitorState.network.cachedDownBps;
        outSnapshot->revision = g_monitorState.snapshotRevision;
        outSnapshot->basicSampleTick =
            g_monitorState.cpu.lastUpdateTick;
        outSnapshot->networkSampleTick =
            g_monitorState.network.lastTick;
        outSnapshot->cpuAvailable =
            g_monitorState.cpu.sampleAvailable;
        outSnapshot->memoryAvailable =
            g_monitorState.memory.sampleAvailable;
        outSnapshot->networkAvailable =
            g_monitorState.network.sampleAvailable;
    }
    ReleaseSRWLockExclusive(&g_monitorStateLock);
    EndMonitorUse();
    return available;
}

BOOL SystemMonitor_GetUsage(float* outCpuPercent, float* outMemPercent) {
    if (!outCpuPercent || !outMemPercent) return FALSE;
    *outCpuPercent = 0.0f;
    *outMemPercent = 0.0f;
    if (!BeginMonitorUse()) return FALSE;

    AcquireSRWLockExclusive(&g_monitorStateLock);
    BOOL available = Monitor_IsInitialized() != 0;
    if (available) {
        Monitor_RefreshBasicCacheIfNeeded();
        *outCpuPercent = g_monitorState.cpu.cachedPercent;
        *outMemPercent = g_monitorState.memory.cachedPercent;
    }
    ReleaseSRWLockExclusive(&g_monitorStateLock);
    EndMonitorUse();
    return available;
}

BOOL SystemMonitor_GetNetSpeed(float* outUpBytesPerSec,
                               float* outDownBytesPerSec) {
    if (!outUpBytesPerSec || !outDownBytesPerSec) return FALSE;
    *outUpBytesPerSec = 0.0f;
    *outDownBytesPerSec = 0.0f;
    if (!BeginMonitorUse()) return FALSE;

    AcquireSRWLockExclusive(&g_monitorStateLock);
    BOOL available = Monitor_IsInitialized() != 0 &&
                     Monitor_EnsureNetworkWorkerStarted();
    if (available) {
        Monitor_StartNetworkRefreshIfNeeded();
        available = g_monitorState.network.sampleAvailable;
    }
    if (available) {
        *outUpBytesPerSec = g_monitorState.network.cachedUpBps;
        *outDownBytesPerSec = g_monitorState.network.cachedDownBps;
    }
    ReleaseSRWLockExclusive(&g_monitorStateLock);
    EndMonitorUse();
    return available;
}

BOOL SystemMonitor_GetBatteryPercent(int* outPercent) {
    if (!outPercent) return FALSE;
    SYSTEM_POWER_STATUS status;
    if (!GetSystemPowerStatus(&status) || status.BatteryFlag == 128 ||
        status.BatteryLifePercent == 255) {
        *outPercent = -1;
        return FALSE;
    }
    *outPercent = (int)status.BatteryLifePercent;
    if (*outPercent > 100) *outPercent = 100;
    if (*outPercent < 0) *outPercent = 0;
    return TRUE;
}
