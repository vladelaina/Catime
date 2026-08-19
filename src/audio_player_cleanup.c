/**
 * @file audio_player_cleanup.c
 * @brief Non-blocking cancellation and cleanup for UI audio requests
 */

#include "audio_player_internal.h"

static DWORD WINAPI AudioCleanupThreadProc(LPVOID parameter) {
    LONG generation = (LONG)(LONG_PTR)parameter;
    AcquireSRWLockExclusive(&g_audioStateLock);
    if (InterlockedCompareExchange(
            &g_audioPlaybackGeneration, 0, 0) == generation) {
        CleanupAudioResourcesLocked();
    }
    ReleaseSRWLockExclusive(&g_audioStateLock);
    return 0;
}

void CleanupAudioResources(void) {
    /* A tray click must remain responsive while a background worker is
     * opening an audio device. The generation change makes that worker clean
     * up before releasing the lock, so waiting here is unnecessary. */
    LONG generation = InterlockedIncrement(&g_audioPlaybackGeneration);
    InterlockedExchange(&g_audioDesiredPaused, 0);
    if (!TryAcquireSRWLockExclusive(&g_audioStateLock)) {
        HANDLE thread = CreateThread(
            NULL, 0, AudioCleanupThreadProc,
            (LPVOID)(LONG_PTR)generation, 0, NULL);
        if (thread) {
            CloseHandle(thread);
        } else {
            LOG_WARNING(
                "Failed to queue deferred audio cleanup (error=%lu)",
                GetLastError());
        }
        return;
    }
    CleanupAudioResourcesLocked();
    ReleaseSRWLockExclusive(&g_audioStateLock);
}

void StopNotificationSound(void) {
    CleanupAudioResources();
}
