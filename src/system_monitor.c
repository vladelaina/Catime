/**
 * @file system_monitor.c
 * @brief Lightweight system performance monitoring with unified state management
 * 
 * Refactored implementation featuring:
 * - Consolidated state management in a single structure
 * - Eliminated code duplication through helper functions and macros
 * - Improved readability with named constants
 * - Enhanced maintainability through modular design
 * 
 * @version 2.0 - Major refactoring for code quality and maintainability
 */

#include <windows.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <stdio.h>

#include "../include/system_monitor.h"

/* ============================================================================
 * Constants and Configuration
 * ============================================================================ */

/** @brief Default refresh interval in milliseconds */
#define DEFAULT_UPDATE_INTERVAL_MS 1000

/** @brief Network interface type for software loopback (to be excluded) */
#define IF_TYPE_SOFTWARE_LOOPBACK 24

/** @brief Maximum value for 32-bit counter (for overflow handling) */
#define COUNTER_MAX_32BIT 0x100000000ULL

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief CPU sampling state for delta calculation
 */
typedef struct {
    FILETIME lastIdle;      /**< Previous idle time */
    FILETIME lastKernel;    /**< Previous kernel time */
    FILETIME lastUser;      /**< Previous user time */
    BOOL hasBaseline;       /**< Whether baseline sample exists */
} CpuTimesState;

/**
 * @brief Network monitoring state for speed calculation
 */
typedef struct {
    BOOL hasBaseline;           /**< Whether baseline sample exists */
    ULONGLONG lastInOctets;     /**< Previous total bytes received */
    ULONGLONG lastOutOctets;    /**< Previous total bytes sent */
    DWORD lastTick;             /**< Previous sample timestamp */
    float cachedUpBps;          /**< Cached upload speed (bytes/sec) */
    float cachedDownBps;        /**< Cached download speed (bytes/sec) */
} NetworkState;

/**
 * @brief Unified system monitor state
 * 
 * Consolidates all monitoring state into a single structure for improved
 * organization and easier maintenance. Reduces global variable clutter
 * from 13 individual variables to 1 structured variable.
 */
typedef struct {
    /** CPU monitoring state */
    struct {
        CpuTimesState timesState;   /**< CPU time sampling state */
        float cachedPercent;         /**< Cached CPU usage percentage */
    } cpu;
    
    /** Memory monitoring state */
    struct {
        float cachedPercent;         /**< Cached memory usage percentage */
    } memory;
    
    /** Network monitoring state */
    NetworkState network;            /**< Network traffic monitoring state */
    
    /** Refresh control */
    DWORD updateIntervalMs;          /**< Minimum milliseconds between refreshes */
    DWORD lastUpdateTick;            /**< Timestamp of last cache update */
} SystemMonitorState;

/* ============================================================================
 * Global State
 * ============================================================================ */

/** @brief Thread-safe initialization flag (atomic operations) */
static volatile LONG g_initialized = 0;

/** @brief Unified monitoring state structure */
static SystemMonitorState g_state = {0};

/* ============================================================================
 * Helper Functions - Utility
 * ============================================================================ */

/**
 * @brief Convert FILETIME (100-nanosecond units) to 64-bit integer
 * @param ft Pointer to FILETIME structure
 * @return 64-bit integer representation
 */
static inline ULONGLONG FileTimeToUll(const FILETIME* ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft->dwLowDateTime;
    u.HighPart = ft->dwHighDateTime;
    return u.QuadPart;
}

/**
 * @brief Clamp percentage value to valid range [0.0, 100.0]
 * @param value Input value (possibly out of range)
 * @return Clamped value between 0.0 and 100.0
 */
static inline float ClampPercent(double value) {
    if (value < 0.0) return 0.0f;
    if (value > 100.0) return 100.0f;
    return (float)value;
}

/**
 * @brief Calculate delta between two 32-bit counter values with overflow handling
 * 
 * Network interface counters are 32-bit and can overflow. This function
 * correctly handles the overflow case by detecting when current < previous.
 * 
 * @param current Current counter value
 * @param previous Previous counter value
 * @return Delta between samples, accounting for potential overflow
 */
static inline ULONGLONG CalculateDelta64(ULONGLONG current, ULONGLONG previous) {
    return (current >= previous) 
        ? (current - previous)
        : (COUNTER_MAX_32BIT - previous + current);
}

/**
 * @brief Check if cache refresh is needed based on time interval
 * @return TRUE if cache is stale and needs refresh
 */
static inline BOOL ShouldRefresh(void) {
    DWORD now = GetTickCount();
    return (g_state.lastUpdateTick == 0) || 
           ((now - g_state.lastUpdateTick) >= g_state.updateIntervalMs);
}

/* ============================================================================
 * Helper Functions - Sampling
 * ============================================================================ */

/**
 * @brief Sample CPU usage using system time delta calculation
 * 
 * Calculates CPU usage by comparing kernel/user/idle time between two samples.
 * First call establishes baseline and returns FALSE.
 * 
 * @param outPercent Output pointer for CPU percentage (0.0-100.0)
 * @return TRUE if valid delta computed, FALSE if establishing baseline or error
 */
static BOOL SampleCpuUsage(float* outPercent) {
    if (!outPercent) return FALSE;

    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        return FALSE;
    }

    /** First sample: establish baseline */
    if (!g_state.cpu.timesState.hasBaseline) {
        g_state.cpu.timesState.lastIdle = idle;
        g_state.cpu.timesState.lastKernel = kernel;
        g_state.cpu.timesState.lastUser = user;
        g_state.cpu.timesState.hasBaseline = TRUE;
        *outPercent = 0.0f;
        return FALSE;
    }

    /** Convert FILETIME to 64-bit integers for calculation */
    ULONGLONG idleNow = FileTimeToUll(&idle);
    ULONGLONG kernelNow = FileTimeToUll(&kernel);
    ULONGLONG userNow = FileTimeToUll(&user);

    ULONGLONG idlePrev = FileTimeToUll(&g_state.cpu.timesState.lastIdle);
    ULONGLONG kernelPrev = FileTimeToUll(&g_state.cpu.timesState.lastKernel);
    ULONGLONG userPrev = FileTimeToUll(&g_state.cpu.timesState.lastUser);

    /** Calculate time deltas */
    ULONGLONG idleDelta = idleNow - idlePrev;
    ULONGLONG kernelDelta = kernelNow - kernelPrev;
    ULONGLONG userDelta = userNow - userPrev;
    ULONGLONG totalDelta = kernelDelta + userDelta;

    /** Avoid division by zero */
    if (totalDelta == 0) {
        *outPercent = 0.0f;
        return TRUE;
    }

    /** CPU usage = (total - idle) / total * 100 */
    ULONGLONG busyDelta = totalDelta - idleDelta;
    double cpu = (double)busyDelta * 100.0 / (double)totalDelta;
    *outPercent = ClampPercent(cpu);

    /** Update baseline for next sample */
    g_state.cpu.timesState.lastIdle = idle;
    g_state.cpu.timesState.lastKernel = kernel;
    g_state.cpu.timesState.lastUser = user;
    
    return TRUE;
}

/**
 * @brief Sample physical memory usage percentage
 * @param outPercent Output pointer for memory percentage (0.0-100.0)
 * @return TRUE on success, FALSE on error
 */
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

/**
 * @brief Sample network interface counters and calculate speed
 * 
 * Aggregates traffic across all non-loopback interfaces and calculates
 * upload/download speeds in bytes per second. First call establishes
 * baseline; subsequent calls return speed deltas.
 */
static void SampleNetworkSpeed(void) {
    /** Query required buffer size */
    DWORD size = 0;
    DWORD ret = GetIfTable(NULL, &size, TRUE);
    if (ret != ERROR_INSUFFICIENT_BUFFER) {
        return;
    }
    
    /** Allocate buffer and retrieve interface table */
    MIB_IFTABLE* pTable = (MIB_IFTABLE*)malloc(size);
    if (!pTable) return;
    
    ret = GetIfTable(pTable, &size, TRUE);
    if (ret != NO_ERROR) {
        goto cleanup;
    }

    /** Aggregate counters across all non-loopback interfaces */
    ULONGLONG inSum = 0;
    ULONGLONG outSum = 0;
    for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
        const MIB_IFROW* row = &pTable->table[i];
        if (row->dwType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        inSum += (ULONGLONG)row->dwInOctets;
        outSum += (ULONGLONG)row->dwOutOctets;
    }

    DWORD now = GetTickCount();
    
    /** First sample: establish baseline */
    if (!g_state.network.hasBaseline) {
        g_state.network.lastInOctets = inSum;
        g_state.network.lastOutOctets = outSum;
        g_state.network.lastTick = now;
        g_state.network.hasBaseline = TRUE;
        goto cleanup;
    }

    /** Calculate speed from delta */
    DWORD elapsedMs = (now >= g_state.network.lastTick) ? 
                      (now - g_state.network.lastTick) : 0;
    
    if (elapsedMs > 0) {
        ULONGLONG din = CalculateDelta64(inSum, g_state.network.lastInOctets);
        ULONGLONG dout = CalculateDelta64(outSum, g_state.network.lastOutOctets);
        
        double seconds = (double)elapsedMs / 1000.0;
        g_state.network.cachedDownBps = (float)((double)din / seconds);
        g_state.network.cachedUpBps = (float)((double)dout / seconds);
    }

    /** Update baseline for next sample */
    g_state.network.lastInOctets = inSum;
    g_state.network.lastOutOctets = outSum;
    g_state.network.lastTick = now;

cleanup:
    free(pTable);
}

/**
 * @brief Refresh all cached metrics if refresh interval has elapsed
 * 
 * Checks if cache is stale and updates CPU, memory, and network metrics
 * if needed. This centralized refresh logic is called by all getter functions.
 */
static void RefreshCacheIfNeeded(void) {
    if (!ShouldRefresh()) {
        return;
    }

    /** Sample CPU (may return FALSE on first call while establishing baseline) */
    float cpuTmp = 0.0f;
    if (SampleCpuUsage(&cpuTmp)) {
        g_state.cpu.cachedPercent = cpuTmp;
    }

    /** Sample memory */
    float memTmp = 0.0f;
    if (SampleMemoryUsage(&memTmp)) {
        g_state.memory.cachedPercent = memTmp;
    }

    /** Sample network speed */
    SampleNetworkSpeed();

    /** Update timestamp */
    g_state.lastUpdateTick = GetTickCount();
}

/* ============================================================================
 * Parameter Validation Macro
 * ============================================================================ */

/**
 * @brief Validate parameters and ensure module is initialized and refreshed
 * 
 * This macro eliminates code duplication across all getter functions by
 * consolidating:
 * 1. NULL pointer validation
 * 2. Automatic initialization if needed
 * 3. Cache refresh check
 * 
 * Usage: VALIDATE_AND_REFRESH(param) at start of getter functions
 */
#define VALIDATE_AND_REFRESH(param) \
    do { \
        if (!(param)) return FALSE; \
        if (g_initialized == 0) SystemMonitor_Init(); \
        RefreshCacheIfNeeded(); \
    } while(0)

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void SystemMonitor_Init(void) {
    /** Thread-safe initialization: only first call takes effect */
    if (InterlockedCompareExchange(&g_initialized, 1, 0) != 0) {
        return;  /** Already initialized */
    }

    /** Zero-initialize entire state structure */
    ZeroMemory(&g_state, sizeof(g_state));
    
    /** Set default configuration */
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
