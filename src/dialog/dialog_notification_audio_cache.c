#include "dialog_notification_audio_internal.h"

BOOL NotificationAudio_CloseCompletedScanThreadLocked(DWORD waitMs) {
    if (!g_hSoundScanThread) {
        return TRUE;
    }

    DWORD wait = WaitForSingleObject(g_hSoundScanThread, waitMs);
    if (wait == WAIT_OBJECT_0) {
        CloseHandle(g_hSoundScanThread);
        g_hSoundScanThread = NULL;
        return TRUE;
    }
    if (wait != WAIT_TIMEOUT) {
        OutputDebugStringW(
            L"NotificationSoundCache: sound scan wait failed\n");
    }
    return FALSE;
}

BOOL NotificationAudio_CloseRetiredScanThreadLocked(DWORD waitMs) {
    if (!g_hRetiredSoundScanThread) {
        return TRUE;
    }

    DWORD wait = WaitForSingleObject(g_hRetiredSoundScanThread, waitMs);
    if (wait == WAIT_OBJECT_0) {
        CloseHandle(g_hRetiredSoundScanThread);
        g_hRetiredSoundScanThread = NULL;
        return TRUE;
    }
    if (wait != WAIT_TIMEOUT) {
        OutputDebugStringW(
            L"NotificationSoundCache: retired sound scan wait failed\n");
    }
    return FALSE;
}

void NotificationAudio_RequestCacheScanAsync(void) {
    AcquireSRWLockExclusive(&g_soundScanThreadLock);
    if (!NotificationAudio_CloseRetiredScanThreadLocked(0)) {
        ReleaseSRWLockExclusive(&g_soundScanThreadLock);
        return;
    }

    if (!g_hRetiredSoundScanThread &&
        InterlockedCompareExchange(&g_soundScanShuttingDown, 0, 0) != 0) {
        InterlockedExchange(&g_soundScanShuttingDown, 0);
    }
    if (NotificationAudio_IsScanShuttingDown() ||
        !NotificationAudio_CloseCompletedScanThreadLocked(0)) {
        ReleaseSRWLockExclusive(&g_soundScanThreadLock);
        return;
    }

    DWORD now = GetTickCount();
    if (NotificationAudio_IsCacheRecentlyScanned(now)) {
        ReleaseSRWLockExclusive(&g_soundScanThreadLock);
        return;
    }

    LONG generation = InterlockedCompareExchange(
        &g_soundScanGeneration, 0, 0);
    HANDLE thread = CreateThread(
        NULL, 0, NotificationAudio_ScanThread,
        (LPVOID)(INT_PTR)generation, 0, NULL);
    if (thread) {
        g_hSoundScanThread = thread;
    } else {
        NotificationAudio_MarkCacheScanFailed();
    }
    ReleaseSRWLockExclusive(&g_soundScanThreadLock);
}

void NotificationSoundCache_Initialize(void) {
    AcquireSRWLockExclusive(&g_soundScanThreadLock);
    if (!NotificationAudio_CloseRetiredScanThreadLocked(
            NOTIFICATION_SOUND_SCAN_STOP_TIMEOUT_MS)) {
        ReleaseSRWLockExclusive(&g_soundScanThreadLock);
        return;
    }
    NotificationAudio_CloseCompletedScanThreadLocked(0);
    ReleaseSRWLockExclusive(&g_soundScanThreadLock);

    InterlockedIncrement(&g_soundScanGeneration);
    InterlockedExchange(&g_soundScanShuttingDown, 0);
    NotificationAudio_StartFolderWatcher();
}

void NotificationSoundCache_RequestScanAsync(void) {
    NotificationAudio_RequestCacheScanAsync();
}

void NotificationSoundCache_SetNotifyWindow(HWND hwnd) {
    AcquireSRWLockExclusive(&g_soundCacheNotifyLock);
    g_soundCacheNotifyHwnd =
        NotificationAudio_IsCurrentProcessWindow(hwnd) ? hwnd : NULL;
    ReleaseSRWLockExclusive(&g_soundCacheNotifyLock);
}

static void WaitForActiveScanThread(void) {
    HANDLE thread = NULL;
    AcquireSRWLockExclusive(&g_soundScanThreadLock);
    thread = g_hSoundScanThread;
    ReleaseSRWLockExclusive(&g_soundScanThreadLock);
    if (!thread) {
        return;
    }

    DWORD wait = WaitForSingleObject(
        thread, NOTIFICATION_SOUND_SCAN_STOP_TIMEOUT_MS);
    if (wait != WAIT_OBJECT_0) {
        OutputDebugStringW(
            L"NotificationSoundCache: sound scan stop timed out\n");
        if (wait == WAIT_TIMEOUT) {
            AcquireSRWLockExclusive(&g_soundScanThreadLock);
            if (g_hSoundScanThread == thread) {
                g_hSoundScanThread = NULL;
                if (NotificationAudio_CloseRetiredScanThreadLocked(0)) {
                    g_hRetiredSoundScanThread = thread;
                } else {
                    CloseHandle(thread);
                }
            }
            ReleaseSRWLockExclusive(&g_soundScanThreadLock);
        }
        return;
    }

    AcquireSRWLockExclusive(&g_soundScanThreadLock);
    if (g_hSoundScanThread == thread) {
        CloseHandle(g_hSoundScanThread);
        g_hSoundScanThread = NULL;
    } else {
        CloseHandle(thread);
    }
    ReleaseSRWLockExclusive(&g_soundScanThreadLock);
}

void NotificationSoundCache_Shutdown(void) {
    NotificationAudio_StopFolderWatcher();
    InterlockedExchange(&g_soundScanShuttingDown, 1);
    InterlockedIncrement(&g_soundScanGeneration);
    WaitForActiveScanThread();

    AcquireSRWLockExclusive(&g_soundFileCacheLock);
    ZeroMemory(g_soundFileCache, sizeof(g_soundFileCache));
    g_soundFileCacheCount = 0;
    g_soundFileCacheReady = FALSE;
    g_soundFileCacheFailed = FALSE;
    ReleaseSRWLockExclusive(&g_soundFileCacheLock);
    InterlockedExchange(&g_soundFileLastScanTick, 0);
}
