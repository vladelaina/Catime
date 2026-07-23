/**
 * @file config_plugin_security_lock.c
 * @brief Lifetime and lock ordering for the plugin trust state
 */
#include "config/config_plugin_security.h"
#include "config_plugin_security_internal.h"

#define PLUGIN_TRUST_CS_UNINITIALIZED 0
#define PLUGIN_TRUST_CS_INITIALIZING 1
#define PLUGIN_TRUST_CS_INITIALIZED 2
#define PLUGIN_TRUST_WAIT_SPIN_LIMIT 64

static CRITICAL_SECTION g_pluginTrustStateCS;
static CRITICAL_SECTION g_pluginTrustMutationCS;
static volatile LONG g_pluginTrustCSState = PLUGIN_TRUST_CS_UNINITIALIZED;

static void WaitForInitialization(void) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(&g_pluginTrustCSState, 0, 0) ==
           PLUGIN_TRUST_CS_INITIALIZING) {
        Sleep(spins++ < PLUGIN_TRUST_WAIT_SPIN_LIMIT ? 0 : 1);
    }
}

static void EnsureInitialized(void) {
    LONG state = InterlockedCompareExchange(&g_pluginTrustCSState, 0, 0);
    if (state == PLUGIN_TRUST_CS_INITIALIZED) return;
    if (InterlockedCompareExchange(&g_pluginTrustCSState,
                                   PLUGIN_TRUST_CS_INITIALIZING,
                                   PLUGIN_TRUST_CS_UNINITIALIZED) ==
        PLUGIN_TRUST_CS_UNINITIALIZED) {
        InitializeCriticalSection(&g_pluginTrustStateCS);
        InitializeCriticalSection(&g_pluginTrustMutationCS);
        InterlockedExchange(&g_pluginTrustCSState,
                            PLUGIN_TRUST_CS_INITIALIZED);
        return;
    }
    WaitForInitialization();
}

void PluginTrustLock_EnterState(void) {
    EnsureInitialized();
    EnterCriticalSection(&g_pluginTrustStateCS);
}

void PluginTrustLock_LeaveState(void) {
    LeaveCriticalSection(&g_pluginTrustStateCS);
}

void PluginTrustLock_EnterMutation(void) {
    EnsureInitialized();
    EnterCriticalSection(&g_pluginTrustMutationCS);
}

void PluginTrustLock_LeaveMutation(void) {
    LeaveCriticalSection(&g_pluginTrustMutationCS);
}

void CleanupPluginTrustCS(void) {
    WaitForInitialization();
    if (InterlockedCompareExchange(&g_pluginTrustCSState, 0, 0) !=
        PLUGIN_TRUST_CS_INITIALIZED) {
        return;
    }

    EnterCriticalSection(&g_pluginTrustMutationCS);
    EnterCriticalSection(&g_pluginTrustStateCS);
    LeaveCriticalSection(&g_pluginTrustStateCS);
    LeaveCriticalSection(&g_pluginTrustMutationCS);
    DeleteCriticalSection(&g_pluginTrustStateCS);
    DeleteCriticalSection(&g_pluginTrustMutationCS);
    InterlockedExchange(&g_pluginTrustCSState,
                        PLUGIN_TRUST_CS_UNINITIALIZED);
}
