#ifndef AUDIO_PLAYER_INTERNAL_H
#define AUDIO_PLAYER_INTERNAL_H

#include "audio_player.h"
#include "config.h"
#include "log.h"
#if !defined(CATIME_AUDIO_IMPLEMENTATION) && defined(MINIAUDIO_IMPLEMENTATION)
#define CATIME_AUDIO_RESTORE_MINIAUDIO_IMPLEMENTATION
#undef MINIAUDIO_IMPLEMENTATION
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4245 4456)
#endif
#include "../libs/miniaudio/miniaudio.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#ifdef CATIME_AUDIO_RESTORE_MINIAUDIO_IMPLEMENTATION
#undef CATIME_AUDIO_RESTORE_MINIAUDIO_IMPLEMENTATION
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>

#define TIMER_INTERVAL_AUDIO_CHECK 500
#define TIMER_INTERVAL_FALLBACK 3000
#define TIMER_INTERVAL_BEEP 500
#define AUDIO_TIMER_ID_BASE ((UINT_PTR)0xA7000000u)
#define AUDIO_TIMER_ID_MASK 0xFFFFu
#define MAX_NOTIFICATION_AUDIO_BYTES (64ull * 1024ull * 1024ull)

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

extern ma_device g_device;
extern ma_bool32 g_decoderInitialized;
extern ma_bool32 g_deviceInitialized;
extern volatile LONG g_decoderAtEnd;
extern volatile LONG g_decoderDrainDeadline;
extern volatile LONG g_decoderDrainRemainingMs;
extern volatile LONG g_decoderPaused;
extern ma_bool32 g_isPlaying;
extern ma_bool32 g_isPaused;
extern AudioPlaybackCompleteCallback g_audioCompleteCallback;
extern HWND g_audioCallbackHwnd;
extern UINT_PTR g_audioTimerId;
extern AudioTimerKind g_audioTimerKind;
extern HWND g_audioTimerHwnd;
extern volatile LONG g_audioTimerSerial;
extern volatile LONG g_audioPlaybackGeneration;
extern SRWLOCK g_audioStateLock;

BOOL IsCurrentProcessAudioWindow(HWND hwnd);
BOOL GetWideCharPath(
    const char* utf8Path, wchar_t* widePath, size_t widePathSize);
BOOL GetAudioFileInfo(const char* filePath, AudioFileInfo* info);
BOOL IsAudioFileSizeAllowed(const char* filePath, ULONGLONG fileSize);
BOOL IsValidFilePath(const char* filePath);
void ResetPlaybackState(void);
BOOL StartPlaybackTimer(HWND hwnd, AudioTimerKind timerKind, UINT interval);
BOOL FallbackToPlaySound(HWND hwnd, const wchar_t* wideFilePath);
BOOL FallbackToSystemBeep(HWND hwnd);
void CleanupMiniaudioObjects(void);
void CleanupMiniaudioAttempt(void);
ma_result LoadAudioFileWide(const wchar_t* wideFilePath);
DWORD CalculateAudioDrainDelayMs(
    const ma_device* device, ma_uint32 callbackFrameCount);
ma_result StartAudioPlayback(void);
BOOL PlayAudioWithMiniaudio(
    HWND hwnd, const char* filePath, const wchar_t* wideFilePath);
void CALLBACK AudioTimerCallback(
    HWND hwnd, UINT message, UINT_PTR idEvent, DWORD time);
void CleanupAudioResourcesLocked(void);

#endif
