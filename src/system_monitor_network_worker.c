/**
 * @file system_monitor_network_worker.c
 * @brief Network refresh worker creation, signaling, and retirement.
 */

#include "system_monitor_internal.h"

void Monitor_CleanupCompletedWorkerLocked(void) {
    if (!g_networkRefreshThread ||
        WaitForSingleObject(g_networkRefreshThread, 0) != WAIT_OBJECT_0) {
        return;
    }
    CloseHandle(g_networkRefreshThread);
    g_networkRefreshThread = NULL;
    if (g_networkRefreshEvent) {
        CloseHandle(g_networkRefreshEvent);
        g_networkRefreshEvent = NULL;
    }
    g_monitorState.network.refreshInProgress = FALSE;
}

BOOL Monitor_CleanupRetiredWorkerLocked(DWORD waitMs) {
    BOOL hadRetiredWorkers = g_retiredNetworkRefreshWorkers != NULL;
    BOOL allCompleted = TRUE;
    ULONGLONG deadline = 0;
    if (waitMs != INFINITE) deadline = Monitor_GetTickMs() + waitMs;

    NetworkRefreshRetiredWorker** cursor =
        &g_retiredNetworkRefreshWorkers;
    while (*cursor) {
        NetworkRefreshRetiredWorker* worker = *cursor;
        DWORD remaining = waitMs;
        if (waitMs != INFINITE) {
            ULONGLONG now = Monitor_GetTickMs();
            if (now >= deadline) {
                remaining = 0;
            } else {
                ULONGLONG delta = deadline - now;
                remaining = delta > MAXDWORD ? MAXDWORD : (DWORD)delta;
            }
        }

        DWORD result = WaitForSingleObject(worker->thread, remaining);
        if (result == WAIT_OBJECT_0) {
            *cursor = worker->next;
            CloseHandle(worker->thread);
            if (worker->event) CloseHandle(worker->event);
            free(worker);
            continue;
        }

        if (result == WAIT_FAILED) {
            OutputDebugStringW(
                L"SystemMonitor: retired network worker wait failed\n");
        }
        allCompleted = FALSE;
        cursor = &worker->next;
    }

    if (hadRetiredWorkers && allCompleted && !g_networkRefreshThread) {
        Monitor_ReleaseNetworkApiResources();
    }
    return allCompleted;
}

BOOL Monitor_RetireNetworkWorkerLocked(HANDLE thread, HANDLE event) {
    if (!thread) {
        if (event) CloseHandle(event);
        return TRUE;
    }

    NetworkRefreshRetiredWorker* worker =
        (NetworkRefreshRetiredWorker*)calloc(1, sizeof(*worker));
    if (!worker) {
        /* Keep the handles valid until the worker has definitely exited. */
        OutputDebugStringW(
            L"SystemMonitor: unable to retain network worker handles; waiting for exit\n");
        if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) {
            OutputDebugStringW(
                L"SystemMonitor: network worker exit wait failed; preserving handles\n");
            return FALSE;
        }
        CloseHandle(thread);
        if (event) CloseHandle(event);
        return TRUE;
    }
    worker->thread = thread;
    worker->event = event;
    worker->next = g_retiredNetworkRefreshWorkers;
    g_retiredNetworkRefreshWorkers = worker;
    return TRUE;
}

static BOOL BeginNetworkRefreshIfNeeded(void) {
    ULONGLONG now = Monitor_GetTickMs();
    if (!g_networkRefreshThread || !g_networkRefreshEvent ||
        g_monitorState.network.refreshInProgress) {
        return FALSE;
    }
    if (g_monitorState.network.lastPollAttemptTick &&
        now - g_monitorState.network.lastPollAttemptTick <
            g_monitorState.updateIntervalMs) {
        return FALSE;
    }
    g_monitorState.network.lastPollAttemptTick = now;
    g_monitorState.network.refreshInProgress = TRUE;
    return TRUE;
}

void Monitor_StartNetworkRefreshIfNeeded(void) {
    if (!BeginNetworkRefreshIfNeeded()) return;
    if (!g_networkRefreshEvent || !SetEvent(g_networkRefreshEvent)) {
        g_monitorState.network.refreshInProgress = FALSE;
    }
}

static BOOL IsStartFailureCoolingDown(ULONGLONG now) {
    return g_networkRefreshStartFailureCooldownUntil != 0 &&
           now < g_networkRefreshStartFailureCooldownUntil;
}

static void MarkStartFailure(ULONGLONG now) {
    g_networkRefreshStartFailureCooldownUntil =
        now + MONITOR_NETWORK_START_COOLDOWN_MS;
}

BOOL Monitor_EnsureNetworkWorkerStarted(void) {
    Monitor_CleanupCompletedWorkerLocked();
    if (!Monitor_CleanupRetiredWorkerLocked(0)) return FALSE;
    if (g_networkRefreshThread && g_networkRefreshEvent) return TRUE;

    ULONGLONG now = Monitor_GetTickMs();
    if (IsStartFailureCoolingDown(now)) return FALSE;
    if (g_networkRefreshEvent && !g_networkRefreshThread) {
        CloseHandle(g_networkRefreshEvent);
        g_networkRefreshEvent = NULL;
    }

    g_networkRefreshEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!g_networkRefreshEvent) {
        MarkStartFailure(now);
        return FALSE;
    }

    NetworkRefreshWorkerContext* context =
        (NetworkRefreshWorkerContext*)calloc(1, sizeof(*context));
    if (!context) {
        CloseHandle(g_networkRefreshEvent);
        g_networkRefreshEvent = NULL;
        MarkStartFailure(now);
        return FALSE;
    }
    context->refreshEvent = g_networkRefreshEvent;
    context->generation = InterlockedCompareExchange(
        &g_networkRefreshGeneration, 0, 0);

    g_networkRefreshThread = CreateThread(
        NULL, 0, Monitor_NetworkRefreshThreadProc, context, 0, NULL);
    if (!g_networkRefreshThread) {
        free(context);
        CloseHandle(g_networkRefreshEvent);
        g_networkRefreshEvent = NULL;
        MarkStartFailure(now);
        return FALSE;
    }
    g_networkRefreshStartFailureCooldownUntil = 0;
    return TRUE;
}
