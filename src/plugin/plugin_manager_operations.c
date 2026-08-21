/**
 * @file plugin_manager_operations.c
 * @brief Serializes plugin operations away from the window message thread.
 */

#include "plugin_manager_internal.h"

typedef struct {
    HWND hwnd;
    LONG serial;
    int operation;
    int index;
    BOOL trustPlugin;
    LONG generation;
    char expectedPath[MAX_PATH];
    char savedHash[65];
} PluginOperation;

static volatile LONG g_operationPending = 0;
static volatile LONG g_operationShuttingDown = 0;
static volatile LONG g_operationSerial = 0;
static volatile LONG g_latestOperationSerial = 0;
static HANDLE g_operationThread = NULL;
static PluginOperation* g_nextOperation = NULL;
static PluginOperation* g_currentOperation = NULL;
static BOOL g_currentSecurityRequested = FALSE;
static SRWLOCK g_operationLock = SRWLOCK_INIT;

static void CopyLastError(wchar_t* dst, size_t count) {
    const wchar_t* error = PluginProcess_GetLastError();
    if (dst && count > 0) {
        dst[0] = L'\0';
        if (error && error[0]) wcsncpy_s(dst, count, error, _TRUNCATE);
    }
}

static void PostOperationResult(const PluginOperation* op, BOOL success) {
    PluginOperationResult* result = calloc(1, sizeof(*result));
    if (!result) return;
    result->operation = op->operation;
    result->serial = op->serial;
    result->index = op->index;
    result->success = success;
    result->generation = op->generation;
    if (!success) CopyLastError(result->error, _countof(result->error));
    if (!op->hwnd || !PostMessage(op->hwnd, WM_PLUGIN_OPERATION_COMPLETE, 0,
                                   (LPARAM)result)) {
        free(result);
    }
}

static BOOL PostSecurityRequest(const PluginOperation* op,
                                const PluginInfo* plugin,
                                const char* path,
                                const char* hash) {
    PluginSecurityRequest* request = calloc(1, sizeof(*request));
    if (!request) return FALSE;
    request->index = op->index;
    request->serial = op->serial;
    strncpy_s(request->path, sizeof(request->path), path, _TRUNCATE);
    WideToUtf8Fixed(plugin->displayName, request->displayName,
                    (int)sizeof(request->displayName));
    if (hash) strncpy_s(request->hash, sizeof(request->hash), hash, _TRUNCATE);
    if (!op->hwnd || !PostMessage(op->hwnd, WM_PLUGIN_SECURITY_REQUEST, 0,
                                   (LPARAM)request)) {
        free(request);
        return FALSE;
    }
    return TRUE;
}

int PluginManager_PostAsyncSecurityRequest(int index, const char* path,
                                           const char* displayName,
                                           const char* hash) {
    PluginOperation* op = g_currentOperation;
    if (!op) return 0;
    PluginSecurityRequest* request = calloc(1, sizeof(*request));
    if (!request) return -1;
    request->serial = op->serial;
    request->index = index;
    if (path) strncpy_s(request->path, sizeof(request->path), path, _TRUNCATE);
    if (displayName) strncpy_s(request->displayName, sizeof(request->displayName),
                               displayName, _TRUNCATE);
    if (hash) strncpy_s(request->hash, sizeof(request->hash), hash, _TRUNCATE);
    if (!op->hwnd || !PostMessage(op->hwnd, WM_PLUGIN_SECURITY_REQUEST, 0,
                                   (LPARAM)request)) {
        free(request);
        return -1;
    }
    g_currentSecurityRequested = TRUE;
    return 1;
}

static BOOL RunStartOperation(const PluginOperation* op, BOOL* securityRequested) {
    PluginInfo plugin;
    char pathUtf8[MAX_PATH] = {0};
    if (!PluginManager_CopyPlugin(op->index, &plugin) ||
        !WideToUtf8Fixed(plugin.path, pathUtf8, MAX_PATH)) {
        return FALSE;
    }
    if (!IsPluginTrusted(pathUtf8)) {
        char hash[65] = {0};
        if (!CalculatePluginHash(pathUtf8, hash)) {
            PluginProcess_SetLastError(L"Hash error");
            return FALSE;
        }
        if (!PostSecurityRequest(op, &plugin, pathUtf8, hash)) {
            PluginProcess_SetLastError(L"Window unavailable");
            return FALSE;
        }
        if (securityRequested) *securityRequested = TRUE;
        return TRUE;
    }
    return StartTrustedPluginWithExpectedPath(op->index, plugin.path);
}

static BOOL RunOperation(const PluginOperation* op, BOOL* securityRequested) {
    switch (op->operation) {
        case PLUGIN_OPERATION_START:
            return RunStartOperation(op, securityRequested);
        case PLUGIN_OPERATION_START_AFTER_SECURITY: {
            return StartPluginAfterSecurityCheckWithSnapshot(
                op->index, op->trustPlugin, op->expectedPath, op->savedHash);
        }
        case PLUGIN_OPERATION_STOP:
            return PluginManager_StopPlugin(op->index);
        case PLUGIN_OPERATION_STOP_ALL:
            PluginManager_StopAllPlugins();
            return TRUE;
        case PLUGIN_OPERATION_STOP_ALL_PRESERVE_DATA:
            StopAllPluginsPreserveData();
            return TRUE;
        case PLUGIN_OPERATION_HOT_RELOAD:
            return PluginManager_RestartPendingHotReload(op->generation);
        default:
            return FALSE;
    }
}

static DWORD WINAPI PluginOperationThread(LPVOID parameter) {
    PluginOperation* op = parameter;
    for (;;) {
        BOOL securityRequested = FALSE;
        g_currentOperation = op;
        g_currentSecurityRequested = FALSE;
        BOOL success = RunOperation(op, &securityRequested);
        securityRequested = securityRequested || g_currentSecurityRequested;
        g_currentOperation = NULL;
        g_currentSecurityRequested = FALSE;

        AcquireSRWLockExclusive(&g_operationLock);
        PluginOperation* next = g_nextOperation;
        g_nextOperation = NULL;
        if (!next) InterlockedExchange(&g_operationPending, 0);
        ReleaseSRWLockExclusive(&g_operationLock);

        /* A later command owns the UI state, so do not apply stale results. */
        if (!securityRequested && !next) PostOperationResult(op, success);
        free(op);
        op = next;
        if (!op) break;
    }
    return 0;
}

static BOOL QueueOperation(const PluginOperation* source) {
    if (!source || !source->hwnd ||
        InterlockedCompareExchange(&g_operationShuttingDown, 0, 0)) {
        return FALSE;
    }
    PluginOperation* op = calloc(1, sizeof(*op));
    if (!op) return FALSE;
    *op = *source;
    AcquireSRWLockExclusive(&g_operationLock);
    if (InterlockedCompareExchange(&g_operationShuttingDown, 0, 0)) {
        ReleaseSRWLockExclusive(&g_operationLock);
        free(op);
        return FALSE;
    }
    op->serial = InterlockedIncrement(&g_operationSerial);
    InterlockedExchange(&g_latestOperationSerial, op->serial);
    if (InterlockedCompareExchange(&g_operationPending, 1, 1) != 0) {
        free(g_nextOperation);
        g_nextOperation = op;
        ReleaseSRWLockExclusive(&g_operationLock);
        return TRUE;
    }
    if (g_operationThread) {
        CloseHandle(g_operationThread);
        g_operationThread = NULL;
    }
    InterlockedExchange(&g_operationPending, 1);
    g_operationThread = CreateThread(
        NULL, 0, PluginOperationThread, op, 0, NULL);
    ReleaseSRWLockExclusive(&g_operationLock);
    if (!g_operationThread) {
        free(op);
        InterlockedExchange(&g_operationPending, 0);
        return FALSE;
    }
    /* The worker owns and frees op after successful thread creation. */
    // cppcheck-suppress memleak
    return TRUE;
}

BOOL PluginManager_RequestStart(HWND hwnd, int index) {
    PluginOperation op = {0};
    op.hwnd = hwnd; op.operation = PLUGIN_OPERATION_START; op.index = index;
    return QueueOperation(&op);
}

BOOL PluginManager_RequestStartAfterSecurityCheck(HWND hwnd, int index,
                                                  BOOL trustPlugin,
                                                  const char* expectedPath,
                                                  const char* savedHash) {
    PluginOperation op = {0};
    op.hwnd = hwnd; op.operation = PLUGIN_OPERATION_START_AFTER_SECURITY;
    op.index = index; op.trustPlugin = trustPlugin;
    if (expectedPath) strncpy_s(op.expectedPath, sizeof(op.expectedPath), expectedPath, _TRUNCATE);
    if (savedHash) strncpy_s(op.savedHash, sizeof(op.savedHash), savedHash, _TRUNCATE);
    return QueueOperation(&op);
}

BOOL PluginManager_RequestStop(HWND hwnd, int index) {
    PluginOperation op = {0};
    op.hwnd = hwnd; op.operation = PLUGIN_OPERATION_STOP; op.index = index;
    return QueueOperation(&op);
}

BOOL PluginManager_RequestStopAll(HWND hwnd) {
    PluginOperation op = {0};
    op.hwnd = hwnd; op.operation = PLUGIN_OPERATION_STOP_ALL;
    return QueueOperation(&op);
}

BOOL PluginManager_RequestStopAllPreserveData(HWND hwnd) {
    PluginOperation op = {0};
    op.hwnd = hwnd; op.operation = PLUGIN_OPERATION_STOP_ALL_PRESERVE_DATA;
    return QueueOperation(&op);
}

BOOL PluginManager_RequestHotReload(HWND hwnd, LONG requestGeneration) {
    PluginOperation op = {0};
    op.hwnd = hwnd; op.operation = PLUGIN_OPERATION_HOT_RELOAD;
    op.generation = requestGeneration;
    return QueueOperation(&op);
}

BOOL PluginManager_IsOperationCurrent(LONG serial) {
    return serial != 0 &&
        InterlockedCompareExchange(&g_latestOperationSerial, 0, 0) == serial;
}

void PluginManager_InitAsync(void) {
    InterlockedExchange(&g_operationShuttingDown, 0);
    InterlockedExchange(&g_operationSerial, 0);
    InterlockedExchange(&g_latestOperationSerial, 0);
}

void PluginManager_ShutdownAsync(void) {
    InterlockedExchange(&g_operationShuttingDown, 1);
    AcquireSRWLockExclusive(&g_operationLock);
    free(g_nextOperation);
    g_nextOperation = NULL;
    HANDLE thread = g_operationThread;
    ReleaseSRWLockExclusive(&g_operationLock);
    if (thread) WaitForSingleObject(thread, INFINITE);
    AcquireSRWLockExclusive(&g_operationLock);
    if (g_operationThread == thread) {
        CloseHandle(g_operationThread);
        g_operationThread = NULL;
    }
    ReleaseSRWLockExclusive(&g_operationLock);
    InterlockedExchange(&g_operationPending, 0);
}
