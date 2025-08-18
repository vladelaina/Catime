#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <windows.h>

typedef void (*AudioPlaybackCompleteCallback)(HWND hwnd);

void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback);

BOOL PlayNotificationSound(HWND hwnd);

BOOL PauseNotificationSound(void);

BOOL ResumeNotificationSound(void);

void StopNotificationSound(void);

void SetAudioVolume(int volume);

#endif