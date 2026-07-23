/**
 * @file system_monitor_network_sample.c
 * @brief Per-interface network delta aggregation and worker sampling loop.
 */

#include "system_monitor_internal.h"

static ULONGLONG CalculateDelta32(ULONGLONG current, ULONGLONG previous) {
    return current >= previous ? current - previous
        : MONITOR_COUNTER_MAX_32BIT - previous + current;
}

static BOOL CalculateDelta64(ULONGLONG current, ULONGLONG previous,
                             ULONGLONG* delta) {
    if (!delta || current < previous) return FALSE;
    *delta = current - previous;
    return TRUE;
}

static int CompareCounterIndex(const void* left, const void* right) {
    const NetInterfaceCounter* a = (const NetInterfaceCounter*)left;
    const NetInterfaceCounter* b = (const NetInterfaceCounter*)right;
    return a->index < b->index ? -1 : (a->index > b->index ? 1 : 0);
}

static const NetInterfaceCounter* FindCounter(
    const NetInterfaceCounter* counters, DWORD count, DWORD index) {
    DWORD low = 0;
    DWORD high = count;
    while (low < high) {
        DWORD middle = low + (high - low) / 2;
        if (counters[middle].index == index) return &counters[middle];
        if (counters[middle].index < index) low = middle + 1;
        else high = middle;
    }
    return NULL;
}

static void SetNetworkBaseline(const NetInterfaceCounter* counters,
                               DWORD count, ULONGLONG now) {
    DWORD clipped = count > MONITOR_MAX_TRACKED_INTERFACES
        ? MONITOR_MAX_TRACKED_INTERFACES : count;
    g_monitorState.network.lastCounterCount = clipped;
    if (clipped) {
        memcpy(g_monitorState.network.lastCounters, counters,
               sizeof(*counters) * clipped);
        qsort(g_monitorState.network.lastCounters, clipped,
              sizeof(*counters), CompareCounterIndex);
    }
    g_monitorState.network.lastTick = now;
    g_monitorState.network.hasBaseline = TRUE;
    g_monitorState.network.sampleAvailable = TRUE;
}

void Monitor_MarkNetworkSampleUnavailable(void) {
    g_monitorState.network.sampleAvailable = FALSE;
    g_monitorState.network.cachedDownBps = 0.0f;
    g_monitorState.network.cachedUpBps = 0.0f;
}

void Monitor_ApplyNetworkSample(const NetInterfaceCounter* counters,
                                DWORD count, BOOL is64Bit, ULONGLONG now) {
    if (!g_monitorState.network.hasBaseline) {
        SetNetworkBaseline(counters, count, now);
        g_monitorState.network.cachedDownBps = 0.0f;
        g_monitorState.network.cachedUpBps = 0.0f;
        return;
    }

    ULONGLONG elapsedMs = now >= g_monitorState.network.lastTick
        ? now - g_monitorState.network.lastTick : 0;
    if (elapsedMs) {
        ULONGLONG totalIn = 0;
        ULONGLONG totalOut = 0;
        for (DWORD i = 0; i < count; ++i) {
            const NetInterfaceCounter* previous = FindCounter(
                g_monitorState.network.lastCounters,
                g_monitorState.network.lastCounterCount, counters[i].index);
            if (!previous) continue;

            ULONGLONG inDelta = 0;
            ULONGLONG outDelta = 0;
            if (is64Bit) {
                if (!CalculateDelta64(counters[i].inOctets,
                                      previous->inOctets, &inDelta) ||
                    !CalculateDelta64(counters[i].outOctets,
                                      previous->outOctets, &outDelta)) {
                    continue;
                }
            } else {
                inDelta = CalculateDelta32(counters[i].inOctets,
                                           previous->inOctets);
                outDelta = CalculateDelta32(counters[i].outOctets,
                                            previous->outOctets);
            }
            totalIn += inDelta;
            totalOut += outDelta;
        }

        double seconds = (double)elapsedMs / 1000.0;
        double down = (double)totalIn / seconds;
        double up = (double)totalOut / seconds;
        if (down <= MONITOR_MAX_REASONABLE_RATE_BPS &&
            up <= MONITOR_MAX_REASONABLE_RATE_BPS) {
            g_monitorState.network.cachedDownBps = (float)down;
            g_monitorState.network.cachedUpBps = (float)up;
        } else {
            g_monitorState.network.cachedDownBps = 0.0f;
            g_monitorState.network.cachedUpBps = 0.0f;
        }
    }
    SetNetworkBaseline(counters, count, now);
}

DWORD WINAPI Monitor_NetworkRefreshThreadProc(LPVOID param) {
    NetworkRefreshWorkerContext* context =
        (NetworkRefreshWorkerContext*)param;
    if (!context) return 1;
    HANDLE refreshEvent = context->refreshEvent;
    LONG generation = context->generation;
    free(context);
    if (!refreshEvent) return 1;

    for (;;) {
        DWORD waitResult = WaitForSingleObject(refreshEvent, INFINITE);
        if (waitResult != WAIT_OBJECT_0 || Monitor_IsInitialized() == 0 ||
            InterlockedCompareExchange(
                &g_networkRefreshGeneration, 0, 0) != generation) {
            break;
        }

        NetInterfaceCounter counters[MONITOR_MAX_TRACKED_INTERFACES];
        DWORD count = 0;
        BOOL is64Bit = FALSE;
        BOOL sampled = Monitor_CollectNetworkCounters(
            counters, &count, &is64Bit);
        ULONGLONG sampleTick = Monitor_GetTickMs();

        AcquireSRWLockExclusive(&g_monitorStateLock);
        if (Monitor_IsInitialized() != 0 &&
            InterlockedCompareExchange(
                &g_networkRefreshGeneration, 0, 0) == generation) {
            if (sampled) {
                Monitor_ApplyNetworkSample(counters, count,
                                           is64Bit, sampleTick);
            } else {
                Monitor_MarkNetworkSampleUnavailable();
            }
            g_monitorState.network.refreshInProgress = FALSE;
            ReleaseSRWLockExclusive(&g_monitorStateLock);
        } else {
            ReleaseSRWLockExclusive(&g_monitorStateLock);
            break;
        }
    }
    return 0;
}
