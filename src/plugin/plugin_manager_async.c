/**
 * @file plugin_manager_async.c
 * @brief Background plugin scans and retired-thread cleanup.
 */

#include "plugin_manager_internal.h"

BOOL CleanupRetiredAsyncScanThread(DWORD waitMs) {
    HANDLE hThread = NULL;
    HANDLE hThreadToClose = NULL;

    AcquireSRWLockExclusive(&g_asyncScanLock);
    hThread = g_hRetiredAsyncScanThread;
    ReleaseSRWLockExclusive(&g_asyncScanLock);

    if (!hThread) {
        return TRUE;
    }

    DWORD waitResult = WaitForSingleObject(hThread, waitMs);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_FAILED) {
            LOG_WARNING("Retired async plugin scan wait failed: %lu", GetLastError());
        }
        return FALSE;
    }

    AcquireSRWLockExclusive(&g_asyncScanLock);
    if (g_hRetiredAsyncScanThread == hThread) {
        g_hRetiredAsyncScanThread = NULL;
        hThreadToClose = hThread;
    }
    ReleaseSRWLockExclusive(&g_asyncScanLock);

    if (hThreadToClose) {
        CloseHandle(hThreadToClose);
    }

    return TRUE;
}

BOOL HasRetiredAsyncScanThread(void) {
    BOOL hasThread = FALSE;

    AcquireSRWLockShared(&g_asyncScanLock);
    hasThread = (g_hRetiredAsyncScanThread != NULL);
    ReleaseSRWLockShared(&g_asyncScanLock);

    return hasThread;
}

BOOL StopAsyncScanThread(void) {
    HANDLE hThread = NULL;

    AcquireSRWLockExclusive(&g_asyncScanLock);
    InterlockedExchange(&g_asyncScanShuttingDown, 1);

    if (g_hAsyncScanThread) {
        hThread = g_hAsyncScanThread;
        g_hAsyncScanThread = NULL;
    }

    InterlockedExchange(&g_asyncScanPending, 0);
    ReleaseSRWLockExclusive(&g_asyncScanLock);

    if (hThread) {
        DWORD waitResult = WaitForSingleObject(hThread, ASYNC_PLUGIN_SCAN_STOP_TIMEOUT_MS);
        if (waitResult != WAIT_OBJECT_0) {
            LOG_WARNING("Async plugin scan stop timed out after %lu ms (wait=%lu, error=%lu)",
                        (DWORD)ASYNC_PLUGIN_SCAN_STOP_TIMEOUT_MS,
                        waitResult,
                        GetLastError());
            AcquireSRWLockExclusive(&g_asyncScanLock);
            if (!g_hRetiredAsyncScanThread) {
                g_hRetiredAsyncScanThread = hThread;
                hThread = NULL;
            }
            ReleaseSRWLockExclusive(&g_asyncScanLock);
            if (hThread) {
                CloseHandle(hThread);
            }
            return FALSE;
        }
        CloseHandle(hThread);
    }

    return TRUE;
}

void PluginManager_RequestScanAsync(void) {
    PluginDirSnapshot currentSnapshot = {0};
    BOOL hasCurrentSnapshot = FALSE;
    AsyncScanThreadParams* threadParams = NULL;

    if (!CleanupRetiredAsyncScanThread(0)) {
        return;
    }

    AcquireSRWLockExclusive(&g_asyncScanLock);

    if (!g_hRetiredAsyncScanThread &&
        InterlockedCompareExchange(&g_asyncScanShuttingDown, 0, 0) != 0) {
        InterlockedExchange(&g_asyncScanShuttingDown, 0);
    }

    if (InterlockedCompareExchange(&g_asyncScanShuttingDown, 0, 0) != 0) {
        ReleaseSRWLockExclusive(&g_asyncScanLock);
        return;
    }

    /* Avoid multiple concurrent scans */
    if (InterlockedCompareExchange(&g_asyncScanPending, 1, 0) != 0) {
        ReleaseSRWLockExclusive(&g_asyncScanLock);
        return;
    }

    if (g_hAsyncScanThread) {
        DWORD wait = WaitForSingleObject(g_hAsyncScanThread, 0);
        if (wait == WAIT_OBJECT_0) {
            CloseHandle(g_hAsyncScanThread);
            g_hAsyncScanThread = NULL;
        } else {
            InterlockedExchange(&g_asyncScanPending, 0);
            ReleaseSRWLockExclusive(&g_asyncScanLock);
            return;
        }
    }

    ReleaseSRWLockExclusive(&g_asyncScanLock);

    hasCurrentSnapshot = GetPluginDirSnapshot(&currentSnapshot);

    threadParams = (AsyncScanThreadParams*)malloc(sizeof(*threadParams));
    if (threadParams) {
        ZeroMemory(threadParams, sizeof(*threadParams));
        threadParams->snapshot = currentSnapshot;
        threadParams->hasSnapshot = hasCurrentSnapshot;
        threadParams->generation =
            InterlockedCompareExchange(&g_asyncScanGeneration, 0, 0);
    }

    AcquireSRWLockExclusive(&g_asyncScanLock);

    if (InterlockedCompareExchange(&g_asyncScanShuttingDown, 0, 0) != 0) {
        free(threadParams);
        InterlockedExchange(&g_asyncScanPending, 0);
        ReleaseSRWLockExclusive(&g_asyncScanLock);
        return;
    }

    if (hasCurrentSnapshot && g_asyncScanHasLastSnapshot &&
        PluginDirSnapshotsEqual(&currentSnapshot, &g_asyncScanLastSnapshot)) {
        free(threadParams);
        InterlockedExchange(&g_asyncScanPending, 0);
        ReleaseSRWLockExclusive(&g_asyncScanLock);
        return;
    }

    if (IsAsyncScanFailureRecentlyCachedLocked(hasCurrentSnapshot,
                                               &currentSnapshot,
                                               GetTickCount())) {
        free(threadParams);
        InterlockedExchange(&g_asyncScanPending, 0);
        ReleaseSRWLockExclusive(&g_asyncScanLock);
        return;
    }

    if (!threadParams) {
        MarkAsyncScanFailureLocked(hasCurrentSnapshot, &currentSnapshot);
        InterlockedExchange(&g_asyncScanPending, 0);
        ReleaseSRWLockExclusive(&g_asyncScanLock);
        return;
    }

    HANDLE hThread = CreateThread(NULL, 0, AsyncScanThread, threadParams, 0, NULL);
    if (hThread) {
        g_hAsyncScanThread = hThread;
    } else {
        free(threadParams);
        MarkAsyncScanFailureLocked(hasCurrentSnapshot, &currentSnapshot);
        InterlockedExchange(&g_asyncScanPending, 0);
    }

    ReleaseSRWLockExclusive(&g_asyncScanLock);
}
