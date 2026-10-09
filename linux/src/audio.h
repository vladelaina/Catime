/**
 * @file audio.h
 * @brief Alarm audio playback via miniaudio (file decode + generated beep).
 */
#ifndef CATIME_LINUX_AUDIO_H
#define CATIME_LINUX_AUDIO_H

/** Initialize the audio backend. Returns 0 on success. */
int audio_init(void);

/** Play an audio file asynchronously at @p volume (0..100). */
int audio_play_file(const char *path, int volume);

/** Play a generated beep asynchronously at @p volume (0..100). */
int audio_play_beep(int volume);

/** Stop any current playback. */
void audio_stop(void);

/** Poll for finished playback and release resources (call from main loop). */
void audio_poll(void);

void audio_shutdown(void);

#endif /* CATIME_LINUX_AUDIO_H */
