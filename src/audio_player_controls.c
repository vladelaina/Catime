#include "audio_player_internal.h"

BOOL PauseNotificationSound(void) {
    AcquireSRWLockExclusive(&g_audioStateLock);
    if (!(g_isPlaying && !g_isPaused && g_deviceInitialized &&
          g_decoderInitialized)) {
        ReleaseSRWLockExclusive(&g_audioStateLock);
        return FALSE;
    }
    DWORD pauseTick = GetTickCount();
    InterlockedExchange(&g_decoderPaused, 1);
    if (ma_device_stop(&g_device) != MA_SUCCESS) {
        InterlockedExchange(&g_decoderPaused, 0);
        ReleaseSRWLockExclusive(&g_audioStateLock);
        return FALSE;
    }
    g_isPaused = MA_TRUE;
    if (InterlockedCompareExchange(&g_decoderAtEnd, 0, 0)) {
        DWORD deadline = (DWORD)InterlockedCompareExchange(
            &g_decoderDrainDeadline, 0, 0);
        LONG remaining = deadline ? (LONG)(deadline - pauseTick) : 0;
        if (remaining < 100) remaining = 100;
        if (remaining > 2000) remaining = 2000;
        InterlockedExchange(&g_decoderDrainRemainingMs, remaining);
        InterlockedExchange(&g_decoderDrainDeadline, 0);
    }
    if (g_audioTimerKind == AUDIO_TIMER_MINIAUDIO && g_audioTimerId != 0) {
        HWND timerWindow = g_audioTimerHwnd;
        HWND killWindow = timerWindow ? timerWindow : g_audioCallbackHwnd;
        if (IsCurrentProcessAudioWindow(killWindow)) {
            KillTimer(killWindow, g_audioTimerId);
        }
        g_audioTimerId = 0;
        g_audioTimerHwnd = timerWindow;
    }
    ReleaseSRWLockExclusive(&g_audioStateLock);
    return TRUE;
}

BOOL ResumeNotificationSound(void) {
    AcquireSRWLockExclusive(&g_audioStateLock);
    if (!(g_isPlaying && g_isPaused && g_deviceInitialized &&
          g_decoderInitialized)) {
        ReleaseSRWLockExclusive(&g_audioStateLock);
        return FALSE;
    }
    HWND timerWindow = g_audioTimerHwnd ?
        g_audioTimerHwnd : g_audioCallbackHwnd;
    BOOL timerStarted = FALSE;
    BOOL atEnd = InterlockedCompareExchange(
        &g_decoderAtEnd, 0, 0) != 0;
    LONG remaining = 0;
    if (g_audioTimerKind == AUDIO_TIMER_MINIAUDIO && g_audioTimerId == 0) {
        if (!IsCurrentProcessAudioWindow(timerWindow) ||
            !StartPlaybackTimer(
                timerWindow, AUDIO_TIMER_MINIAUDIO,
                TIMER_INTERVAL_AUDIO_CHECK)) {
            ReleaseSRWLockExclusive(&g_audioStateLock);
            return FALSE;
        }
        timerStarted = TRUE;
    }
    if (atEnd) {
        remaining = InterlockedCompareExchange(
            &g_decoderDrainRemainingMs, 0, 0);
        if (remaining < 100) remaining = 100;
        DWORD deadline = GetTickCount() + (DWORD)remaining;
        if (deadline == 0) deadline = 1;
        InterlockedExchange(&g_decoderDrainDeadline, (LONG)deadline);
    }
    InterlockedExchange(&g_decoderPaused, 0);
    if (!ma_device_is_started(&g_device) &&
        ma_device_start(&g_device) != MA_SUCCESS) {
        InterlockedExchange(&g_decoderPaused, 1);
        if (atEnd) {
            InterlockedExchange(&g_decoderDrainDeadline, 0);
            InterlockedExchange(&g_decoderDrainRemainingMs, remaining);
        }
        if (timerStarted) {
            KillTimer(timerWindow, g_audioTimerId);
            g_audioTimerId = 0;
        }
        ReleaseSRWLockExclusive(&g_audioStateLock);
        return FALSE;
    }
    if (atEnd) InterlockedExchange(&g_decoderDrainRemainingMs, 0);
    g_isPaused = MA_FALSE;
    ReleaseSRWLockExclusive(&g_audioStateLock);
    return TRUE;
}
