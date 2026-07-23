#include "audio_player_internal.h"

void CALLBACK AudioTimerCallback(
    HWND hwnd, UINT message, UINT_PTR eventId, DWORD time) {
    (void)time;
    AudioPlaybackCompleteCallback callback = NULL;
    HWND callbackHwnd = NULL;
    AcquireSRWLockExclusive(&g_audioStateLock);
    if (message != WM_TIMER || eventId != g_audioTimerId ||
        g_audioTimerKind == AUDIO_TIMER_NONE ||
        hwnd != g_audioTimerHwnd || !IsCurrentProcessAudioWindow(hwnd)) {
        ReleaseSRWLockExclusive(&g_audioStateLock);
        return;
    }
    BOOL shouldStop = FALSE;
    AudioTimerKind timerKind = g_audioTimerKind;
    if (timerKind == AUDIO_TIMER_MINIAUDIO) {
        if (g_deviceInitialized && g_decoderInitialized) {
            BOOL atEnd = InterlockedCompareExchange(
                &g_decoderAtEnd, 0, 0) != 0;
            DWORD deadline = (DWORD)InterlockedCompareExchange(
                &g_decoderDrainDeadline, 0, 0);
            BOOL drained = atEnd && deadline != 0 &&
                (LONG)(GetTickCount() - deadline) >= 0;
            if ((drained || !ma_device_is_started(&g_device)) && !g_isPaused) {
                CleanupMiniaudioObjects();
                shouldStop = TRUE;
            }
        } else {
            shouldStop = TRUE;
        }
    } else if (timerKind == AUDIO_TIMER_PLAYSOUND) {
        PlaySoundW(NULL, NULL, SND_PURGE);
        shouldStop = TRUE;
    } else if (timerKind == AUDIO_TIMER_BEEP) {
        shouldStop = TRUE;
    }
    if (shouldStop) {
        HWND timerWindow = g_audioTimerHwnd ? g_audioTimerHwnd : hwnd;
        callbackHwnd = g_audioCallbackHwnd;
        callback = g_audioCompleteCallback;
        BOOL notify = callback && callbackHwnd &&
            callbackHwnd == timerWindow &&
            IsCurrentProcessAudioWindow(callbackHwnd);
        if (IsCurrentProcessAudioWindow(timerWindow)) {
            KillTimer(timerWindow, g_audioTimerId);
        }
        ResetPlaybackState();
        if (!notify) {
            callback = NULL;
            callbackHwnd = NULL;
        }
    }
    ReleaseSRWLockExclusive(&g_audioStateLock);
    if (callback) callback(callbackHwnd);
}
