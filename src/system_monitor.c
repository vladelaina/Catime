/**
 * @file system_monitor.c
 * @brief Lightweight system performance monitoring (CPU and memory usage)
 */

#include <windows.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <stdio.h>

#include "../include/system_monitor.h"

/**
 * Internal state for CPU usage calculation using GetSystemTimes.
 */
typedef struct {
    FILETIME lastIdle;
    FILETIME lastKernel;
    FILETIME lastUser;
    BOOL hasBaseline;
} CpuTimesState;

static volatile LONG g_initialized = 0;
static DWORD g_updateIntervalMs = 1000; /** default 1s */
static DWORD g_lastUpdateTick = 0;
static float g_cachedCpuPercent = 0.0f;
static float g_cachedMemPercent = 0.0f;
static CpuTimesState g_cpuState;

/** Network sampling state */
static BOOL g_netHasBaseline = FALSE;
static ULONGLONG g_lastInOctetsSum = 0;
static ULONGLONG g_lastOutOctetsSum = 0;
static DWORD g_lastNetTick = 0;
static float g_cachedUpBps = 0.0f;
static float g_cachedDownBps = 0.0f;

/**
 * @brief Convert FILETIME (100ns units) to unsigned 64-bit value.
 */
static ULONGLONG FileTimeToUll(const FILETIME* ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft->dwLowDateTime;
    u.HighPart = ft->dwHighDateTime;
    return u.QuadPart;
}

/**
 * @brief Compute CPU usage since the last sample using GetSystemTimes.
 *        Returns TRUE if a valid delta was computed.
 */
static BOOL SampleCpuUsage(float* outPercent) {
    if (!outPercent) return FALSE;

    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        return FALSE;
    }

    if (!g_cpuState.hasBaseline) {
        g_cpuState.lastIdle = idle;
        g_cpuState.lastKernel = kernel;
        g_cpuState.lastUser = user;
        g_cpuState.hasBaseline = TRUE;
        *outPercent = 0.0f;
        return FALSE; /** first sample has no delta */
    }

    ULONGLONG idleNow = FileTimeToUll(&idle);
    ULONGLONG kernelNow = FileTimeToUll(&kernel);
    ULONGLONG userNow = FileTimeToUll(&user);

    ULONGLONG idlePrev = FileTimeToUll(&g_cpuState.lastIdle);
    ULONGLONG kernelPrev = FileTimeToUll(&g_cpuState.lastKernel);
    ULONGLONG userPrev = FileTimeToUll(&g_cpuState.lastUser);

    ULONGLONG idleDelta = idleNow - idlePrev;
    ULONGLONG kernelDelta = kernelNow - kernelPrev;
    ULONGLONG userDelta = userNow - userPrev;

    ULONGLONG totalDelta = kernelDelta + userDelta;
    if (totalDelta == 0) {
        *outPercent = 0.0f;
        return TRUE;
    }

    ULONGLONG busyDelta = totalDelta - idleDelta;
    double cpu = (double)busyDelta * 100.0 / (double)totalDelta;
    if (cpu < 0.0) cpu = 0.0;
    if (cpu > 100.0) cpu = 100.0;
    *outPercent = (float)cpu;

    g_cpuState.lastIdle = idle;
    g_cpuState.lastKernel = kernel;
    g_cpuState.lastUser = user;
    return TRUE;
}

/**
 * @brief Sample memory usage using GlobalMemoryStatusEx.
 */
static BOOL SampleMemoryUsage(float* outPercent) {
    if (!outPercent) return FALSE;
    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if (!GlobalMemoryStatusEx(&st)) return FALSE;
    if (st.ullTotalPhys == 0) return FALSE;
    ULONGLONG used = st.ullTotalPhys - st.ullAvailPhys;
    double mem = (double)used * 100.0 / (double)st.ullTotalPhys;
    if (mem < 0.0) mem = 0.0;
    if (mem > 100.0) mem = 100.0;
    *outPercent = (float)mem;
    return TRUE;
}

/**
 * @brief Aggregate interface octet counters and compute bytes/sec.
 */
static void SampleNetworkSpeed(void) {
    DWORD size = 0;
    MIB_IFTABLE* pTable = NULL;
    DWORD ret = GetIfTable(NULL, &size, TRUE);
    if (ret != ERROR_INSUFFICIENT_BUFFER) {
        return;
    }
    pTable = (MIB_IFTABLE*)malloc(size);
    if (!pTable) return;
    ret = GetIfTable(pTable, &size, TRUE);
    if (ret != NO_ERROR) {
        free(pTable);
        return;
    }

    ULONGLONG inSum = 0;
    ULONGLONG outSum = 0;
    for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
        const MIB_IFROW* row = &pTable->table[i];
        if (row->dwType == 24 /* IF_TYPE_SOFTWARE_LOOPBACK */) continue;
        inSum += (ULONGLONG)row->dwInOctets;
        outSum += (ULONGLONG)row->dwOutOctets;
    }

    DWORD now = GetTickCount();
    if (!g_netHasBaseline) {
        g_lastInOctetsSum = inSum;
        g_lastOutOctetsSum = outSum;
        g_lastNetTick = now;
        g_netHasBaseline = TRUE;
        free(pTable);
        return;
    }

    DWORD elapsedMs = (now >= g_lastNetTick) ? (now - g_lastNetTick) : 0;
    if (elapsedMs > 0) {
        ULONGLONG din;
        ULONGLONG dout;
        if (inSum >= g_lastInOctetsSum) din = inSum - g_lastInOctetsSum; else din = (0x100000000ULL - g_lastInOctetsSum) + inSum;
        if (outSum >= g_lastOutOctetsSum) dout = outSum - g_lastOutOctetsSum; else dout = (0x100000000ULL - g_lastOutOctetsSum) + outSum;
        double seconds = (double)elapsedMs / 1000.0;
        g_cachedDownBps = (float)((double)din / seconds);
        g_cachedUpBps = (float)((double)dout / seconds);
    }

    g_lastInOctetsSum = inSum;
    g_lastOutOctetsSum = outSum;
    g_lastNetTick = now;

    free(pTable);
}

/**
 * @brief Refresh cache if stale according to g_updateIntervalMs.
 */
static void RefreshCacheIfNeeded(void) {
    DWORD now = GetTickCount();
    if (g_lastUpdateTick != 0 && (now - g_lastUpdateTick) < g_updateIntervalMs) {
        return;
    }

    float cpu = 0.0f;
    float mem = 0.0f;

    float cpuTmp = 0.0f;
    if (SampleCpuUsage(&cpuTmp)) {
        cpu = cpuTmp;
    } else {
        /** Keep previous CPU value on first sample; produces 0 until next call */
        cpu = g_cachedCpuPercent;
    }

    if (SampleMemoryUsage(&mem)) {
        g_cachedMemPercent = mem;
    }
    g_cachedCpuPercent = cpu;
    g_lastUpdateTick = now;

    SampleNetworkSpeed();
}

void SystemMonitor_Init(void) {
    if (InterlockedCompareExchange(&g_initialized, 1, 0) == 0) {
        ZeroMemory(&g_cpuState, sizeof(g_cpuState));
        g_lastUpdateTick = 0;
        g_cachedCpuPercent = 0.0f;
        g_cachedMemPercent = 0.0f;
        g_updateIntervalMs = 1000;
        g_netHasBaseline = FALSE;
        g_lastInOctetsSum = 0;
        g_lastOutOctetsSum = 0;
        g_lastNetTick = 0;
        g_cachedUpBps = 0.0f;
        g_cachedDownBps = 0.0f;
    }
}

void SystemMonitor_Shutdown(void) {
    InterlockedExchange(&g_initialized, 0);
}

void SystemMonitor_SetUpdateIntervalMs(DWORD intervalMs) {
    if (intervalMs == 0) intervalMs = 1000;
    g_updateIntervalMs = intervalMs;
}

void SystemMonitor_ForceRefresh(void) {
    g_lastUpdateTick = 0;
    RefreshCacheIfNeeded();
}

BOOL SystemMonitor_GetCpuUsage(float* outPercent) {
    if (!outPercent) return FALSE;
    if (g_initialized == 0) SystemMonitor_Init();
    RefreshCacheIfNeeded();
    *outPercent = g_cachedCpuPercent;
    return TRUE;
}

BOOL SystemMonitor_GetMemoryUsage(float* outPercent) {
    if (!outPercent) return FALSE;
    if (g_initialized == 0) SystemMonitor_Init();
    RefreshCacheIfNeeded();
    *outPercent = g_cachedMemPercent;
    return TRUE;
}

BOOL SystemMonitor_GetUsage(float* outCpuPercent, float* outMemPercent) {
    if (!outCpuPercent || !outMemPercent) return FALSE;
    if (g_initialized == 0) SystemMonitor_Init();
    RefreshCacheIfNeeded();
    *outCpuPercent = g_cachedCpuPercent;
    *outMemPercent = g_cachedMemPercent;
    return TRUE;
}

BOOL SystemMonitor_GetNetSpeed(float* outUpBytesPerSec, float* outDownBytesPerSec) {
    if (!outUpBytesPerSec || !outDownBytesPerSec) return FALSE;
    if (g_initialized == 0) SystemMonitor_Init();
    RefreshCacheIfNeeded();
    *outUpBytesPerSec = g_cachedUpBps;
    *outDownBytesPerSec = g_cachedDownBps;
    return TRUE;
}


