/**
 * @file system_monitor.c
 * @brief System performance monitoring with macro-driven validation
 */

#include <windows.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string.h>

#include "system_monitor.h"

/* 1000ms update interval balances accuracy with minimal CPU overhead for metrics */
#define DEFAULT_UPDATE_INTERVAL_MS 1000
#define IF_TYPE_SOFTWARE_LOOPBACK 24
#define COUNTER_MAX_32BIT 0x100000000ULL
#define MAX_REASONABLE_RATE_BPS 100000000000.0 /* 100 GB/s guardrail for reset/wrap anomalies */
#define MAX_TRACKED_INTERFACES 256

typedef struct {
    FILETIME lastIdle;
    FILETIME lastKernel;
    FILETIME lastUser;
    BOOL hasBaseline;
} CpuTimesState;

typedef struct {
    DWORD index;
    ULONGLONG inOctets;
    ULONGLONG outOctets;
} NetInterfaceCounter;

typedef struct {
    BOOL hasBaseline;
    ULONGLONG lastTick;
    NetInterfaceCounter lastCounters[MAX_TRACKED_INTERFACES];
    DWORD lastCounterCount;
    float cachedUpBps;
    float cachedDownBps;
    BOOL sampleAvailable;
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
    ULONGLONG lastUpdateTick;
} SystemMonitorState;

static volatile LONG g_initialized = 0;
static SystemMonitorState g_state = {0};
static SRWLOCK g_stateLock = SRWLOCK_INIT;

typedef enum {
    CPU_SAMPLE_ERROR = 0,
    CPU_SAMPLE_BASELINE_ONLY,
    CPU_SAMPLE_OK
} CpuSampleResult;

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
static inline ULONGLONG CalculateDelta32(ULONGLONG current, ULONGLONG previous) {
    return (current >= previous) 
        ? (current - previous)
        : (COUNTER_MAX_32BIT - previous + current);
}

static inline ULONGLONG GetMonotonicTickMs(void) {
    return GetTickCount64();
}

static inline BOOL ShouldRefresh(void) {
    ULONGLONG now = GetMonotonicTickMs();
    return (g_state.lastUpdateTick == 0) || 
           ((now - g_state.lastUpdateTick) >= g_state.updateIntervalMs);
}

/** First call establishes baseline, second+ return deltas */
static CpuSampleResult SampleCpuUsage(float* outPercent) {
    if (!outPercent) return CPU_SAMPLE_ERROR;

    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        return CPU_SAMPLE_ERROR;
    }

    if (!g_state.cpu.timesState.hasBaseline) {
        g_state.cpu.timesState.lastIdle = idle;
        g_state.cpu.timesState.lastKernel = kernel;
        g_state.cpu.timesState.lastUser = user;
        g_state.cpu.timesState.hasBaseline = TRUE;
        *outPercent = 0.0f;
        return CPU_SAMPLE_BASELINE_ONLY;
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
        return CPU_SAMPLE_OK;
    }

    ULONGLONG busyDelta = totalDelta - idleDelta;
    double cpu = (double)busyDelta * 100.0 / (double)totalDelta;
    *outPercent = ClampPercent(cpu);

    g_state.cpu.timesState.lastIdle = idle;
    g_state.cpu.timesState.lastKernel = kernel;
    g_state.cpu.timesState.lastUser = user;
    
    return CPU_SAMPLE_OK;
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

/** Collect active non-loopback interface counters (32-bit counters) */
static BOOL CollectNetworkCounters32(NetInterfaceCounter* outCounters, DWORD* outCount) {
    if (!outCounters || !outCount) return FALSE;
    *outCount = 0;

    DWORD size = 0;
    DWORD ret = GetIfTable(NULL, &size, TRUE);
    if (ret != ERROR_INSUFFICIENT_BUFFER) {
        return FALSE;
    }
    
    MIB_IFTABLE* pTable = (MIB_IFTABLE*)malloc(size);
    if (!pTable) return FALSE;
    
    ret = GetIfTable(pTable, &size, TRUE);
    if (ret != NO_ERROR) {
        free(pTable);
        return FALSE;
    }

    DWORD count = 0;
    for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
        const MIB_IFROW* row = &pTable->table[i];
        if (row->dwType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
#ifdef IF_OPER_STATUS_OPERATIONAL
        if (row->dwOperStatus != IF_OPER_STATUS_OPERATIONAL) continue;
#endif

        if (count < MAX_TRACKED_INTERFACES) {
            outCounters[count].index = row->dwIndex;
            outCounters[count].inOctets = (ULONGLONG)row->dwInOctets;
            outCounters[count].outOctets = (ULONGLONG)row->dwOutOctets;
            ++count;
        }
    }

    free(pTable);
    *outCount = count;
    return TRUE;
}

static const NetInterfaceCounter* FindCounterByIndex(const NetInterfaceCounter* counters, DWORD count, DWORD index) {
    for (DWORD i = 0; i < count; ++i) {
        if (counters[i].index == index) {
            return &counters[i];
        }
    }
    return NULL;
}

static void SetNetworkBaseline(const NetInterfaceCounter* counters, DWORD count, ULONGLONG now) {
    DWORD clipped = (count > MAX_TRACKED_INTERFACES) ? MAX_TRACKED_INTERFACES : count;
    g_state.network.lastCounterCount = clipped;
    if (clipped > 0) {
        memcpy(g_state.network.lastCounters, counters, sizeof(NetInterfaceCounter) * clipped);
    }
    g_state.network.lastTick = now;
    g_state.network.hasBaseline = TRUE;
    g_state.network.sampleAvailable = TRUE;
}

/** Aggregates active interfaces and computes B/s from per-interface deltas */
static void SampleNetworkSpeed(void) {
    NetInterfaceCounter currentCounters[MAX_TRACKED_INTERFACES];
    DWORD currentCount = 0;
    if (!CollectNetworkCounters32(currentCounters, &currentCount)) {
        g_state.network.sampleAvailable = FALSE;
        g_state.network.cachedDownBps = 0.0f;
        g_state.network.cachedUpBps = 0.0f;
        return;
    }

    ULONGLONG now = GetMonotonicTickMs();

    if (!g_state.network.hasBaseline) {
        SetNetworkBaseline(currentCounters, currentCount, now);
        g_state.network.cachedDownBps = 0.0f;
        g_state.network.cachedUpBps = 0.0f;
        return;
    }

    ULONGLONG elapsedMs = (now >= g_state.network.lastTick) ?
                          (now - g_state.network.lastTick) : 0;
    
    if (elapsedMs > 0) {
        ULONGLONG totalInDelta = 0;
        ULONGLONG totalOutDelta = 0;

        for (DWORD i = 0; i < currentCount; ++i) {
            const NetInterfaceCounter* cur = &currentCounters[i];
            const NetInterfaceCounter* prev = FindCounterByIndex(g_state.network.lastCounters, g_state.network.lastCounterCount, cur->index);
            if (!prev) {
                /* New interface: establish baseline for this adapter first. */
                continue;
            }
            totalInDelta += CalculateDelta32(cur->inOctets, prev->inOctets);
            totalOutDelta += CalculateDelta32(cur->outOctets, prev->outOctets);
        }
        
        double seconds = (double)elapsedMs / 1000.0;
        double downBps = (double)totalInDelta / seconds;
        double upBps = (double)totalOutDelta / seconds;

        /* Guard against reset/wrap artifacts from legacy counters. */
        if (downBps <= MAX_REASONABLE_RATE_BPS && upBps <= MAX_REASONABLE_RATE_BPS) {
            g_state.network.cachedDownBps = (float)downBps;
            g_state.network.cachedUpBps = (float)upBps;
        } else {
            g_state.network.cachedDownBps = 0.0f;
            g_state.network.cachedUpBps = 0.0f;
        }
    }

    SetNetworkBaseline(currentCounters, currentCount, now);
}

/** Centralized refresh called by all getters */
static void RefreshCacheIfNeeded(void) {
    if (!ShouldRefresh()) {
        return;
    }

    float cpuTmp = 0.0f;
    CpuSampleResult cpuStatus = SampleCpuUsage(&cpuTmp);
    if (cpuStatus == CPU_SAMPLE_OK) {
        g_state.cpu.cachedPercent = cpuTmp;
    }

    float memTmp = 0.0f;
    if (SampleMemoryUsage(&memTmp)) {
        g_state.memory.cachedPercent = memTmp;
    }

    SampleNetworkSpeed();
    /* Baseline creation requires an immediate follow-up sample to avoid startup 0%. */
    if (cpuStatus == CPU_SAMPLE_BASELINE_ONLY) {
        g_state.lastUpdateTick = 0;
    } else {
        g_state.lastUpdateTick = GetMonotonicTickMs();
    }
}

static inline LONG IsMonitorInitialized(void) {
    return InterlockedCompareExchange(&g_initialized, 0, 0);
}

void SystemMonitor_Init(void) {
    if (InterlockedCompareExchange(&g_initialized, 1, 0) != 0) {
        return;
    }

    AcquireSRWLockExclusive(&g_stateLock);
    ZeroMemory(&g_state, sizeof(g_state));
    g_state.updateIntervalMs = DEFAULT_UPDATE_INTERVAL_MS;
    /*
     * Prime CPU baseline during init so the next caller can obtain a
     * delta sample instead of always seeing startup 0%.
     */
    {
        FILETIME idle, kernel, user;
        if (GetSystemTimes(&idle, &kernel, &user)) {
            g_state.cpu.timesState.lastIdle = idle;
            g_state.cpu.timesState.lastKernel = kernel;
            g_state.cpu.timesState.lastUser = user;
            g_state.cpu.timesState.hasBaseline = TRUE;
        }
    }
    ReleaseSRWLockExclusive(&g_stateLock);
}

void SystemMonitor_Shutdown(void) {
    AcquireSRWLockExclusive(&g_stateLock);
    InterlockedExchange(&g_initialized, 0);
    ZeroMemory(&g_state, sizeof(g_state));
    ReleaseSRWLockExclusive(&g_stateLock);
}

void SystemMonitor_SetUpdateIntervalMs(DWORD intervalMs) {
    if (IsMonitorInitialized() == 0) {
        SystemMonitor_Init();
    }
    AcquireSRWLockExclusive(&g_stateLock);
    if (IsMonitorInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_stateLock);
        return;
    }
    g_state.updateIntervalMs = (intervalMs == 0) ? DEFAULT_UPDATE_INTERVAL_MS : intervalMs;
    ReleaseSRWLockExclusive(&g_stateLock);
}

void SystemMonitor_ForceRefresh(void) {
    if (IsMonitorInitialized() == 0) {
        SystemMonitor_Init();
    }
    AcquireSRWLockExclusive(&g_stateLock);
    if (IsMonitorInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_stateLock);
        return;
    }
    g_state.lastUpdateTick = 0;
    RefreshCacheIfNeeded();
    ReleaseSRWLockExclusive(&g_stateLock);
}

BOOL SystemMonitor_GetCpuUsage(float* outPercent) {
    if (!outPercent) return FALSE;
    *outPercent = 0.0f;
    if (IsMonitorInitialized() == 0) {
        SystemMonitor_Init();
    }
    AcquireSRWLockExclusive(&g_stateLock);
    if (IsMonitorInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_stateLock);
        return FALSE;
    }
    RefreshCacheIfNeeded();
    *outPercent = g_state.cpu.cachedPercent;
    ReleaseSRWLockExclusive(&g_stateLock);
    return TRUE;
}

BOOL SystemMonitor_GetMemoryUsage(float* outPercent) {
    if (!outPercent) return FALSE;
    *outPercent = 0.0f;
    if (IsMonitorInitialized() == 0) {
        SystemMonitor_Init();
    }
    AcquireSRWLockExclusive(&g_stateLock);
    if (IsMonitorInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_stateLock);
        return FALSE;
    }
    RefreshCacheIfNeeded();
    *outPercent = g_state.memory.cachedPercent;
    ReleaseSRWLockExclusive(&g_stateLock);
    return TRUE;
}

BOOL SystemMonitor_GetUsage(float* outCpuPercent, float* outMemPercent) {
    if (!outMemPercent) return FALSE;
    if (!outCpuPercent) return FALSE;
    *outCpuPercent = 0.0f;
    *outMemPercent = 0.0f;
    if (IsMonitorInitialized() == 0) {
        SystemMonitor_Init();
    }
    AcquireSRWLockExclusive(&g_stateLock);
    if (IsMonitorInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_stateLock);
        return FALSE;
    }
    RefreshCacheIfNeeded();
    
    *outCpuPercent = g_state.cpu.cachedPercent;
    *outMemPercent = g_state.memory.cachedPercent;
    ReleaseSRWLockExclusive(&g_stateLock);
    return TRUE;
}

BOOL SystemMonitor_GetNetSpeed(float* outUpBytesPerSec, float* outDownBytesPerSec) {
    if (!outDownBytesPerSec) return FALSE;
    if (!outUpBytesPerSec) return FALSE;
    *outUpBytesPerSec = 0.0f;
    *outDownBytesPerSec = 0.0f;
    if (IsMonitorInitialized() == 0) {
        SystemMonitor_Init();
    }
    AcquireSRWLockExclusive(&g_stateLock);
    if (IsMonitorInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_stateLock);
        return FALSE;
    }
    RefreshCacheIfNeeded();
    
    if (!g_state.network.sampleAvailable) {
        ReleaseSRWLockExclusive(&g_stateLock);
        return FALSE;
    }
    *outUpBytesPerSec = g_state.network.cachedUpBps;
    *outDownBytesPerSec = g_state.network.cachedDownBps;
    ReleaseSRWLockExclusive(&g_stateLock);
    return TRUE;
}

BOOL SystemMonitor_GetBatteryPercent(int* outPercent) {
    if (!outPercent) return FALSE;
    
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) {
        *outPercent = -1;
        return FALSE;
    }
    
    if (sps.BatteryFlag == 128 || sps.BatteryLifePercent == 255) {
        *outPercent = -1;
        return FALSE;
    }
    
    *outPercent = (int)sps.BatteryLifePercent;
    if (*outPercent > 100) *outPercent = 100;
    if (*outPercent < 0) *outPercent = 0;
    
    return TRUE;
}
