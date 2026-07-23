/**
 * @file config_watcher_lifecycle.c
 * @brief Configuration watcher startup and bounded shutdown.
 */

#include "config_watcher_internal.h"

static void CleanupCompletedWatcherThread(void) {
    if (!g_watcherThread) return;
    DWORD waitResult = WaitForSingleObject(g_watcherThread, 0);
    if (waitResult != WAIT_OBJECT_0) {
        return;
    }
    CloseHandle(g_watcherThread);
    g_watcherThread = NULL;
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = NULL;
    }
    g_targetHwnd = NULL;
    InterlockedExchange(&g_acceptingChanges, 0);
    InterlockedExchange(&g_configReloadPending, 0);
    InterlockedExchange(&g_configReloadDirty, 0);
}
void ConfigWatcher_Start(HWND hwnd) {
    CleanupCompletedWatcherThread();
    if (g_watcherThread) {
        LOG_WARNING("ConfigWatcher: previous watcher thread is still stopping; start deferred");
        return;
    }
    if (!ConfigWatcher_IsValidTargetWindow(hwnd)) {
        LOG_WARNING("ConfigWatcher: invalid target window");
        return;
    }
    g_targetHwnd = hwnd;
    InterlockedExchange(&g_acceptingChanges, 1);
    InterlockedExchange(&g_configReloadPending, 0);
    InterlockedExchange(&g_configReloadDirty, 0);
    g_stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_stopEvent) {
        LOG_ERROR("ConfigWatcher: Failed to create stop event");
        g_targetHwnd = NULL;
        InterlockedExchange(&g_acceptingChanges, 0);
        return;
    }
    ConfigWatcherThreadContext* context = (ConfigWatcherThreadContext*)calloc(1, sizeof(ConfigWatcherThreadContext));
    if (!context) {
        LOG_ERROR("ConfigWatcher: Failed to allocate watcher context");
        CloseHandle(g_stopEvent);
        g_stopEvent = NULL;
        g_targetHwnd = NULL;
        InterlockedExchange(&g_acceptingChanges, 0);
        InterlockedExchange(&g_configReloadPending, 0);
        InterlockedExchange(&g_configReloadDirty, 0);
        return;
    }
    if (!DuplicateHandle(GetCurrentProcess(), g_stopEvent, GetCurrentProcess(), &context->stopEvent, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        LOG_ERROR("ConfigWatcher: Failed to duplicate stop event");
        free(context);
        CloseHandle(g_stopEvent);
        g_stopEvent = NULL;
        g_targetHwnd = NULL;
        InterlockedExchange(&g_acceptingChanges, 0);
        InterlockedExchange(&g_configReloadPending, 0);
        InterlockedExchange(&g_configReloadDirty, 0);
        return;
    }
    context->targetHwnd = hwnd;
    g_watcherThread = CreateThread(NULL, 0, ConfigWatcher_ThreadProc, context, 0, NULL);
    if (!g_watcherThread) {
        LOG_ERROR("ConfigWatcher: Failed to create watcher thread");
        CloseHandle(context->stopEvent);
        free(context);
        CloseHandle(g_stopEvent);
        g_stopEvent = NULL;
        g_targetHwnd = NULL;
        InterlockedExchange(&g_acceptingChanges, 0);
        InterlockedExchange(&g_configReloadPending, 0);
        InterlockedExchange(&g_configReloadDirty, 0);
    }
}
void ConfigWatcher_Stop(void) {
    CleanupCompletedWatcherThread();
    if (!g_watcherThread) {
        InterlockedExchange(&g_acceptingChanges, 0);
        if (g_stopEvent) {
            CloseHandle(g_stopEvent);
            g_stopEvent = NULL;
        }
        g_targetHwnd = NULL;
        InterlockedExchange(&g_configReloadPending, 0);
        InterlockedExchange(&g_configReloadDirty, 0);
        return;
    }
    InterlockedExchange(&g_acceptingChanges, 0);
    SetEvent(g_stopEvent);
    DWORD waitResult = WaitForSingleObject(g_watcherThread, WATCHER_STOP_TIMEOUT_MS);
    if (waitResult != WAIT_OBJECT_0) {
        LOG_WARNING("ConfigWatcher: stop timed out after %lu ms (wait=%lu, error=%lu)", (DWORD)WATCHER_STOP_TIMEOUT_MS, waitResult, GetLastError());
        g_targetHwnd = NULL;
        InterlockedExchange(&g_configReloadPending, 0);
        InterlockedExchange(&g_configReloadDirty, 0);
        return;
    }
    CloseHandle(g_watcherThread);
    g_watcherThread = NULL;
    CloseHandle(g_stopEvent);
    g_stopEvent = NULL;
    g_targetHwnd = NULL;
    InterlockedExchange(&g_configReloadPending, 0);
    InterlockedExchange(&g_configReloadDirty, 0);
}
BOOL ConfigWatcher_Shutdown(void) {
    CleanupCompletedWatcherThread();
    if (!g_watcherThread) {
        InterlockedExchange(&g_acceptingChanges, 0);
        if (g_stopEvent) {
            CloseHandle(g_stopEvent);
            g_stopEvent = NULL;
        }
        g_targetHwnd = NULL;
        InterlockedExchange(&g_configReloadPending, 0);
        InterlockedExchange(&g_configReloadDirty, 0);
        return TRUE;
    }
    InterlockedExchange(&g_acceptingChanges, 0);
    SetEvent(g_stopEvent);
    DWORD waitResult = WaitForSingleObject(g_watcherThread, WATCHER_FINAL_STOP_TIMEOUT_MS);
    if (waitResult != WAIT_OBJECT_0) {
        LOG_WARNING("ConfigWatcher: final stop timed out after %lu ms (wait=%lu, error=%lu)", (DWORD)WATCHER_FINAL_STOP_TIMEOUT_MS, waitResult, GetLastError());
        g_targetHwnd = NULL;
        InterlockedExchange(&g_configReloadPending, 0);
        InterlockedExchange(&g_configReloadDirty, 0);
        return FALSE;
    }
    CloseHandle(g_watcherThread);
    g_watcherThread = NULL;
    CloseHandle(g_stopEvent);
    g_stopEvent = NULL;
    g_targetHwnd = NULL;
    InterlockedExchange(&g_configReloadPending, 0);
    InterlockedExchange(&g_configReloadDirty, 0);
    return TRUE;
}
