/**
 * @file audio.c
 * @brief miniaudio-based alarm playback (file decode + synthesized beep).
 *
 * miniaudio is built here (MINIAUDIO_IMPLEMENTATION). On Linux it auto-selects
 * PulseAudio/ALSA/etc., so we deliberately do NOT restrict backends the way the
 * Windows build does.
 */
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

typedef enum { SRC_NONE = 0, SRC_FILE, SRC_BEEP } SourceType;

static ma_context g_ctx;
static int g_ctx_ok = 0;

static ma_device g_device;
static ma_decoder g_decoder;
static int g_device_active = 0;
static SourceType g_source = SRC_NONE;
static int g_finished = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* beep generator state */
static double g_beep_phase = 0.0;
static ma_uint64 g_beep_frames_left = 0;
#define BEEP_HZ 880.0
#define BEEP_SECONDS 0.5

static void on_data(ma_device *pDevice, void *pOutput, const void *pInput,
                    ma_uint32 frameCount) {
    (void)pInput;
    pthread_mutex_lock(&g_lock);
    SourceType src = g_source;
    pthread_mutex_unlock(&g_lock);

    if (src == SRC_FILE) {
        ma_uint64 framesRead = 0;
        ma_decoder_read_pcm_frames(&g_decoder, pOutput, frameCount, &framesRead);
        if (framesRead < frameCount) {
            /* zero-fill remainder and signal completion */
            ma_uint32 chans = g_device.playback.channels;
            float *out = (float *)pOutput;
            for (ma_uint64 i = framesRead * chans; i < (ma_uint64)frameCount * chans; i++)
                out[i] = 0.0f;
            pthread_mutex_lock(&g_lock);
            if (!g_finished) g_finished = 1;
            pthread_mutex_unlock(&g_lock);
        }
    } else if (src == SRC_BEEP) {
        float *out = (float *)pOutput;
        ma_uint32 chans = pDevice->playback.channels;
        double step = 2.0 * M_PI * BEEP_HZ / (double)pDevice->sampleRate;
        for (ma_uint32 i = 0; i < frameCount; i++) {
            double sample = 0.0;
            pthread_mutex_lock(&g_lock);
            if (g_beep_frames_left > 0) {
                g_beep_frames_left--;
                sample = sin(g_beep_phase);
                g_beep_phase += step;
            } else if (!g_finished) {
                g_finished = 1;
            }
            pthread_mutex_unlock(&g_lock);
            for (ma_uint32 c = 0; c < chans; c++)
                out[i * chans + c] = (float)sample;
        }
    } else {
        memset(pOutput, 0, frameCount * pDevice->playback.channels * sizeof(float));
    }
    (void)pDevice;
}

int audio_init(void) {
    if (g_ctx_ok) return 0;
    ma_result r = ma_context_init(NULL, 0, NULL, &g_ctx);
    if (r != MA_SUCCESS) {
        LOG_WARNING("audio: context init failed: %d", r);
        return -1;
    }
    g_ctx_ok = 1;
    return 0;
}

static void teardown_locked(void) {
    if (g_device_active) {
        ma_device_uninit(&g_device);
        g_device_active = 0;
    }
    if (g_source == SRC_FILE) {
        ma_decoder_uninit(&g_decoder);
    }
    g_source = SRC_NONE;
    g_finished = 0;
    g_beep_phase = 0.0;
    g_beep_frames_left = 0;
}

void audio_stop(void) {
    pthread_mutex_lock(&g_lock);
    teardown_locked();
    pthread_mutex_unlock(&g_lock);
}

void audio_poll(void) {
    int done = 0;
    pthread_mutex_lock(&g_lock);
    if (g_finished) {
        done = 1;
        teardown_locked();
    }
    pthread_mutex_unlock(&g_lock);
    (void)done;
}

void audio_shutdown(void) {
    audio_stop();
    if (g_ctx_ok) {
        ma_context_uninit(&g_ctx);
        g_ctx_ok = 0;
    }
}

static int start_playback(int channels, ma_uint32 sample_rate, float volume) {
    if (!g_ctx_ok) audio_init();
    if (!g_ctx_ok) return -1;

    pthread_mutex_lock(&g_lock);
    teardown_locked();
    pthread_mutex_unlock(&g_lock);

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = channels;
    cfg.sampleRate = sample_rate;
    cfg.dataCallback = on_data;
    cfg.pUserData = NULL;

    ma_result r = ma_device_init(&g_ctx, &cfg, &g_device);
    if (r != MA_SUCCESS) {
        LOG_WARNING("audio: device init failed: %d", r);
        pthread_mutex_lock(&g_lock);
        teardown_locked();
        pthread_mutex_unlock(&g_lock);
        return -1;
    }
    g_device_active = 1;
    ma_device_set_master_volume(&g_device, volume);

    r = ma_device_start(&g_device);
    if (r != MA_SUCCESS) {
        LOG_WARNING("audio: device start failed: %d", r);
        pthread_mutex_lock(&g_lock);
        teardown_locked();
        pthread_mutex_unlock(&g_lock);
        return -1;
    }
    return 0;
}

int audio_play_file(const char *path, int volume) {
    if (!path || !*path) return audio_play_beep(volume);
    if (!g_ctx_ok) audio_init();
    if (!g_ctx_ok) return -1;

    ma_decoder_config dc = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_result r = ma_decoder_init_file(path, NULL, &g_decoder);
    if (r != MA_SUCCESS) {
        LOG_WARNING("audio: cannot decode %s: %d", path, r);
        return audio_play_beep(volume);
    }
    /* request f32 output matching decoder channels/rate */
    dc = ma_decoder_config_init(ma_format_f32, g_decoder.outputChannels,
                                g_decoder.outputSampleRate);
    ma_decoder_uninit(&g_decoder);
    r = ma_decoder_init_file(path, NULL, &g_decoder);
    if (r != MA_SUCCESS) {
        return audio_play_beep(volume);
    }
    (void)dc;

    int channels = g_decoder.outputChannels ? g_decoder.outputChannels : 2;
    ma_uint32 rate = g_decoder.outputSampleRate ? g_decoder.outputSampleRate : 44100;

    pthread_mutex_lock(&g_lock);
    g_source = SRC_FILE;
    g_finished = 0;
    pthread_mutex_unlock(&g_lock);

    float vol = volume < 0 ? 0 : (volume > 100 ? 1.0f : volume / 100.0f);
    if (start_playback(channels, rate, vol) != 0) {
        pthread_mutex_lock(&g_lock);
        teardown_locked();
        pthread_mutex_unlock(&g_lock);
        return -1;
    }
    return 0;
}

int audio_play_beep(int volume) {
    if (!g_ctx_ok) audio_init();
    if (!g_ctx_ok) return -1;

    pthread_mutex_lock(&g_lock);
    g_source = SRC_BEEP;
    g_finished = 0;
    g_beep_phase = 0.0;
    g_beep_frames_left = (ma_uint64)(44100 * BEEP_SECONDS);
    pthread_mutex_unlock(&g_lock);

    float vol = volume < 0 ? 0 : (volume > 100 ? 1.0f : volume / 100.0f);
    if (start_playback(2, 44100, vol) != 0) {
        pthread_mutex_lock(&g_lock);
        teardown_locked();
        pthread_mutex_unlock(&g_lock);
        return -1;
    }
    return 0;
}
