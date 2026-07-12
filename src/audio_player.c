/**
 * @file audio_player.c
 * @brief Three-tier audio fallback (miniaudio → PlaySound → beep)
 * 
 * Why three tiers: miniaudio handles MP3/WAV, PlaySound covers WAV fallback, beep never fails.
 * Path handling: UTF-8 → native Windows wide paths.
 * Completion: miniaudio polls 500ms, PlaySound 3s timeout/purge (no API), beep 500ms fixed.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include "../libs/miniaudio/miniaudio.h"
#include "config.h"
#include "log.h"

#if defined(MA_NO_DECODING) || defined(MA_NO_MP3) || defined(MA_NO_WAV)
#error "Catime requires MP3 and WAV decoding support"
#endif

/* Audio polling intervals:
 * - 500ms for miniaudio: Frequent checks detect completion quickly while minimizing overhead
 * - 3000ms for PlaySound: API lacks completion callback, timeout based on typical notification length
 * - 500ms for beep: Fixed duration matching typical system beep length
 */
#define TIMER_INTERVAL_AUDIO_CHECK 500
#define TIMER_INTERVAL_FALLBACK    3000
#define TIMER_INTERVAL_BEEP        500
#define AUDIO_TIMER_ID_BASE        ((UINT_PTR)0xA7000000u)
#define AUDIO_TIMER_ID_MASK        0xFFFFu
#define MAX_NOTIFICATION_AUDIO_BYTES (64ull * 1024ull * 1024ull)
typedef void (*AudioPlaybackCompleteCallback)(HWND hwnd);

typedef enum {
    AUDIO_TIMER_NONE = 0,
    AUDIO_TIMER_MINIAUDIO,
    AUDIO_TIMER_PLAYSOUND,
    AUDIO_TIMER_BEEP
} AudioTimerKind;

typedef struct {
    wchar_t path[MAX_PATH * 2];
    ULONGLONG sizeBytes;
} AudioFileInfo;

typedef struct {
    HWND hwnd;
    char soundFile[MAX_PATH];
    LONG generation;
} AudioPlaybackRequest;

typedef enum {
    AUDIO_DECODER_NONE = 0,
    AUDIO_DECODER_WAV,
    AUDIO_DECODER_MP3
} AudioDecoderKind;

typedef union {
    ma_wav wav;
    ma_mp3 mp3;
} AudioDecoder;

static AudioDecoder g_decoder;
static AudioDecoderKind g_decoderKind = AUDIO_DECODER_NONE;
static ma_device g_device;
static ma_bool32 g_decoderInitialized = MA_FALSE;
static ma_bool32 g_deviceInitialized = MA_FALSE;
static volatile LONG g_decoderAtEnd = 0;
static volatile LONG g_decoderDrainDeadline = 0;
static volatile LONG g_decoderDrainRemainingMs = 0;
static volatile LONG g_decoderPaused = 0;

static ma_bool32 g_isPlaying = MA_FALSE;
static ma_bool32 g_isPaused = MA_FALSE;
static AudioPlaybackCompleteCallback g_audioCompleteCallback = NULL;
static HWND g_audioCallbackHwnd = NULL;
static UINT_PTR g_audioTimerId = 0;
static AudioTimerKind g_audioTimerKind = AUDIO_TIMER_NONE;
static HWND g_audioTimerHwnd = NULL;
static volatile LONG g_audioTimerSerial = 0;
static volatile LONG g_audioPlaybackGeneration = 0;
static SRWLOCK g_audioStateLock = SRWLOCK_INIT;

static void CALLBACK AudioTimerCallback(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime);
static DWORD WINAPI AudioPlaybackThreadProc(LPVOID lpParam);
static BOOL FallbackToPlaySound(HWND hwnd, const wchar_t* wFilePath);
static BOOL FallbackToSystemBeep(HWND hwnd);
static void ResetPlaybackState(void);
static BOOL StartPlaybackTimer(HWND hwnd, AudioTimerKind timerKind, UINT interval);
static void CleanupMiniaudioObjects(void);
static void CleanupMiniaudioAttempt(void);
static void CleanupAudioResourcesLocked(void);
static BOOL PlayNotificationSoundFileInternalLocked(HWND hwnd, const char* soundFile,
                                                    BOOL allowFinalBeepFallback);
static BOOL GetWideCharPath(const char* utf8Path, wchar_t* wPath, size_t wPathSize);

static BOOL IsCurrentProcessAudioWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    return processId == GetCurrentProcessId();
}

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/** Resolve once, then reuse the wide path and file metadata across fallback tiers. */
static BOOL GetAudioFileInfo(const char* filePath, AudioFileInfo* info) {
    if (!filePath || filePath[0] == '\0' || !info) return FALSE;

    info->path[0] = L'\0';
    info->sizeBytes = 0;

    if (!GetWideCharPath(filePath, info->path, MAX_PATH * 2)) {
        return FALSE;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(info->path, GetFileExInfoStandard, &attrs) ||
        (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return FALSE;
    }

    info->sizeBytes = ((ULONGLONG)attrs.nFileSizeHigh << 32) | attrs.nFileSizeLow;
    return TRUE;
}

static BOOL IsAudioFileSizeAllowed(const char* filePath, ULONGLONG fileSize) {
    if (fileSize > MAX_NOTIFICATION_AUDIO_BYTES) {
        LOG_WARNING("Audio file too large: %s (%llu bytes, limit %llu bytes)",
                    filePath, fileSize, (ULONGLONG)MAX_NOTIFICATION_AUDIO_BYTES);
        return FALSE;
    }

    return TRUE;
}

/** Basic validation (prevents misconfig crashes, not full security) */
static BOOL IsValidFilePath(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    if (strchr(filePath, '=') != NULL) return FALSE;
    if (strlen(filePath) >= MAX_PATH) return FALSE;
    return TRUE;
}

static void ResetPlaybackState(void) {
    g_isPlaying = MA_FALSE;
    g_isPaused = MA_FALSE;
    g_audioTimerId = 0;
    g_audioTimerKind = AUDIO_TIMER_NONE;
    g_audioTimerHwnd = NULL;
    InterlockedExchange(&g_decoderAtEnd, 0);
    InterlockedExchange(&g_decoderDrainDeadline, 0);
    InterlockedExchange(&g_decoderDrainRemainingMs, 0);
    InterlockedExchange(&g_decoderPaused, 0);
}

static UINT_PTR NextAudioTimerId(void) {
    LONG serial = InterlockedIncrement(&g_audioTimerSerial);
    return AUDIO_TIMER_ID_BASE + ((UINT_PTR)serial & AUDIO_TIMER_ID_MASK);
}

/** One timer at a time (kills previous) */
static BOOL StartPlaybackTimer(HWND hwnd, AudioTimerKind timerKind, UINT interval) {
    if (!IsCurrentProcessAudioWindow(hwnd)) {
        LOG_WARNING("Cannot start audio completion timer without a window handle");
        return FALSE;
    }

    if (g_audioTimerId != 0) {
        HWND timerHwnd = g_audioTimerHwnd ? g_audioTimerHwnd : hwnd;
        if (IsCurrentProcessAudioWindow(timerHwnd)) {
            KillTimer(timerHwnd, g_audioTimerId);
        }
        g_audioTimerId = 0;
        g_audioTimerKind = AUDIO_TIMER_NONE;
        g_audioTimerHwnd = NULL;
    }

    UINT_PTR timerId = NextAudioTimerId();
    if (SetTimer(hwnd, timerId, interval, (TIMERPROC)AudioTimerCallback) == 0) {
        LOG_WARNING("Failed to start audio completion timer (kind: %d, error: %lu)",
                    (int)timerKind, GetLastError());
        return FALSE;
    }

    g_audioTimerId = timerId;
    g_audioTimerKind = timerKind;
    g_audioTimerHwnd = hwnd;
    return TRUE;
}

/* ============================================================================
 * Fallback mechanisms
 * ============================================================================ */

/** Tier 2: PlaySound (WAV only) */
static BOOL FallbackToPlaySound(HWND hwnd, const wchar_t* wFilePath) {
    if (!PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
        return FALSE;
    }

    if (!StartPlaybackTimer(hwnd, AUDIO_TIMER_PLAYSOUND, TIMER_INTERVAL_FALLBACK)) {
        PlaySoundW(NULL, NULL, SND_PURGE);
        ResetPlaybackState();
        return FALSE;
    }

    g_isPlaying = MA_TRUE;
    return TRUE;
}

/** Tier 3: System beep (never fails) */
static BOOL FallbackToSystemBeep(HWND hwnd) {
    MessageBeep(MB_OK);

    if (!StartPlaybackTimer(hwnd, AUDIO_TIMER_BEEP, TIMER_INTERVAL_BEEP)) {
        ResetPlaybackState();
        return FALSE;
    }

    g_isPlaying = MA_TRUE;
    return TRUE;
}

static void CleanupMiniaudioObjects(void) {
    if (g_deviceInitialized) {
        ma_device_stop(&g_device);
        ma_device_uninit(&g_device);
        g_deviceInitialized = MA_FALSE;
    }
    if (g_decoderInitialized) {
        switch (g_decoderKind) {
            case AUDIO_DECODER_WAV:
                ma_wav_uninit(&g_decoder.wav, NULL);
                break;
            case AUDIO_DECODER_MP3:
                ma_mp3_uninit(&g_decoder.mp3, NULL);
                break;
            default:
                break;
        }
        g_decoderKind = AUDIO_DECODER_NONE;
        g_decoderInitialized = MA_FALSE;
    }
    InterlockedExchange(&g_decoderAtEnd, 0);
    InterlockedExchange(&g_decoderDrainDeadline, 0);
    InterlockedExchange(&g_decoderDrainRemainingMs, 0);
    InterlockedExchange(&g_decoderPaused, 0);
}

static void CleanupMiniaudioAttempt(void) {
    CleanupMiniaudioObjects();
    ResetPlaybackState();
}


/* ============================================================================
 * Path conversion utilities
 * ============================================================================ */

/** Convert the UTF-8 configuration path to the native Windows path used by miniaudio. */
static BOOL GetWideCharPath(const char* utf8Path, wchar_t* wPath, size_t wPathSize) {
    if (!wPath || wPathSize == 0) return FALSE;
    wPath[0] = L'\0';
    if (!utf8Path || wPathSize > INT_MAX) return FALSE;

    if (MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, (int)wPathSize) <= 0) {
        wPath[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

/* ============================================================================
 * Audio playback core
 * ============================================================================ */

static ma_result InitTypedDecoderFileWide(const wchar_t* path) {
    ma_decoding_backend_config config =
        ma_decoding_backend_config_init(ma_format_unknown, 0);
    ma_result result;

    ZeroMemory(&g_decoder, sizeof(g_decoder));
    result = ma_wav_init_file_w(path, &config, NULL, &g_decoder.wav);
    if (result == MA_SUCCESS) {
        g_decoderKind = AUDIO_DECODER_WAV;
        return MA_SUCCESS;
    }

    ZeroMemory(&g_decoder, sizeof(g_decoder));
    result = ma_mp3_init_file_w(path, &config, NULL, &g_decoder.mp3);
    if (result == MA_SUCCESS) {
        g_decoderKind = AUDIO_DECODER_MP3;
        return MA_SUCCESS;
    }

    g_decoderKind = AUDIO_DECODER_NONE;
    ZeroMemory(&g_decoder, sizeof(g_decoder));
    return result;
}

static ma_result ReadTypedDecoderFrames(void* output, ma_uint64 frameCount,
                                        ma_uint64* framesRead) {
    switch (g_decoderKind) {
        case AUDIO_DECODER_WAV:
            return ma_wav_read_pcm_frames(&g_decoder.wav, output, frameCount,
                                          framesRead);
        case AUDIO_DECODER_MP3:
            return ma_mp3_read_pcm_frames(&g_decoder.mp3, output, frameCount,
                                          framesRead);
        default:
            if (framesRead) *framesRead = 0;
            return MA_INVALID_OPERATION;
    }
}

static ma_result GetTypedDecoderDataFormat(ma_format* format,
                                           ma_uint32* channels,
                                           ma_uint32* sampleRate) {
    switch (g_decoderKind) {
        case AUDIO_DECODER_WAV:
            return ma_wav_get_data_format(&g_decoder.wav, format, channels,
                                          sampleRate, NULL, 0);
        case AUDIO_DECODER_MP3:
            return ma_mp3_get_data_format(&g_decoder.mp3, format, channels,
                                          sampleRate, NULL, 0);
        default:
            return MA_INVALID_OPERATION;
    }
}

static ma_result LoadAudioFileWide(const wchar_t* wFilePath) {
    ma_result result = InitTypedDecoderFileWide(wFilePath);
    if (result == MA_SUCCESS) {
        g_decoderInitialized = MA_TRUE;
    }
    return result;
}

static DWORD CalculateAudioDrainDelayMs(const ma_device* device,
                                        ma_uint32 callbackFrameCount) {
    if (!device) {
        return 500;
    }

    ma_uint64 internalFrames =
        (ma_uint64)device->playback.internalPeriodSizeInFrames *
        device->playback.internalPeriods;
    if (internalFrames > INT_MAX) {
        internalFrames = INT_MAX;
    }

    DWORD delayMs = 50;
    if (device->playback.internalSampleRate > 0) {
        delayMs += (DWORD)MulDiv((int)internalFrames, 1000,
                                (int)device->playback.internalSampleRate);
    }
    if (device->sampleRate > 0 && callbackFrameCount <= INT_MAX) {
        delayMs += (DWORD)MulDiv((int)callbackFrameCount, 1000,
                                (int)device->sampleRate);
    }

    if (delayMs < 100) delayMs = 100;
    if (delayMs > 2000) delayMs = 2000;
    return delayMs;
}

static void AudioDataCallback(ma_device* device, void* output, const void* input,
                              ma_uint32 frameCount) {
    (void)input;

    if (InterlockedCompareExchange(&g_decoderPaused, 0, 0)) {
        ma_silence_pcm_frames(output, frameCount,
                              device->playback.format,
                              device->playback.channels);
        return;
    }

    ma_uint64 framesRead = 0;
    ma_result result = ReadTypedDecoderFrames(output, frameCount, &framesRead);
    if (result != MA_SUCCESS || framesRead < frameCount) {
        if (framesRead < frameCount) {
            ma_silence_pcm_frames(
                ma_offset_pcm_frames_ptr(output, framesRead,
                                         device->playback.format,
                                         device->playback.channels),
                frameCount - framesRead,
                device->playback.format,
                device->playback.channels);
        }
        DWORD deadline = GetTickCount() + CalculateAudioDrainDelayMs(device, frameCount);
        if (deadline == 0) deadline = 1;
        InterlockedCompareExchange(&g_decoderDrainDeadline, (LONG)deadline, 0);
        InterlockedExchange(&g_decoderAtEnd, 1);
    }
}

static ma_result StartAudioPlayback(void) {
    if (!g_decoderInitialized) {
        return MA_ERROR;
    }

    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_result result = GetTypedDecoderDataFormat(&format, &channels, &sampleRate);
    if (result != MA_SUCCESS) {
        return result;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = format;
    config.playback.channels = channels;
    config.sampleRate = sampleRate;
    config.dataCallback = AudioDataCallback;
    config.pUserData = NULL;

    result = ma_device_init(NULL, &config, &g_device);
    if (result != MA_SUCCESS) {
        return result;
    }
    g_deviceInitialized = MA_TRUE;

    float volume = (float)g_AppConfig.notification.sound.volume / 100.0f;
    ma_device_set_master_volume(&g_device, volume);
    InterlockedExchange(&g_decoderAtEnd, 0);
    InterlockedExchange(&g_decoderDrainDeadline, 0);
    InterlockedExchange(&g_decoderDrainRemainingMs, 0);
    InterlockedExchange(&g_decoderPaused, 0);

    result = ma_device_start(&g_device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&g_device);
        g_deviceInitialized = MA_FALSE;
    }
    return result;
}

/** Tier 1: miniaudio with automatic fallback */
static BOOL PlayAudioWithMiniaudio(HWND hwnd, const char* filePath, const wchar_t* wFilePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    if (!wFilePath || wFilePath[0] == L'\0') return FALSE;
    
    ma_result result = LoadAudioFileWide(wFilePath);
    if (result != MA_SUCCESS) {
        LOG_WARNING("miniaudio failed to load audio file (error: %d), falling back to PlaySound", result);
        CleanupMiniaudioAttempt();
        return FallbackToPlaySound(hwnd, wFilePath);
    }
    
    if (StartAudioPlayback() != MA_SUCCESS) {
        LOG_WARNING("miniaudio playback start failed, falling back to PlaySound");
        CleanupMiniaudioAttempt();
        return FallbackToPlaySound(hwnd, wFilePath);
    }
    
    if (!StartPlaybackTimer(hwnd, AUDIO_TIMER_MINIAUDIO, TIMER_INTERVAL_AUDIO_CHECK)) {
        CleanupMiniaudioAttempt();
        return FallbackToPlaySound(hwnd, wFilePath);
    }

    g_isPlaying = MA_TRUE;
    return TRUE;
}

/* ============================================================================
 * Timer callbacks
 * ============================================================================ */

/** Unified completion detector (polls miniaudio, timeouts for others) */
static void CALLBACK AudioTimerCallback(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime) {
    (void)dwTime;

    AudioPlaybackCompleteCallback callback = NULL;
    HWND callbackHwnd = NULL;

    AcquireSRWLockExclusive(&g_audioStateLock);

    if (message != WM_TIMER ||
        idEvent != g_audioTimerId ||
        g_audioTimerKind == AUDIO_TIMER_NONE ||
        hwnd != g_audioTimerHwnd ||
        !IsCurrentProcessAudioWindow(hwnd)) {
        ReleaseSRWLockExclusive(&g_audioStateLock);
        return;
    }
    
    BOOL shouldStop = FALSE;
    AudioTimerKind timerKind = g_audioTimerKind;
    
    switch (timerKind) {
        case AUDIO_TIMER_MINIAUDIO:
            if (g_deviceInitialized && g_decoderInitialized) {
                BOOL decoderAtEnd =
                    InterlockedCompareExchange(&g_decoderAtEnd, 0, 0) != 0;
                DWORD drainDeadline = (DWORD)InterlockedCompareExchange(
                    &g_decoderDrainDeadline, 0, 0);
                BOOL deviceDrained = decoderAtEnd && drainDeadline != 0 &&
                    (LONG)(GetTickCount() - drainDeadline) >= 0;
                if ((deviceDrained || !ma_device_is_started(&g_device)) &&
                    !g_isPaused) {
                    CleanupMiniaudioObjects();
                    shouldStop = TRUE;
                }
            } else {
                shouldStop = TRUE;
            }
            break;
            
        case AUDIO_TIMER_PLAYSOUND:
            PlaySoundW(NULL, NULL, SND_PURGE);
            shouldStop = TRUE;
            break;

        case AUDIO_TIMER_BEEP:
            shouldStop = TRUE;
            break;

        default:
            break;
    }
    
    if (shouldStop) {
        HWND timerHwnd = g_audioTimerHwnd ? g_audioTimerHwnd : hwnd;
        callbackHwnd = g_audioCallbackHwnd;
        callback = g_audioCompleteCallback;
        BOOL shouldNotifyCallback = callback &&
                                    callbackHwnd &&
                                    callbackHwnd == timerHwnd &&
                                    IsCurrentProcessAudioWindow(callbackHwnd);

        if (IsCurrentProcessAudioWindow(timerHwnd)) {
            KillTimer(timerHwnd, g_audioTimerId);
        }
        ResetPlaybackState();
        
        if (!shouldNotifyCallback) {
            callback = NULL;
            callbackHwnd = NULL;
        }
    }

    ReleaseSRWLockExclusive(&g_audioStateLock);

    if (callback) {
        callback(callbackHwnd);
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void SetAudioVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    AcquireSRWLockExclusive(&g_audioStateLock);

    if (g_deviceInitialized) {
        float volFloat = (float)volume / 100.0f;
        ma_device_set_master_volume(&g_device, volFloat);
    }

    ReleaseSRWLockExclusive(&g_audioStateLock);
}

void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback) {
    AcquireSRWLockExclusive(&g_audioStateLock);

    if (callback && IsCurrentProcessAudioWindow(hwnd)) {
        g_audioCallbackHwnd = hwnd;
        g_audioCompleteCallback = callback;
    } else {
        g_audioCallbackHwnd = NULL;
        g_audioCompleteCallback = NULL;
    }

    ReleaseSRWLockExclusive(&g_audioStateLock);
}

static void CleanupAudioResourcesLocked(void) {
    PlaySoundW(NULL, NULL, SND_PURGE);

    CleanupMiniaudioObjects();

    if (g_audioTimerId != 0 && IsCurrentProcessAudioWindow(g_audioTimerHwnd)) {
        KillTimer(g_audioTimerHwnd, g_audioTimerId);
    }

    ResetPlaybackState();
}

void CleanupAudioResources(void) {
    InterlockedIncrement(&g_audioPlaybackGeneration);
    AcquireSRWLockExclusive(&g_audioStateLock);
    CleanupAudioResourcesLocked();
    ReleaseSRWLockExclusive(&g_audioStateLock);
}

static BOOL PlayNotificationSoundFileInternalLocked(HWND hwnd, const char* soundFile,
                                                    BOOL allowFinalBeepFallback) {
    CleanupAudioResourcesLocked();

    if (!soundFile || soundFile[0] == '\0') {
        return TRUE;
    }

    if (strcmp(soundFile, "SYSTEM_BEEP") == 0) {
        return FallbackToSystemBeep(hwnd);
    }

    if (!IsValidFilePath(soundFile)) {
        LOG_WARNING("Invalid audio file path%s: %s",
                    allowFinalBeepFallback ? " (will fallback to system beep)" : "",
                    soundFile);
        return allowFinalBeepFallback ? FallbackToSystemBeep(hwnd) : FALSE;
    }

    AudioFileInfo fileInfo;
    if (!GetAudioFileInfo(soundFile, &fileInfo)) {
        LOG_WARNING("Cannot find audio file%s: %s",
                    allowFinalBeepFallback ? " (will fallback to system beep)" : "",
                    soundFile);
        return allowFinalBeepFallback ? FallbackToSystemBeep(hwnd) : FALSE;
    }

    if (!IsAudioFileSizeAllowed(soundFile, fileInfo.sizeBytes)) {
        return allowFinalBeepFallback ? FallbackToSystemBeep(hwnd) : FALSE;
    }

    if (PlayAudioWithMiniaudio(hwnd, soundFile, fileInfo.path)) {
        return TRUE;
    }

    LOG_WARNING("All audio playback methods failed%s",
                allowFinalBeepFallback ? ", using system beep as final fallback" : "");
    return allowFinalBeepFallback ? FallbackToSystemBeep(hwnd) : FALSE;
}

static DWORD WINAPI AudioPlaybackThreadProc(LPVOID lpParam) {
    AudioPlaybackRequest* request = (AudioPlaybackRequest*)lpParam;
    if (!request) {
        return 0;
    }

    HWND hwnd = request->hwnd;
    LONG generation = request->generation;
    char soundFile[MAX_PATH] = {0};
    strncpy(soundFile, request->soundFile, sizeof(soundFile) - 1);
    soundFile[sizeof(soundFile) - 1] = '\0';
    free(request);

    if (InterlockedCompareExchange(&g_audioPlaybackGeneration, 0, 0) != generation) {
        return 0;
    }

    AcquireSRWLockExclusive(&g_audioStateLock);
    if (InterlockedCompareExchange(&g_audioPlaybackGeneration, 0, 0) == generation) {
        PlayNotificationSoundFileInternalLocked(hwnd, soundFile, TRUE);
        if (InterlockedCompareExchange(&g_audioPlaybackGeneration, 0, 0) != generation) {
            CleanupAudioResourcesLocked();
        }
    }
    ReleaseSRWLockExclusive(&g_audioStateLock);
    return 0;
}

BOOL PlayNotificationSoundFile(HWND hwnd, const char* soundFile) {
    InterlockedIncrement(&g_audioPlaybackGeneration);
    AcquireSRWLockExclusive(&g_audioStateLock);
    BOOL result = PlayNotificationSoundFileInternalLocked(hwnd, soundFile, TRUE);
    ReleaseSRWLockExclusive(&g_audioStateLock);
    return result;
}

BOOL PreviewNotificationSoundFile(HWND hwnd, const char* soundFile) {
    InterlockedIncrement(&g_audioPlaybackGeneration);
    AcquireSRWLockExclusive(&g_audioStateLock);
    BOOL result = PlayNotificationSoundFileInternalLocked(hwnd, soundFile, FALSE);
    ReleaseSRWLockExclusive(&g_audioStateLock);
    return result;
}

BOOL PlayNotificationSound(HWND hwnd) {
    if (g_AppConfig.notification.sound.sound_file[0] == '\0') {
        return TRUE;
    }

    AudioPlaybackRequest* request = (AudioPlaybackRequest*)calloc(1, sizeof(*request));
    if (!request) {
        LOG_WARNING("Failed to allocate async audio playback request");
        return PlayNotificationSoundFile(hwnd, g_AppConfig.notification.sound.sound_file);
    }

    request->hwnd = hwnd;
    request->generation = InterlockedIncrement(&g_audioPlaybackGeneration);
    strncpy(request->soundFile, g_AppConfig.notification.sound.sound_file,
            sizeof(request->soundFile) - 1);
    request->soundFile[sizeof(request->soundFile) - 1] = '\0';

    HANDLE thread = CreateThread(NULL, 0, AudioPlaybackThreadProc, request, 0, NULL);
    if (!thread) {
        LOG_WARNING("Failed to start async audio playback thread (error=%lu)", GetLastError());
        BOOL result = PlayNotificationSoundFile(hwnd, request->soundFile);
        free(request);
        return result;
    }

    CloseHandle(thread);
    return TRUE;
}

BOOL PauseNotificationSound(void) {
    AcquireSRWLockExclusive(&g_audioStateLock);

    if (g_isPlaying && !g_isPaused && g_deviceInitialized && g_decoderInitialized) {
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
            LONG remainingMs = deadline ? (LONG)(deadline - pauseTick) : 0;
            if (remainingMs < 100) remainingMs = 100;
            if (remainingMs > 2000) remainingMs = 2000;
            InterlockedExchange(&g_decoderDrainRemainingMs, remainingMs);
            InterlockedExchange(&g_decoderDrainDeadline, 0);
        }

        if (g_audioTimerKind == AUDIO_TIMER_MINIAUDIO && g_audioTimerId != 0) {
            HWND timerHwnd = g_audioTimerHwnd;
            HWND killHwnd = timerHwnd ? timerHwnd : g_audioCallbackHwnd;
            if (IsCurrentProcessAudioWindow(killHwnd)) {
                KillTimer(killHwnd, g_audioTimerId);
            }
            g_audioTimerId = 0;
            g_audioTimerHwnd = timerHwnd;
        }
        ReleaseSRWLockExclusive(&g_audioStateLock);
        return TRUE;
    }

    ReleaseSRWLockExclusive(&g_audioStateLock);
    return FALSE;
}

BOOL ResumeNotificationSound(void) {
    AcquireSRWLockExclusive(&g_audioStateLock);

    if (g_isPlaying && g_isPaused && g_deviceInitialized && g_decoderInitialized) {
        HWND timerHwnd = g_audioTimerHwnd ? g_audioTimerHwnd : g_audioCallbackHwnd;
        BOOL timerStarted = FALSE;
        BOOL decoderAtEnd =
            InterlockedCompareExchange(&g_decoderAtEnd, 0, 0) != 0;
        LONG remainingMs = 0;

        if (g_audioTimerKind == AUDIO_TIMER_MINIAUDIO && g_audioTimerId == 0) {
            if (!IsCurrentProcessAudioWindow(timerHwnd) ||
                !StartPlaybackTimer(timerHwnd, AUDIO_TIMER_MINIAUDIO,
                                    TIMER_INTERVAL_AUDIO_CHECK)) {
                ReleaseSRWLockExclusive(&g_audioStateLock);
                return FALSE;
            }
            timerStarted = TRUE;
        }

        if (decoderAtEnd) {
            remainingMs = InterlockedCompareExchange(
                &g_decoderDrainRemainingMs, 0, 0);
            if (remainingMs < 100) remainingMs = 100;
            DWORD deadline = GetTickCount() + (DWORD)remainingMs;
            if (deadline == 0) deadline = 1;
            InterlockedExchange(&g_decoderDrainDeadline, (LONG)deadline);
        }

        InterlockedExchange(&g_decoderPaused, 0);
        if (!ma_device_is_started(&g_device) &&
            ma_device_start(&g_device) != MA_SUCCESS) {
            InterlockedExchange(&g_decoderPaused, 1);
            if (decoderAtEnd) {
                InterlockedExchange(&g_decoderDrainDeadline, 0);
                InterlockedExchange(&g_decoderDrainRemainingMs, remainingMs);
            }
            if (timerStarted) {
                KillTimer(timerHwnd, g_audioTimerId);
                g_audioTimerId = 0;
            }
            ReleaseSRWLockExclusive(&g_audioStateLock);
            return FALSE;
        }

        if (decoderAtEnd) {
            InterlockedExchange(&g_decoderDrainRemainingMs, 0);
        }
        g_isPaused = MA_FALSE;
        ReleaseSRWLockExclusive(&g_audioStateLock);
        return TRUE;
    }

    ReleaseSRWLockExclusive(&g_audioStateLock);
    return FALSE;
}

void StopNotificationSound(void) {
    CleanupAudioResources();
}
