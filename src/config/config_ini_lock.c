/**
 * @file config_ini_lock.c
 * @brief Process-local cache lock and cross-process write mutex.
 */

#include "config_ini_internal.h"

IniFile* g_ConfigIni = NULL;
CRITICAL_SECTION g_IniCriticalSection;
volatile LONG g_IniCriticalSectionInitialized = INI_CS_UNINITIALIZED;
HANDLE g_ConfigWriteMutex = NULL;

static void WaitWhileIniCSInitializing(void) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(
               &g_IniCriticalSectionInitialized, 0, 0) ==
           INI_CS_INITIALIZING) {
        Sleep(spins++ < INI_WAIT_SPIN_LIMIT ? 0 : 1);
    }
}

static void EnsureCriticalSectionInitialized(void) {
    if (InterlockedCompareExchange(
            &g_IniCriticalSectionInitialized, INI_CS_INITIALIZING,
            INI_CS_UNINITIALIZED) == INI_CS_UNINITIALIZED) {
        InitializeCriticalSection(&g_IniCriticalSection);
        InterlockedExchange(&g_IniCriticalSectionInitialized,
                            INI_CS_INITIALIZED);
    }
    WaitWhileIniCSInitializing();
}

void AcquireIniLock(void) {
    EnsureCriticalSectionInitialized();
    EnterCriticalSection(&g_IniCriticalSection);
}

void ReleaseIniLock(void) {
    LeaveCriticalSection(&g_IniCriticalSection);
}

static HANDLE GetConfigWriteMutex(void) {
    HANDLE mutex = (HANDLE)InterlockedCompareExchangePointer(
        (PVOID volatile*)&g_ConfigWriteMutex, NULL, NULL);
    if (mutex) return mutex;

    HANDLE newMutex = CreateMutexW(NULL, FALSE, L"CatimeConfigWriteMutex");
    if (!newMutex) return NULL;
    HANDLE existing = (HANDLE)InterlockedCompareExchangePointer(
        (PVOID volatile*)&g_ConfigWriteMutex, newMutex, NULL);
    if (existing) {
        CloseHandle(newMutex);
        return existing;
    }
    return newMutex;
}

BOOL AcquireConfigWriteLock(void) {
    HANDLE mutex = GetConfigWriteMutex();
    if (!mutex) {
        LOG_WARNING("Config write mutex unavailable");
        return FALSE;
    }
    DWORD result = WaitForSingleObject(
        mutex, CONFIG_WRITE_LOCK_TIMEOUT_MS);
    if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) return TRUE;
    if (result == WAIT_TIMEOUT) {
        LOG_WARNING("Timed out waiting for config write mutex after %lu ms",
                    (DWORD)CONFIG_WRITE_LOCK_TIMEOUT_MS);
    } else {
        LOG_WARNING("Failed waiting for config write mutex (result=%lu, error=%lu)",
                    result, GetLastError());
    }
    return FALSE;
}

static void ReleaseConfigWriteLock(void) {
    HANDLE mutex = GetConfigWriteMutex();
    if (mutex) ReleaseMutex(mutex);
}

void ReleaseConfigWriteAndIniLocks(void) {
    ReleaseIniLock();
    ReleaseConfigWriteLock();
}

void ShutdownIniCache(void) {
    WaitWhileIniCSInitializing();
    if (InterlockedCompareExchange(
            &g_IniCriticalSectionInitialized, 0, 0) == INI_CS_INITIALIZED) {
        AcquireIniLock();
        FreeIniFile(g_ConfigIni);
        g_ConfigIni = NULL;
        ReleaseIniLock();
        DeleteCriticalSection(&g_IniCriticalSection);
        g_IniCriticalSectionInitialized = INI_CS_UNINITIALIZED;
    } else if (g_ConfigIni) {
        FreeIniFile(g_ConfigIni);
        g_ConfigIni = NULL;
    }

    HANDLE mutex = (HANDLE)InterlockedExchangePointer(
        (PVOID volatile*)&g_ConfigWriteMutex, NULL);
    if (mutex) CloseHandle(mutex);
}
