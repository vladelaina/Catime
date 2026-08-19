#include "audio_player_internal.h"

#include <assert.h>
#include <stdio.h>

volatile LONG g_audioPlaybackGeneration = 0;
volatile LONG g_audioDesiredVolume = -1;
volatile LONG g_audioDesiredPaused = 0;
SRWLOCK g_audioStateLock = SRWLOCK_INIT;

static HANDLE g_lockHeldEvent = NULL;
static HANDLE g_releaseLockEvent = NULL;
static volatile LONG g_cleanupCount = 0;

void WriteLog(LogLevel level, const char* format, ...) {
    (void)level;
    (void)format;
}

void CleanupAudioResourcesLocked(void) {
    InterlockedIncrement(&g_cleanupCount);
}

static DWORD WINAPI HoldAudioLock(LPVOID parameter) {
    (void)parameter;
    AcquireSRWLockExclusive(&g_audioStateLock);
    SetEvent(g_lockHeldEvent);
    WaitForSingleObject(g_releaseLockEvent, INFINITE);
    ReleaseSRWLockExclusive(&g_audioStateLock);
    return 0;
}

static DWORD WINAPI StopAudio(LPVOID parameter) {
    (void)parameter;
    StopNotificationSound();
    return 0;
}

static void TestBusyAudioLockDoesNotBlockStop(void) {
    g_lockHeldEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_releaseLockEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    assert(g_lockHeldEvent != NULL);
    assert(g_releaseLockEvent != NULL);

    HANDLE holder = CreateThread(NULL, 0, HoldAudioLock, NULL, 0, NULL);
    assert(holder != NULL);
    assert(WaitForSingleObject(g_lockHeldEvent, 2000) == WAIT_OBJECT_0);

    HANDLE stopper = CreateThread(NULL, 0, StopAudio, NULL, 0, NULL);
    assert(stopper != NULL);
    DWORD stopResult = WaitForSingleObject(stopper, 250);

    SetEvent(g_releaseLockEvent);
    assert(WaitForSingleObject(holder, 2000) == WAIT_OBJECT_0);
    assert(WaitForSingleObject(stopper, 2000) == WAIT_OBJECT_0);
    assert(stopResult == WAIT_OBJECT_0);
    assert(InterlockedCompareExchange(&g_audioPlaybackGeneration, 0, 0) == 1);
    ULONGLONG deadline = GetTickCount64() + 2000;
    while (InterlockedCompareExchange(&g_cleanupCount, 0, 0) == 0 &&
           GetTickCount64() < deadline) {
        Sleep(1);
    }
    assert(InterlockedCompareExchange(&g_cleanupCount, 0, 0) == 1);

    CloseHandle(stopper);
    CloseHandle(holder);
    CloseHandle(g_releaseLockEvent);
    CloseHandle(g_lockHeldEvent);
}

static void TestAvailableAudioLockCleansUp(void) {
    StopNotificationSound();
    assert(InterlockedCompareExchange(&g_audioPlaybackGeneration, 0, 0) == 2);
    assert(InterlockedCompareExchange(&g_cleanupCount, 0, 0) == 2);
}

int main(void) {
    TestBusyAudioLockDoesNotBlockStop();
    TestAvailableAudioLockCleansUp();
    puts("audio player cleanup tests passed");
    return 0;
}
