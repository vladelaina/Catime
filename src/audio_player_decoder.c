#define CATIME_AUDIO_IMPLEMENTATION
#include "audio_player_internal.h"

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

void CleanupMiniaudioObjects(void) {
    if (g_deviceInitialized) {
        ma_device_stop(&g_device);
        ma_device_uninit(&g_device);
        g_deviceInitialized = MA_FALSE;
    }
    if (g_decoderInitialized) {
        if (g_decoderKind == AUDIO_DECODER_WAV) {
            ma_wav_uninit(&g_decoder.wav, NULL);
        } else if (g_decoderKind == AUDIO_DECODER_MP3) {
            ma_mp3_uninit(&g_decoder.mp3, NULL);
        }
        g_decoderKind = AUDIO_DECODER_NONE;
        g_decoderInitialized = MA_FALSE;
    }
    InterlockedExchange(&g_decoderAtEnd, 0);
    InterlockedExchange(&g_decoderDrainDeadline, 0);
    InterlockedExchange(&g_decoderDrainRemainingMs, 0);
    InterlockedExchange(&g_decoderPaused, 0);
}

void CleanupMiniaudioAttempt(void) {
    CleanupMiniaudioObjects();
    ResetPlaybackState();
}

static ma_result InitTypedDecoderFileWide(const wchar_t* path) {
    ma_decoding_backend_config config =
        ma_decoding_backend_config_init(ma_format_unknown, 0);
    ZeroMemory(&g_decoder, sizeof(g_decoder));
    ma_result result = ma_wav_init_file_w(
        path, &config, NULL, &g_decoder.wav);
    if (result == MA_SUCCESS) {
        g_decoderKind = AUDIO_DECODER_WAV;
        return result;
    }
    ZeroMemory(&g_decoder, sizeof(g_decoder));
    result = ma_mp3_init_file_w(path, &config, NULL, &g_decoder.mp3);
    if (result == MA_SUCCESS) {
        g_decoderKind = AUDIO_DECODER_MP3;
        return result;
    }
    g_decoderKind = AUDIO_DECODER_NONE;
    ZeroMemory(&g_decoder, sizeof(g_decoder));
    return result;
}

static ma_result ReadTypedDecoderFrames(
    void* output, ma_uint64 frameCount, ma_uint64* framesRead) {
    if (g_decoderKind == AUDIO_DECODER_WAV) {
        return ma_wav_read_pcm_frames(
            &g_decoder.wav, output, frameCount, framesRead);
    }
    if (g_decoderKind == AUDIO_DECODER_MP3) {
        return ma_mp3_read_pcm_frames(
            &g_decoder.mp3, output, frameCount, framesRead);
    }
    if (framesRead) *framesRead = 0;
    return MA_INVALID_OPERATION;
}

static ma_result GetTypedDecoderDataFormat(
    ma_format* format, ma_uint32* channels, ma_uint32* sampleRate) {
    if (g_decoderKind == AUDIO_DECODER_WAV) {
        return ma_wav_get_data_format(
            &g_decoder.wav, format, channels, sampleRate, NULL, 0);
    }
    if (g_decoderKind == AUDIO_DECODER_MP3) {
        return ma_mp3_get_data_format(
            &g_decoder.mp3, format, channels, sampleRate, NULL, 0);
    }
    return MA_INVALID_OPERATION;
}

ma_result LoadAudioFileWide(const wchar_t* wideFilePath) {
    ma_decoding_backend_config config =
        ma_decoding_backend_config_init(ma_format_unknown, 0);
    (void)config;
    ma_result result = InitTypedDecoderFileWide(wideFilePath);
    if (result == MA_SUCCESS) g_decoderInitialized = MA_TRUE;
    return result;
}

DWORD CalculateAudioDrainDelayMs(
    const ma_device* device, ma_uint32 callbackFrameCount) {
    if (!device) return 500;
    ma_uint64 internalFrames =
        (ma_uint64)device->playback.internalPeriodSizeInFrames *
        device->playback.internalPeriods;
    if (internalFrames > INT_MAX) internalFrames = INT_MAX;
    DWORD delay = 50;
    if (device->playback.internalSampleRate > 0) {
        delay += (DWORD)MulDiv(
            (int)internalFrames, 1000,
            (int)device->playback.internalSampleRate);
    }
    if (device->sampleRate > 0 && callbackFrameCount <= INT_MAX) {
        delay += (DWORD)MulDiv(
            (int)callbackFrameCount, 1000,
            (int)device->sampleRate);
    }
    if (delay < 100) delay = 100;
    if (delay > 2000) delay = 2000;
    return delay;
}

static void AudioDataCallback(
    ma_device* device, void* output, const void* input,
    ma_uint32 frameCount) {
    (void)input;
    if (InterlockedCompareExchange(&g_decoderPaused, 0, 0)) {
        ma_silence_pcm_frames(
            output, frameCount,
            device->playback.format, device->playback.channels);
        return;
    }
    ma_uint64 framesRead = 0;
    ma_result result = ReadTypedDecoderFrames(
        output, frameCount, &framesRead);
    if (result != MA_SUCCESS || framesRead < frameCount) {
        if (framesRead < frameCount) {
            ma_silence_pcm_frames(
                ma_offset_pcm_frames_ptr(
                    output, framesRead,
                    device->playback.format,
                    device->playback.channels),
                frameCount - framesRead,
                device->playback.format,
                device->playback.channels);
        }
        DWORD deadline = GetTickCount() +
            CalculateAudioDrainDelayMs(device, frameCount);
        if (deadline == 0) deadline = 1;
        InterlockedCompareExchange(&g_decoderDrainDeadline,
                                    (LONG)deadline, 0);
        InterlockedExchange(&g_decoderAtEnd, 1);
    }
}

ma_result StartAudioPlayback(void) {
    if (!g_decoderInitialized) return MA_ERROR;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_result result = GetTypedDecoderDataFormat(
        &format, &channels, &sampleRate);
    if (result != MA_SUCCESS) return result;
    ma_device_config deviceConfig =
        ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = format;
    deviceConfig.playback.channels = channels;
    deviceConfig.sampleRate = sampleRate;
    deviceConfig.dataCallback = AudioDataCallback;
    deviceConfig.pUserData = NULL;
    result = ma_device_init(NULL, &deviceConfig, &g_device);
    if (result != MA_SUCCESS) return result;
    g_deviceInitialized = MA_TRUE;
    LONG desiredVolume = InterlockedCompareExchange(
        &g_audioDesiredVolume, 0, 0);
    if (desiredVolume < 0 || desiredVolume > 100) {
        desiredVolume = g_AppConfig.notification.sound.volume;
    }
    ma_device_set_master_volume(
        &g_device, (float)desiredVolume / 100.0f);
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

BOOL PlayAudioWithMiniaudio(
    HWND hwnd, const char* filePath, const wchar_t* wideFilePath) {
    if (!filePath || filePath[0] == '\0' ||
        !wideFilePath || wideFilePath[0] == L'\0') return FALSE;
    ma_result result = LoadAudioFileWide(wideFilePath);
    if (result != MA_SUCCESS) {
        LOG_WARNING(
            "miniaudio failed to load audio file (error: %d), falling back to PlaySound",
            result);
        CleanupMiniaudioAttempt();
        return FallbackToPlaySound(hwnd, wideFilePath);
    }
    if (StartAudioPlayback() != MA_SUCCESS) {
        LOG_WARNING(
            "miniaudio playback start failed, falling back to PlaySound");
        CleanupMiniaudioAttempt();
        return FallbackToPlaySound(hwnd, wideFilePath);
    }
    if (!StartPlaybackTimer(
            hwnd, AUDIO_TIMER_MINIAUDIO, TIMER_INTERVAL_AUDIO_CHECK)) {
        CleanupMiniaudioAttempt();
        return FallbackToPlaySound(hwnd, wideFilePath);
    }
    g_isPlaying = MA_TRUE;
    return TRUE;
}
