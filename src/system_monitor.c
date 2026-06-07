/**
 * @file system_monitor.c
 * @brief System performance monitoring with macro-driven validation
 */

#include <winsock2.h>
#include <windows.h>
#include <psapi.h>
#include <ifdef.h>
#include <netioapi.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system_monitor.h"

/* 1000ms update interval balances accuracy with minimal CPU overhead for metrics */
#define DEFAULT_UPDATE_INTERVAL_MS 1000
#define IF_TYPE_SOFTWARE_LOOPBACK 24
#define COUNTER_MAX_32BIT 0x100000000ULL
#define MAX_REASONABLE_RATE_BPS 100000000000.0 /* 100 GB/s guardrail for reset/wrap anomalies */
#define MAX_TRACKED_INTERFACES 256
#define MAX_NETWORK_IF_TABLE_BYTES (1024u * 1024u)
#define NETWORK_REFRESH_START_FAILURE_COOLDOWN_MS 2000

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
    HANDLE refreshEvent;
    LONG generation;
} NetworkRefreshWorkerContext;

typedef struct {
    BOOL hasBaseline;
    ULONGLONG lastTick;
    ULONGLONG lastPollAttemptTick;
    NetInterfaceCounter lastCounters[MAX_TRACKED_INTERFACES];
    DWORD lastCounterCount;
    float cachedUpBps;
    float cachedDownBps;
    BOOL sampleAvailable;
    BOOL refreshInProgress;
} NetworkState;

/** Consolidates 13 globals into 1 structure */
typedef struct {
    struct {
        CpuTimesState timesState;
        float cachedPercent;
        ULONGLONG lastUpdateTick;
    } cpu;
    struct {
        float cachedPercent;
        ULONGLONG lastUpdateTick;
    } memory;
    NetworkState network;
    DWORD updateIntervalMs;
} SystemMonitorState;

static volatile LONG g_initialized = 0;
static SystemMonitorState g_state = {0};
static SRWLOCK g_monitorLifecycleLock = SRWLOCK_INIT;
static SRWLOCK g_stateLock = SRWLOCK_INIT;
static SRWLOCK g_networkApiLock = SRWLOCK_INIT;
static HANDLE g_networkRefreshThread = NULL;
static HANDLE g_networkRefreshEvent = NULL;
static HANDLE g_retiredNetworkRefreshThread = NULL;
static HANDLE g_retiredNetworkRefreshEvent = NULL;
static volatile LONG g_networkRefreshGeneration = 0;
static DWORD g_networkRefreshLastStartFailureTick = 0;

#define NETWORK_REFRESH_SHUTDOWN_WAIT_MS 2000

static inline LONG IsMonitorInitialized(void);

typedef enum {
    CPU_SAMPLE_ERROR = 0,
    CPU_SAMPLE_BASELINE_ONLY,
    CPU_SAMPLE_OK
} CpuSampleResult;

static inline ULONGLONG FileTimeToUll(const FILETIME* ft) {
    return ((ULONGLONG)ft->dwHighDateTime << 32) | (ULONGLONG)ft->dwLowDateTime;
}

static inline float ClampPercent(double value) {
    if (value < 0.0) return 0.0f;
    if (value > 100.0) return 100.0f;
    return (float)value;
}

/** Network counters from GetIfTable are 32-bit and wrap around. */
static inline ULONGLONG CalculateDelta32(ULONGLONG current, ULONGLONG previous) {
    return (current >= previous)
        ? (current - previous)
        : (COUNTER_MAX_32BIT - previous + current);
}

/** 64-bit counters should not wrap in practice; decreasing values indicate reset. */
static inline BOOL CalculateDelta64(ULONGLONG current, ULONGLONG previous, ULONGLONG* outDelta) {
    if (!outDelta || current < previous) return FALSE;
    *outDelta = current - previous;
    return TRUE;
}

static inline ULONGLONG GetMonotonicTickMs(void) {
    return GetTickCount64();
}

static inline BOOL ShouldRefreshTick(ULONGLONG now, ULONGLONG lastUpdateTick) {
    return (lastUpdateTick == 0) ||
           ((now - lastUpdateTick) >= g_state.updateIntervalMs);
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

    if (idleNow < idlePrev || kernelNow < kernelPrev || userNow < userPrev) {
        g_state.cpu.timesState.lastIdle = idle;
        g_state.cpu.timesState.lastKernel = kernel;
        g_state.cpu.timesState.lastUser = user;
        return CPU_SAMPLE_ERROR;
    }

    ULONGLONG idleDelta = idleNow - idlePrev;
    ULONGLONG kernelDelta = kernelNow - kernelPrev;
    ULONGLONG userDelta = userNow - userPrev;
    ULONGLONG totalDelta = kernelDelta + userDelta;

    if (totalDelta == 0) {
        *outPercent = g_state.cpu.cachedPercent;
        return CPU_SAMPLE_OK;
    }

    if (idleDelta > totalDelta) {
        g_state.cpu.timesState.lastIdle = idle;
        g_state.cpu.timesState.lastKernel = kernel;
        g_state.cpu.timesState.lastUser = user;
        return CPU_SAMPLE_ERROR;
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

typedef NETIO_STATUS (WINAPI *GetIfTable2Fn)(PMIB_IF_TABLE2* Table);
typedef VOID (WINAPI *FreeMibTableFn)(PVOID Memory);

static HMODULE g_iphlpapiModule = NULL;
static GetIfTable2Fn g_getIfTable2 = NULL;
static FreeMibTableFn g_freeMibTable = NULL;
static BOOL g_netioApiResolved = FALSE;
static BOOL g_iphlpapiLoadedByUs = FALSE;

static BOOL ResolveNetworkApi64(void) {
    if (g_netioApiResolved) {
        return g_getIfTable2 && g_freeMibTable;
    }

    g_netioApiResolved = TRUE;
    g_iphlpapiModule = GetModuleHandleW(L"iphlpapi.dll");
    if (!g_iphlpapiModule) {
        g_iphlpapiModule = LoadLibraryW(L"iphlpapi.dll");
        g_iphlpapiLoadedByUs = (g_iphlpapiModule != NULL);
    }
    if (!g_iphlpapiModule) return FALSE;

    FARPROC getIfTable2Proc = GetProcAddress(g_iphlpapiModule, "GetIfTable2");
    FARPROC freeMibTableProc = GetProcAddress(g_iphlpapiModule, "FreeMibTable");
    memcpy(&g_getIfTable2, &getIfTable2Proc, sizeof(g_getIfTable2));
    memcpy(&g_freeMibTable, &freeMibTableProc, sizeof(g_freeMibTable));
    return g_getIfTable2 && g_freeMibTable;
}

static void ReleaseNetworkApiResources(void) {
    AcquireSRWLockExclusive(&g_networkApiLock);
    g_getIfTable2 = NULL;
    g_freeMibTable = NULL;
    g_netioApiResolved = FALSE;
    if (g_iphlpapiLoadedByUs && g_iphlpapiModule) {
        FreeLibrary(g_iphlpapiModule);
    }
    g_iphlpapiModule = NULL;
    g_iphlpapiLoadedByUs = FALSE;
    ReleaseSRWLockExclusive(&g_networkApiLock);
}

static BOOL CollectNetworkCounters64(NetInterfaceCounter* outCounters, DWORD* outCount) {
    if (!outCounters || !outCount) return FALSE;
    *outCount = 0;

    BOOL success = FALSE;
    AcquireSRWLockExclusive(&g_networkApiLock);
    do {
        if (InterlockedCompareExchange(&g_initialized, 0, 0) == 0) {
            break;
        }
        if (!ResolveNetworkApi64()) {
            break;
        }

        PMIB_IF_TABLE2 table = NULL;
        if (g_getIfTable2(&table) != NO_ERROR || !table) {
            break;
        }

        DWORD count = 0;
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            if (InterlockedCompareExchange(&g_initialized, 0, 0) == 0) {
                break;
            }
            const MIB_IF_ROW2* row = &table->Table[i];
            if (row->Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (row->OperStatus != IfOperStatusUp) continue;

            if (count < MAX_TRACKED_INTERFACES) {
                outCounters[count].index = row->InterfaceIndex;
                outCounters[count].inOctets = row->InOctets;
                outCounters[count].outOctets = row->OutOctets;
                ++count;
            }
        }

        g_freeMibTable(table);
        *outCount = count;
        success = TRUE;
    } while (0);
    ReleaseSRWLockExclusive(&g_networkApiLock);

    return success;
}

/** Collect active non-loopback interface counters (32-bit fallback). */
static BOOL CollectNetworkCounters32(NetInterfaceCounter* outCounters, DWORD* outCount) {
    if (!outCounters || !outCount) return FALSE;
    *outCount = 0;
    if (InterlockedCompareExchange(&g_initialized, 0, 0) == 0) return FALSE;

    DWORD size = 0;
    DWORD ret = GetIfTable(NULL, &size, TRUE);
    if (ret != ERROR_INSUFFICIENT_BUFFER) {
        return FALSE;
    }
    if (size == 0 || size > MAX_NETWORK_IF_TABLE_BYTES) {
        return FALSE;
    }

    MIB_IFTABLE* pTable = (MIB_IFTABLE*)malloc(size);
    if (!pTable) return FALSE;

    if (InterlockedCompareExchange(&g_initialized, 0, 0) == 0) {
        free(pTable);
        return FALSE;
    }

    ret = GetIfTable(pTable, &size, TRUE);
    if (ret != NO_ERROR) {
        free(pTable);
        return FALSE;
    }
    if (size > MAX_NETWORK_IF_TABLE_BYTES ||
        size < (DWORD)FIELD_OFFSET(MIB_IFTABLE, table)) {
        free(pTable);
        return FALSE;
    }

    DWORD count = 0;
    DWORD maxRowsInBuffer =
        (size - (DWORD)FIELD_OFFSET(MIB_IFTABLE, table)) / sizeof(MIB_IFROW);
    DWORD rowsToScan = (pTable->dwNumEntries < maxRowsInBuffer) ? pTable->dwNumEntries : maxRowsInBuffer;
    for (DWORD i = 0; i < rowsToScan; ++i) {
        if (InterlockedCompareExchange(&g_initialized, 0, 0) == 0) {
            free(pTable);
            return FALSE;
        }
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

static BOOL CollectNetworkCounters(NetInterfaceCounter* outCounters, DWORD* outCount, BOOL* outIs64Bit) {
    if (!outCounters || !outCount || !outIs64Bit) return FALSE;
    if (InterlockedCompareExchange(&g_initialized, 0, 0) == 0) return FALSE;

    if (CollectNetworkCounters64(outCounters, outCount)) {
        *outIs64Bit = TRUE;
        return TRUE;
    }

    if (InterlockedCompareExchange(&g_initialized, 0, 0) == 0) return FALSE;

    *outIs64Bit = FALSE;
    return CollectNetworkCounters32(outCounters, outCount);
}

static int CompareNetInterfaceCounterByIndex(const void* lhs, const void* rhs) {
    const NetInterfaceCounter* a = (const NetInterfaceCounter*)lhs;
    const NetInterfaceCounter* b = (const NetInterfaceCounter*)rhs;
    if (a->index < b->index) return -1;
    if (a->index > b->index) return 1;
    return 0;
}

static const NetInterfaceCounter* FindCounterByIndex(const NetInterfaceCounter* counters, DWORD count, DWORD index) {
    DWORD low = 0;
    DWORD high = count;

    while (low < high) {
        DWORD mid = low + ((high - low) / 2);
        DWORD midIndex = counters[mid].index;
        if (midIndex == index) {
            return &counters[mid];
        }
        if (midIndex < index) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return NULL;
}

static void SetNetworkBaseline(const NetInterfaceCounter* counters, DWORD count, ULONGLONG now) {
    DWORD clipped = (count > MAX_TRACKED_INTERFACES) ? MAX_TRACKED_INTERFACES : count;
    g_state.network.lastCounterCount = clipped;
    if (clipped > 0) {
        memcpy(g_state.network.lastCounters, counters, sizeof(NetInterfaceCounter) * clipped);
        qsort(g_state.network.lastCounters, clipped, sizeof(NetInterfaceCounter), CompareNetInterfaceCounterByIndex);
    }
    g_state.network.lastTick = now;
    g_state.network.hasBaseline = TRUE;
    g_state.network.sampleAvailable = TRUE;
}

static void MarkNetworkSampleUnavailable(void) {
    g_state.network.sampleAvailable = FALSE;
    g_state.network.cachedDownBps = 0.0f;
    g_state.network.cachedUpBps = 0.0f;
}

/** Aggregates active interfaces and computes B/s from per-interface deltas. */
static void ApplyNetworkSample(const NetInterfaceCounter* currentCounters,
                               DWORD currentCount,
                               BOOL countersAre64Bit,
                               ULONGLONG now) {
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
            ULONGLONG inDelta = 0;
            ULONGLONG outDelta = 0;
            if (countersAre64Bit) {
                if (!CalculateDelta64(cur->inOctets, prev->inOctets, &inDelta) ||
                    !CalculateDelta64(cur->outOctets, prev->outOctets, &outDelta)) {
                    continue;
                }
            } else {
                inDelta = CalculateDelta32(cur->inOctets, prev->inOctets);
                outDelta = CalculateDelta32(cur->outOctets, prev->outOctets);
            }
            totalInDelta += inDelta;
            totalOutDelta += outDelta;
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

static DWORD WINAPI NetworkRefreshThreadProc(LPVOID param) {
    NetworkRefreshWorkerContext* context = (NetworkRefreshWorkerContext*)param;
    HANDLE refreshEvent = NULL;
    LONG workerGeneration = 0;

    if (!context) {
        return 1;
    }

    refreshEvent = context->refreshEvent;
    workerGeneration = context->generation;
    free(context);

    if (!refreshEvent) {
        return 1;
    }

    for (;;) {
        DWORD waitResult = WaitForSingleObject(refreshEvent, INFINITE);
        if (waitResult != WAIT_OBJECT_0 ||
            IsMonitorInitialized() == 0 ||
            InterlockedCompareExchange(&g_networkRefreshGeneration, 0, 0) != workerGeneration) {
            break;
        }

        NetInterfaceCounter currentCounters[MAX_TRACKED_INTERFACES];
        DWORD currentCount = 0;
        BOOL countersAre64Bit = FALSE;
        BOOL sampleOk = CollectNetworkCounters(currentCounters, &currentCount, &countersAre64Bit);
        ULONGLONG sampleTick = GetMonotonicTickMs();

        AcquireSRWLockExclusive(&g_stateLock);
        if (IsMonitorInitialized() != 0 &&
            InterlockedCompareExchange(&g_networkRefreshGeneration, 0, 0) == workerGeneration) {
            if (sampleOk) {
                ApplyNetworkSample(currentCounters, currentCount, countersAre64Bit, sampleTick);
            } else {
                MarkNetworkSampleUnavailable();
            }
            g_state.network.refreshInProgress = FALSE;
            ReleaseSRWLockExclusive(&g_stateLock);
        } else {
            ReleaseSRWLockExclusive(&g_stateLock);
            break;
        }
    }

    return 0;
}

static void CleanupCompletedNetworkRefreshWorkerLocked(void) {
    if (!g_networkRefreshThread) {
        return;
    }

    if (WaitForSingleObject(g_networkRefreshThread, 0) != WAIT_OBJECT_0) {
        return;
    }

    CloseHandle(g_networkRefreshThread);
    g_networkRefreshThread = NULL;

    if (g_networkRefreshEvent) {
        CloseHandle(g_networkRefreshEvent);
        g_networkRefreshEvent = NULL;
    }
    g_state.network.refreshInProgress = FALSE;
}

static BOOL CleanupRetiredNetworkRefreshWorkerLocked(DWORD waitMs) {
    if (!g_retiredNetworkRefreshThread) {
        if (g_retiredNetworkRefreshEvent) {
            CloseHandle(g_retiredNetworkRefreshEvent);
            g_retiredNetworkRefreshEvent = NULL;
        }
        return TRUE;
    }

    DWORD waitResult = WaitForSingleObject(g_retiredNetworkRefreshThread, waitMs);
    if (waitResult != WAIT_OBJECT_0) {
        return FALSE;
    }

    CloseHandle(g_retiredNetworkRefreshThread);
    g_retiredNetworkRefreshThread = NULL;

    if (g_retiredNetworkRefreshEvent) {
        CloseHandle(g_retiredNetworkRefreshEvent);
        g_retiredNetworkRefreshEvent = NULL;
    }
    if (!g_networkRefreshThread) {
        ReleaseNetworkApiResources();
    }
    return TRUE;
}

static void RefreshCpuCacheIfNeeded(ULONGLONG now) {
    if (!ShouldRefreshTick(now, g_state.cpu.lastUpdateTick)) {
        return;
    }

    float cpuTmp = 0.0f;
    CpuSampleResult cpuStatus = SampleCpuUsage(&cpuTmp);
    if (cpuStatus == CPU_SAMPLE_OK) {
        g_state.cpu.cachedPercent = cpuTmp;
    }

    /* Baseline creation requires an immediate follow-up sample to avoid startup 0%. */
    if (cpuStatus == CPU_SAMPLE_BASELINE_ONLY) {
        g_state.cpu.lastUpdateTick = 0;
    } else {
        g_state.cpu.lastUpdateTick = GetMonotonicTickMs();
    }
}

static void RefreshMemoryCacheIfNeeded(ULONGLONG now) {
    if (!ShouldRefreshTick(now, g_state.memory.lastUpdateTick)) {
        return;
    }

    float memTmp = 0.0f;
    if (SampleMemoryUsage(&memTmp)) {
        g_state.memory.cachedPercent = memTmp;
    }
    g_state.memory.lastUpdateTick = GetMonotonicTickMs();
}

/** Refresh CPU/memory cache only. Network is sampled by a background worker. */
static void RefreshBasicCacheIfNeeded(void) {
    ULONGLONG now = GetMonotonicTickMs();
    RefreshCpuCacheIfNeeded(now);
    RefreshMemoryCacheIfNeeded(now);
}

static BOOL BeginNetworkRefreshIfNeeded(void) {
    ULONGLONG now = GetMonotonicTickMs();
    if (!g_networkRefreshThread || !g_networkRefreshEvent) {
        return FALSE;
    }
    if (g_state.network.refreshInProgress) {
        return FALSE;
    }
    if (g_state.network.lastPollAttemptTick != 0 &&
        (now - g_state.network.lastPollAttemptTick) < g_state.updateIntervalMs) {
        return FALSE;
    }

    g_state.network.lastPollAttemptTick = now;
    g_state.network.refreshInProgress = TRUE;
    return TRUE;
}

static BOOL IsNetworkRefreshStartFailureCoolingDown(ULONGLONG now) {
    return g_networkRefreshLastStartFailureTick != 0 &&
           (DWORD)(now - g_networkRefreshLastStartFailureTick) <
               NETWORK_REFRESH_START_FAILURE_COOLDOWN_MS;
}

static void MarkNetworkRefreshStartFailure(ULONGLONG now) {
    g_networkRefreshLastStartFailureTick = (DWORD)(now ? now : 1);
}

static void StartNetworkRefreshIfNeeded(void) {
    if (!BeginNetworkRefreshIfNeeded()) {
        return;
    }

    if (!g_networkRefreshEvent || !SetEvent(g_networkRefreshEvent)) {
        g_state.network.refreshInProgress = FALSE;
    }
}

static BOOL EnsureNetworkRefreshWorkerStarted(void) {
    CleanupCompletedNetworkRefreshWorkerLocked();
    if (!CleanupRetiredNetworkRefreshWorkerLocked(0)) {
        return FALSE;
    }

    if (g_networkRefreshThread && g_networkRefreshEvent) {
        return TRUE;
    }

    ULONGLONG now = GetMonotonicTickMs();
    if (IsNetworkRefreshStartFailureCoolingDown(now)) {
        return FALSE;
    }

    if (g_networkRefreshEvent && !g_networkRefreshThread) {
        CloseHandle(g_networkRefreshEvent);
        g_networkRefreshEvent = NULL;
    }

    g_networkRefreshEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!g_networkRefreshEvent) {
        MarkNetworkRefreshStartFailure(now);
        return FALSE;
    }

    LONG workerGeneration = InterlockedCompareExchange(&g_networkRefreshGeneration, 0, 0);
    NetworkRefreshWorkerContext* context =
        (NetworkRefreshWorkerContext*)calloc(1, sizeof(NetworkRefreshWorkerContext));
    if (!context) {
        CloseHandle(g_networkRefreshEvent);
        g_networkRefreshEvent = NULL;
        MarkNetworkRefreshStartFailure(now);
        return FALSE;
    }
    context->refreshEvent = g_networkRefreshEvent;
    context->generation = workerGeneration;

    NetworkRefreshWorkerContext* threadContext = context;
    context = NULL;
    g_networkRefreshThread = CreateThread(NULL, 0, NetworkRefreshThreadProc,
                                          threadContext, 0, NULL);
    if (!g_networkRefreshThread) {
        free(threadContext);
        CloseHandle(g_networkRefreshEvent);
        g_networkRefreshEvent = NULL;
        MarkNetworkRefreshStartFailure(now);
        return FALSE;
    }

    g_networkRefreshLastStartFailureTick = 0;
    return TRUE;
}

static inline LONG IsMonitorInitialized(void) {
    return InterlockedCompareExchange(&g_initialized, 0, 0);
}

static BOOL BeginMonitorUse(void) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (IsMonitorInitialized() == 0) {
            SystemMonitor_Init();
        }

        AcquireSRWLockShared(&g_monitorLifecycleLock);
        if (IsMonitorInitialized() != 0) {
            return TRUE;
        }
        ReleaseSRWLockShared(&g_monitorLifecycleLock);
    }

    return FALSE;
}

static void EndMonitorUse(void) {
    ReleaseSRWLockShared(&g_monitorLifecycleLock);
}

void SystemMonitor_Init(void) {
    AcquireSRWLockExclusive(&g_monitorLifecycleLock);
    CleanupRetiredNetworkRefreshWorkerLocked(0);

    if (InterlockedCompareExchange(&g_initialized, 1, 0) != 0) {
        ReleaseSRWLockExclusive(&g_monitorLifecycleLock);
        return;
    }

    InterlockedIncrement(&g_networkRefreshGeneration);
    AcquireSRWLockExclusive(&g_stateLock);
    ZeroMemory(&g_state, sizeof(g_state));
    g_state.updateIntervalMs = DEFAULT_UPDATE_INTERVAL_MS;
    g_networkRefreshLastStartFailureTick = 0;
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
    ReleaseSRWLockExclusive(&g_monitorLifecycleLock);
}

void SystemMonitor_Shutdown(void) {
    HANDLE hRefreshThread = NULL;
    HANDLE hRefreshEvent = NULL;
    BOOL refreshThreadExited = TRUE;

    AcquireSRWLockExclusive(&g_monitorLifecycleLock);
    CleanupRetiredNetworkRefreshWorkerLocked(0);

    AcquireSRWLockExclusive(&g_stateLock);
    InterlockedExchange(&g_initialized, 0);
    InterlockedIncrement(&g_networkRefreshGeneration);
    hRefreshThread = g_networkRefreshThread;
    hRefreshEvent = g_networkRefreshEvent;
    g_networkRefreshThread = NULL;
    g_networkRefreshEvent = NULL;
    ZeroMemory(&g_state, sizeof(g_state));
    ReleaseSRWLockExclusive(&g_stateLock);

    if (hRefreshEvent) {
        SetEvent(hRefreshEvent);
    }
    if (hRefreshThread) {
        DWORD waitResult = WaitForSingleObject(hRefreshThread, NETWORK_REFRESH_SHUTDOWN_WAIT_MS);
        refreshThreadExited = (waitResult == WAIT_OBJECT_0);
        if (!refreshThreadExited) {
            if (waitResult == WAIT_TIMEOUT) {
                OutputDebugStringW(L"SystemMonitor: network refresh worker did not exit before shutdown timeout\n");
            }
            g_retiredNetworkRefreshThread = hRefreshThread;
            g_retiredNetworkRefreshEvent = hRefreshEvent;
            hRefreshThread = NULL;
            hRefreshEvent = NULL;
        }
    }
    if (hRefreshThread) {
        CloseHandle(hRefreshThread);
    }
    if (hRefreshEvent) {
        CloseHandle(hRefreshEvent);
    }

    if (refreshThreadExited && !g_retiredNetworkRefreshThread) {
        ReleaseNetworkApiResources();
    }
    ReleaseSRWLockExclusive(&g_monitorLifecycleLock);
}

void SystemMonitor_SetUpdateIntervalMs(DWORD intervalMs) {
    if (!BeginMonitorUse()) return;
    AcquireSRWLockExclusive(&g_stateLock);
    if (IsMonitorInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_stateLock);
        EndMonitorUse();
        return;
    }
    g_state.updateIntervalMs = (intervalMs == 0) ? DEFAULT_UPDATE_INTERVAL_MS : intervalMs;
    ReleaseSRWLockExclusive(&g_stateLock);
    EndMonitorUse();
}

void SystemMonitor_ForceRefresh(void) {
    if (!BeginMonitorUse()) return;
    AcquireSRWLockExclusive(&g_stateLock);
    if (IsMonitorInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_stateLock);
        EndMonitorUse();
        return;
    }
    g_state.cpu.lastUpdateTick = 0;
    g_state.memory.lastUpdateTick = 0;
    RefreshBasicCacheIfNeeded();
    if (EnsureNetworkRefreshWorkerStarted()) {
        g_state.network.lastPollAttemptTick = 0;
        StartNetworkRefreshIfNeeded();
    }
    ReleaseSRWLockExclusive(&g_stateLock);
    EndMonitorUse();
}

BOOL SystemMonitor_GetCpuUsage(float* outPercent) {
    if (!outPercent) return FALSE;
    *outPercent = 0.0f;
    if (!BeginMonitorUse()) return FALSE;
    AcquireSRWLockExclusive(&g_stateLock);
    if (IsMonitorInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_stateLock);
        EndMonitorUse();
        return FALSE;
    }
    RefreshCpuCacheIfNeeded(GetMonotonicTickMs());
    *outPercent = g_state.cpu.cachedPercent;
    ReleaseSRWLockExclusive(&g_stateLock);
    EndMonitorUse();
    return TRUE;
}

BOOL SystemMonitor_GetMemoryUsage(float* outPercent) {
    if (!outPercent) return FALSE;
    *outPercent = 0.0f;
    if (!BeginMonitorUse()) return FALSE;
    AcquireSRWLockExclusive(&g_stateLock);
    if (IsMonitorInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_stateLock);
        EndMonitorUse();
        return FALSE;
    }
    RefreshMemoryCacheIfNeeded(GetMonotonicTickMs());
    *outPercent = g_state.memory.cachedPercent;
    ReleaseSRWLockExclusive(&g_stateLock);
    EndMonitorUse();
    return TRUE;
}

BOOL SystemMonitor_GetUsage(float* outCpuPercent, float* outMemPercent) {
    if (!outMemPercent) return FALSE;
    if (!outCpuPercent) return FALSE;
    *outCpuPercent = 0.0f;
    *outMemPercent = 0.0f;
    if (!BeginMonitorUse()) return FALSE;
    AcquireSRWLockExclusive(&g_stateLock);
    if (IsMonitorInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_stateLock);
        EndMonitorUse();
        return FALSE;
    }
    RefreshBasicCacheIfNeeded();
    
    *outCpuPercent = g_state.cpu.cachedPercent;
    *outMemPercent = g_state.memory.cachedPercent;
    ReleaseSRWLockExclusive(&g_stateLock);
    EndMonitorUse();
    return TRUE;
}

BOOL SystemMonitor_GetNetSpeed(float* outUpBytesPerSec, float* outDownBytesPerSec) {
    if (!outDownBytesPerSec) return FALSE;
    if (!outUpBytesPerSec) return FALSE;
    *outUpBytesPerSec = 0.0f;
    *outDownBytesPerSec = 0.0f;
    if (!BeginMonitorUse()) return FALSE;

    AcquireSRWLockExclusive(&g_stateLock);
    if (IsMonitorInitialized() == 0) {
        ReleaseSRWLockExclusive(&g_stateLock);
        EndMonitorUse();
        return FALSE;
    }

    if (!EnsureNetworkRefreshWorkerStarted()) {
        ReleaseSRWLockExclusive(&g_stateLock);
        EndMonitorUse();
        return FALSE;
    }

    StartNetworkRefreshIfNeeded();

    if (!g_state.network.sampleAvailable) {
        ReleaseSRWLockExclusive(&g_stateLock);
        EndMonitorUse();
        return FALSE;
    }
    *outUpBytesPerSec = g_state.network.cachedUpBps;
    *outDownBytesPerSec = g_state.network.cachedDownBps;
    ReleaseSRWLockExclusive(&g_stateLock);
    EndMonitorUse();
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
