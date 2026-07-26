/**
 * @file system_monitor_basic.c
 * @brief CPU and physical-memory sampling and cache refresh.
 */

#include "system_monitor_internal.h"

typedef enum {
    CPU_SAMPLE_ERROR = 0,
    CPU_SAMPLE_BASELINE_ONLY,
    CPU_SAMPLE_OK
} CpuSampleResult;

static ULONGLONG FileTimeToValue(const FILETIME* time) {
    return ((ULONGLONG)time->dwHighDateTime << 32) | time->dwLowDateTime;
}

static float ClampPercent(double value) {
    if (value < 0.0) return 0.0f;
    if (value > 100.0) return 100.0f;
    return (float)value;
}

static CpuSampleResult SampleCpuUsage(float* outPercent) {
    if (!outPercent) return CPU_SAMPLE_ERROR;
    FILETIME idle;
    FILETIME kernel;
    FILETIME user;
    if (!GetSystemTimes(&idle, &kernel, &user)) return CPU_SAMPLE_ERROR;

    CpuTimesState* state = &g_monitorState.cpu.timesState;
    if (!state->hasBaseline) {
        state->lastIdle = idle;
        state->lastKernel = kernel;
        state->lastUser = user;
        state->hasBaseline = TRUE;
        *outPercent = 0.0f;
        return CPU_SAMPLE_BASELINE_ONLY;
    }

    ULONGLONG idleNow = FileTimeToValue(&idle);
    ULONGLONG kernelNow = FileTimeToValue(&kernel);
    ULONGLONG userNow = FileTimeToValue(&user);
    ULONGLONG idlePrevious = FileTimeToValue(&state->lastIdle);
    ULONGLONG kernelPrevious = FileTimeToValue(&state->lastKernel);
    ULONGLONG userPrevious = FileTimeToValue(&state->lastUser);

    if (idleNow < idlePrevious || kernelNow < kernelPrevious ||
        userNow < userPrevious) {
        state->lastIdle = idle;
        state->lastKernel = kernel;
        state->lastUser = user;
        return CPU_SAMPLE_ERROR;
    }

    ULONGLONG idleDelta = idleNow - idlePrevious;
    ULONGLONG totalDelta = kernelNow - kernelPrevious +
                           userNow - userPrevious;
    if (totalDelta == 0) {
        *outPercent = g_monitorState.cpu.cachedPercent;
        return CPU_SAMPLE_OK;
    }
    if (idleDelta > totalDelta) {
        state->lastIdle = idle;
        state->lastKernel = kernel;
        state->lastUser = user;
        return CPU_SAMPLE_ERROR;
    }

    *outPercent = ClampPercent(
        (double)(totalDelta - idleDelta) * 100.0 / (double)totalDelta);
    state->lastIdle = idle;
    state->lastKernel = kernel;
    state->lastUser = user;
    return CPU_SAMPLE_OK;
}

static BOOL SampleMemoryUsage(float* outPercent) {
    if (!outPercent) return FALSE;
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status) || status.ullTotalPhys == 0) {
        return FALSE;
    }
    ULONGLONG used = status.ullTotalPhys - status.ullAvailPhys;
    *outPercent = ClampPercent(
        (double)used * 100.0 / (double)status.ullTotalPhys);
    return TRUE;
}

void Monitor_RefreshBasicCacheIfNeeded(void) {
    ULONGLONG now = Monitor_GetTickMs();
    if (!Monitor_ShouldRefresh(now, g_monitorState.cpu.lastUpdateTick)) {
        return;
    }

    float cpuPercent = 0.0f;
    CpuSampleResult cpuResult = SampleCpuUsage(&cpuPercent);
    if (cpuResult == CPU_SAMPLE_OK) {
        g_monitorState.cpu.cachedPercent = cpuPercent;
        g_monitorState.cpu.sampleAvailable = TRUE;
    }

    float memoryPercent = 0.0f;
    if (SampleMemoryUsage(&memoryPercent)) {
        g_monitorState.memory.cachedPercent = memoryPercent;
        g_monitorState.memory.sampleAvailable = TRUE;
    }

    ULONGLONG sampleTick = Monitor_GetTickMs();
    g_monitorState.cpu.lastUpdateTick = sampleTick;
    g_monitorState.memory.lastUpdateTick = sampleTick;
    Monitor_AdvanceSnapshotRevision();
}
