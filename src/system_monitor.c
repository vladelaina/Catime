/**
 * @file system_monitor.c
 * @brief System performance monitoring with macro-driven validation
 */

#include <windows.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <stdio.h>

#include "system_monitor.h"

/* 1000ms update interval balances accuracy with minimal CPU overhead for metrics */
#define DEFAULT_UPDATE_INTERVAL_MS 1000
#define IF_TYPE_SOFTWARE_LOOPBACK 24
#define COUNTER_MAX_32BIT 0x100000000ULL

typedef struct {
    FILETIME lastIdle;
    FILETIME lastKernel;
    FILETIME lastUser;
    BOOL hasBaseline;
} CpuTimesState;

typedef struct {
    BOOL hasBaseline;
    ULONGLONG lastInOctets;
    ULONGLONG lastOutOctets;
    DWORD lastTick;
    float cachedUpBps;
    float cachedDownBps;
} NetworkState;

/** Consolidates 13 globals into 1 structure */
typedef struct {
    struct {
        CpuTimesState timesState;
        float cachedPercent;
    } cpu;
    struct {
        float cachedPercent;
    } memory;
    NetworkState network;
    DWORD updateIntervalMs;
    DWORD lastUpdateTick;
} SystemMonitorState;

static volatile LONG g_initialized = 0;
static SystemMonitorState g_state = {0};

static inline ULONGLONG FileTimeToUll(const FILETIME* ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft->dwLowDateTime;
    u.HighPart = ft->dwHighDateTime;
    return u.QuadPart;
}

static inline float ClampPercent(double value) {
    if (value < 0.0) return 0.0f;
    if (value > 100.0) return 100.0f;
    return (float)value;
}

/** Network counters are 32-bit and wrap around */
static inline ULONGLONG CalculateDelta64(ULONGLONG current, ULONGLONG previous) {
    return (current >= previous) 
        ? (current - previous)
        : (COUNTER_MAX_32BIT - previous + current);
}

static inline BOOL ShouldRefresh(void) {
    DWORD now = GetTickCount();
    return (g_state.lastUpdateTick == 0) || 
           ((now - g_state.lastUpdateTick) >= g_state.updateIntervalMs);
}

/** First call establishes baseline, second+ return deltas */
static BOOL SampleCpuUsage(float* outPercent) {
    if (!outPercent) return FALSE;

    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        return FALSE;
    }

    if (!g_state.cpu.timesState.hasBaseline) {
        g_state.cpu.timesState.lastIdle = idle;
        g_state.cpu.timesState.lastKernel = kernel;
        g_state.cpu.timesState.lastUser = user;
        g_state.cpu.timesState.hasBaseline = TRUE;
        *outPercent = 0.0f;
        return FALSE;
    }

    ULONGLONG idleNow = FileTimeToUll(&idle);
    ULONGLONG kernelNow = FileTimeToUll(&kernel);
    ULONGLONG userNow = FileTimeToUll(&user);

    ULONGLONG idlePrev = FileTimeToUll(&g_state.cpu.timesState.lastIdle);
    ULONGLONG kernelPrev = FileTimeToUll(&g_state.cpu.timesState.lastKernel);
    ULONGLONG userPrev = FileTimeToUll(&g_state.cpu.timesState.lastUser);

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
    *outPercent = ClampPercent(cpu);

    g_state.cpu.timesState.lastIdle = idle;
    g_state.cpu.timesState.lastKernel = kernel;
    g_state.cpu.timesState.lastUser = user;
    
    return TRUE;
}

static BOOL SampleMemoryUsage(float* outPercent) {
    if (!outPercent) return FALSE;
    
    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if (!GlobalMemoryStatusEx(&st)) return FALSE;
    if (st.ullTotalPhys == 0) return FALSE;
    
    ULONGLONG used = st.ullTotalPhys - st.ullAvailPhys;
    double mem = (double)used * 100.0 / (double)st.ullTotalPhys;
    *outPercent = ClampPercent(mem);
    
    return TRUE;
}

/** Aggregates all non-loopback interfaces */
static void SampleNetworkSpeed(void) {
    DWORD size = 0;
    DWORD ret = GetIfTable(NULL, &size, TRUE);
    if (ret != ERROR_INSUFFICIENT_BUFFER) {
        return;
    }
    
    MIB_IFTABLE* pTable = (MIB_IFTABLE*)malloc(size);
    if (!pTable) return;
    
    ret = GetIfTable(pTable, &size, TRUE);
    if (ret != NO_ERROR) {
        goto cleanup;
    }

    ULONGLONG inSum = 0;
    ULONGLONG outSum = 0;
    for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
        const MIB_IFROW* row = &pTable->table[i];
        if (row->dwType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        inSum += (ULONGLONG)row->dwInOctets;
        outSum += (ULONGLONG)row->dwOutOctets;
    }

    DWORD now = GetTickCount();
    
    if (!g_state.network.hasBaseline) {
        g_state.network.lastInOctets = inSum;
        g_state.network.lastOutOctets = outSum;
        g_state.network.lastTick = now;
        g_state.network.hasBaseline = TRUE;
        goto cleanup;
    }

    DWORD elapsedMs = (now >= g_state.network.lastTick) ? 
                      (now - g_state.network.lastTick) : 0;
    
    if (elapsedMs > 0) {
        ULONGLONG din = CalculateDelta64(inSum, g_state.network.lastInOctets);
        ULONGLONG dout = CalculateDelta64(outSum, g_state.network.lastOutOctets);
        
        double seconds = (double)elapsedMs / 1000.0;
        g_state.network.cachedDownBps = (float)((double)din / seconds);
        g_state.network.cachedUpBps = (float)((double)dout / seconds);
    }

    g_state.network.lastInOctets = inSum;
    g_state.network.lastOutOctets = outSum;
    g_state.network.lastTick = now;

cleanup:
    free(pTable);
}

/** Centralized refresh called by all getters */
static void RefreshCacheIfNeeded(void) {
    if (!ShouldRefresh()) {
        return;
    }

    float cpuTmp = 0.0f;
    if (SampleCpuUsage(&cpuTmp)) {
        g_state.cpu.cachedPercent = cpuTmp;
    }

    float memTmp = 0.0f;
    if (SampleMemoryUsage(&memTmp)) {
        g_state.memory.cachedPercent = memTmp;
    }

    SampleNetworkSpeed();
    g_state.lastUpdateTick = GetTickCount();
}

/** Eliminates validation boilerplate in all getters */
#define VALIDATE_AND_REFRESH(param) \
    do { \
        if (!(param)) return FALSE; \
        if (g_initialized == 0) SystemMonitor_Init(); \
        RefreshCacheIfNeeded(); \
    } while(0)

void SystemMonitor_Init(void) {
    if (InterlockedCompareExchange(&g_initialized, 1, 0) != 0) {
        return;
    }

    ZeroMemory(&g_state, sizeof(g_state));
    g_state.updateIntervalMs = DEFAULT_UPDATE_INTERVAL_MS;
}

void SystemMonitor_Shutdown(void) {
    InterlockedExchange(&g_initialized, 0);
    ZeroMemory(&g_state, sizeof(g_state));
}

void SystemMonitor_SetUpdateIntervalMs(DWORD intervalMs) {
    g_state.updateIntervalMs = (intervalMs == 0) ? DEFAULT_UPDATE_INTERVAL_MS : intervalMs;
}

void SystemMonitor_ForceRefresh(void) {
    g_state.lastUpdateTick = 0;
    RefreshCacheIfNeeded();
}

BOOL SystemMonitor_GetCpuUsage(float* outPercent) {
    VALIDATE_AND_REFRESH(outPercent);
    *outPercent = g_state.cpu.cachedPercent;
    return TRUE;
}

BOOL SystemMonitor_GetMemoryUsage(float* outPercent) {
    VALIDATE_AND_REFRESH(outPercent);
    *outPercent = g_state.memory.cachedPercent;
    return TRUE;
}

BOOL SystemMonitor_GetUsage(float* outCpuPercent, float* outMemPercent) {
    VALIDATE_AND_REFRESH(outCpuPercent);
    if (!outMemPercent) return FALSE;
    
    *outCpuPercent = g_state.cpu.cachedPercent;
    *outMemPercent = g_state.memory.cachedPercent;
    return TRUE;
}

BOOL SystemMonitor_GetNetSpeed(float* outUpBytesPerSec, float* outDownBytesPerSec) {
    VALIDATE_AND_REFRESH(outUpBytesPerSec);
    if (!outDownBytesPerSec) return FALSE;
    
    *outUpBytesPerSec = g_state.network.cachedUpBps;
    *outDownBytesPerSec = g_state.network.cachedDownBps;
    return TRUE;
}
