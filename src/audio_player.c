/**
 * @file audio_player.c
 * @brief Public audio playback requests and resource lifecycle
 */

#include "audio_player_internal.h"

ma_device g_device;
ma_bool32 g_decoderInitialized = MA_FALSE;
ma_bool32 g_deviceInitialized = MA_FALSE;
volatile LONG g_decoderAtEnd = 0;
volatile LONG g_decoderDrainDeadline = 0;
volatile LONG g_decoderDrainRemainingMs = 0;
volatile LONG g_decoderPaused = 0;
ma_bool32 g_isPlaying = MA_FALSE;
ma_bool32 g_isPaused = MA_FALSE;
AudioPlaybackCompleteCallback g_audioCompleteCallback = NULL;
HWND g_audioCallbackHwnd = NULL;
UINT_PTR g_audioTimerId = 0;
AudioTimerKind g_audioTimerKind = AUDIO_TIMER_NONE;
HWND g_audioTimerHwnd = NULL;
volatile LONG g_audioTimerSerial = 0;
volatile LONG g_audioPlaybackGeneration = 0;
volatile LONG g_audioDesiredVolume = -1;
volatile LONG g_audioDesiredPaused = 0;
SRWLOCK g_audioStateLock = SRWLOCK_INIT;
SRWLOCK g_audioCallbackLock = SRWLOCK_INIT;

void SetAudioVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    InterlockedExchange(&g_audioDesiredVolume, (LONG)volume);
    AcquireSRWLockExclusive(&g_audioCallbackLock);
    if (g_deviceInitialized) {
        ma_device_set_master_volume(&g_device, (float)volume / 100.0f);
    }
    ReleaseSRWLockExclusive(&g_audioCallbackLock);
}

void SetAudioPlaybackCompleteCallback(
    HWND hwnd, AudioPlaybackCompleteCallback callback) {
    if (!TryAcquireSRWLockExclusive(&g_audioStateLock)) {
        return;
    }
    if (callback && IsCurrentProcessAudioWindow(hwnd)) {
        g_audioCallbackHwnd = hwnd;
        g_audioCompleteCallback = callback;
    } else {
        g_audioCallbackHwnd = NULL;
        g_audioCompleteCallback = NULL;
    }
    ReleaseSRWLockExclusive(&g_audioStateLock);
}

void CleanupAudioResourcesLocked(void) {
    PlaySoundW(NULL, NULL, SND_PURGE);
    CleanupMiniaudioObjects();
    if (g_audioTimerId != 0 &&
        IsCurrentProcessAudioWindow(g_audioTimerHwnd)) {
        KillTimer(g_audioTimerHwnd, g_audioTimerId);
    }
    ResetPlaybackState();
}

static BOOL PlayNotificationSoundFileInternalLocked(
    HWND hwnd, const char* soundFile, BOOL allowFinalBeepFallback) {
    CleanupAudioResourcesLocked();
    if (!soundFile || soundFile[0] == '\0') return TRUE;
    if (strcmp(soundFile, "SYSTEM_BEEP") == 0) {
        return FallbackToSystemBeep(hwnd);
    }
    if (!IsValidFilePath(soundFile)) {
        LOG_WARNING("Invalid audio file path%s: %s",
                    allowFinalBeepFallback ?
                        " (will fallback to system beep)" : "", soundFile);
        return allowFinalBeepFallback ? FallbackToSystemBeep(hwnd) : FALSE;
    }
    AudioFileInfo fileInfo;
    if (!GetAudioFileInfo(soundFile, &fileInfo)) {
        LOG_WARNING("Cannot find audio file%s: %s",
                    allowFinalBeepFallback ?
                        " (will fallback to system beep)" : "", soundFile);
        return allowFinalBeepFallback ? FallbackToSystemBeep(hwnd) : FALSE;
    }
    if (!IsAudioFileSizeAllowed(soundFile, fileInfo.sizeBytes)) {
        return allowFinalBeepFallback ? FallbackToSystemBeep(hwnd) : FALSE;
    }
    if (PlayAudioWithMiniaudio(hwnd, soundFile, fileInfo.path)) return TRUE;
    LOG_WARNING("All audio playback methods failed%s",
                allowFinalBeepFallback ?
                    ", using system beep as final fallback" : "");
    return allowFinalBeepFallback ? FallbackToSystemBeep(hwnd) : FALSE;
}

static DWORD WINAPI AudioPlaybackThreadProc(LPVOID parameter) {
    AudioPlaybackRequest* request = parameter;
    if (!request) return 0;
    HWND hwnd = request->hwnd;
    LONG generation = request->generation;
    BOOL allowFinalBeepFallback = request->allowFinalBeepFallback;
    char soundFile[MAX_PATH] = {0};
    strncpy(soundFile, request->soundFile, sizeof(soundFile) - 1);
    soundFile[sizeof(soundFile) - 1] = '\0';
    free(request);
    if (InterlockedCompareExchange(
            &g_audioPlaybackGeneration, 0, 0) != generation) return 0;
    AcquireSRWLockExclusive(&g_audioStateLock);
    BOOL ownedPlaybackAttempt = FALSE;
    BOOL playbackResult = FALSE;
    if (InterlockedCompareExchange(
            &g_audioPlaybackGeneration, 0, 0) == generation) {
        ownedPlaybackAttempt = TRUE;
        playbackResult = PlayNotificationSoundFileInternalLocked(
            hwnd, soundFile, allowFinalBeepFallback);
        if (InterlockedCompareExchange(&g_audioDesiredPaused, 0, 0)) {
            PauseNotificationSoundLocked();
        }
    }
    if (ownedPlaybackAttempt && InterlockedCompareExchange(
            &g_audioPlaybackGeneration, 0, 0) != generation) {
        CleanupAudioResourcesLocked();
    }
    ReleaseSRWLockExclusive(&g_audioStateLock);
    if (ownedPlaybackAttempt && !playbackResult && !allowFinalBeepFallback) {
        AudioPlaybackCompleteCallback callback = NULL;
        AcquireSRWLockShared(&g_audioCallbackLock);
        callback = g_audioCompleteCallback;
        ReleaseSRWLockShared(&g_audioCallbackLock);
        if (callback) callback(hwnd);
    }
    return 0;
}

static BOOL QueueAudioPlayback(
    HWND hwnd, const char* soundFile, BOOL allowFinalBeepFallback) {
    if (!soundFile) return FALSE;
    InterlockedExchange(&g_audioDesiredPaused, 0);
    AudioPlaybackRequest* request = calloc(1, sizeof(*request));
    if (!request) {
        LOG_WARNING("Failed to allocate async audio playback request");
        return FALSE;
    }
    request->hwnd = hwnd;
    request->generation = InterlockedIncrement(&g_audioPlaybackGeneration);
    request->allowFinalBeepFallback = allowFinalBeepFallback;
    strncpy(request->soundFile, soundFile,
            sizeof(request->soundFile) - 1);
    request->soundFile[sizeof(request->soundFile) - 1] = '\0';
    HANDLE thread = CreateThread(
        NULL, 0, AudioPlaybackThreadProc, request, 0, NULL);
    if (!thread) {
        LOG_WARNING(
            "Failed to start async audio playback thread (error=%lu)",
            GetLastError());
        free(request);
        return FALSE;
    }
    CloseHandle(thread);
    return TRUE;
}

BOOL PlayNotificationSoundFile(HWND hwnd, const char* soundFile) {
    return QueueAudioPlayback(hwnd, soundFile, TRUE);
}

BOOL PreviewNotificationSoundFile(HWND hwnd, const char* soundFile) {
    return QueueAudioPlayback(hwnd, soundFile, FALSE);
}

BOOL PlayNotificationSound(HWND hwnd) {
    const char* configuredFile =
        g_AppConfig.notification.sound.sound_file;
    if (configuredFile[0] == '\0') return TRUE;
    return QueueAudioPlayback(hwnd, configuredFile, TRUE);
}
