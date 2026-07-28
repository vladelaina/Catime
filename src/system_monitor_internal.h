/**
 * @file system_monitor_internal.h
 * @brief Shared state and helpers for the system monitor implementation.
 */

#ifndef CATIME_SYSTEM_MONITOR_INTERNAL_H
#define CATIME_SYSTEM_MONITOR_INTERNAL_H

#include <winsock2.h>
#include <windows.h>
#include <psapi.h>
#include <ifdef.h>
#include <netioapi.h>
#include <iphlpapi.h>
#include <stdlib.h>
#include <string.h>

#include "system_monitor.h"

#define MONITOR_DEFAULT_UPDATE_INTERVAL_MS 1000u
#define MONITOR_MIN_UPDATE_INTERVAL_MS 500u
#define MONITOR_NETWORK_SHUTDOWN_WAIT_MS 2000u
#define MONITOR_NETWORK_START_COOLDOWN_MS 2000u
#define MONITOR_MAX_TRACKED_INTERFACES 256u
#define MONITOR_MAX_IF_TABLE_BYTES (1024u * 1024u)
#define MONITOR_COUNTER_MAX_32BIT 0x100000000ULL
#define MONITOR_MAX_REASONABLE_RATE_BPS 100000000000.0
#define MONITOR_LOOPBACK_INTERFACE_TYPE 24u

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

typedef struct NetworkRefreshRetiredWorker {
    HANDLE thread;
    HANDLE event;
    struct NetworkRefreshRetiredWorker* next;
} NetworkRefreshRetiredWorker;

typedef struct {
    BOOL hasBaseline;
    ULONGLONG lastTick;
    ULONGLONG lastPollAttemptTick;
    NetInterfaceCounter lastCounters[MONITOR_MAX_TRACKED_INTERFACES];
    DWORD lastCounterCount;
    float cachedUpBps;
    float cachedDownBps;
    BOOL sampleAvailable;
    BOOL refreshInProgress;
} NetworkState;

typedef struct {
    struct {
        CpuTimesState timesState;
        float cachedPercent;
        ULONGLONG lastUpdateTick;
        BOOL sampleAvailable;
    } cpu;
    struct {
        float cachedPercent;
        ULONGLONG lastUpdateTick;
        BOOL sampleAvailable;
    } memory;
    NetworkState network;
    DWORD updateIntervalMs;
    ULONGLONG snapshotRevision;
} SystemMonitorState;

extern volatile LONG g_monitorInitialized;
extern SystemMonitorState g_monitorState;
extern SRWLOCK g_monitorLifecycleLock;
extern SRWLOCK g_monitorStateLock;
extern SRWLOCK g_networkApiLock;
extern HANDLE g_networkRefreshThread;
extern HANDLE g_networkRefreshEvent;
extern NetworkRefreshRetiredWorker* g_retiredNetworkRefreshWorkers;
extern volatile LONG g_networkRefreshGeneration;
extern ULONGLONG g_networkRefreshStartFailureCooldownUntil;

LONG Monitor_IsInitialized(void);
ULONGLONG Monitor_GetTickMs(void);
BOOL Monitor_ShouldRefresh(ULONGLONG now, ULONGLONG lastUpdateTick);
void Monitor_RefreshBasicCacheIfNeeded(void);
void Monitor_AdvanceSnapshotRevision(void);

BOOL Monitor_CollectNetworkCounters(NetInterfaceCounter* counters,
                                    DWORD* count, BOOL* is64Bit);
void Monitor_ReleaseNetworkApiResources(void);
void Monitor_ApplyNetworkSample(const NetInterfaceCounter* counters,
                                DWORD count, BOOL is64Bit, ULONGLONG now);
void Monitor_MarkNetworkSampleUnavailable(void);
DWORD WINAPI Monitor_NetworkRefreshThreadProc(LPVOID param);

void Monitor_CleanupCompletedWorkerLocked(void);
BOOL Monitor_CleanupRetiredWorkerLocked(DWORD waitMs);
BOOL Monitor_RetireNetworkWorkerLocked(HANDLE thread, HANDLE event);
BOOL Monitor_EnsureNetworkWorkerStarted(void);
void Monitor_StartNetworkRefreshIfNeeded(void);

#endif /* CATIME_SYSTEM_MONITOR_INTERNAL_H */
