#ifndef DIALOG_NOTIFICATION_AUDIO_INTERNAL_H
#define DIALOG_NOTIFICATION_AUDIO_INTERNAL_H

#include "dialog/dialog_notification_audio.h"
#include "config.h"
#include "language.h"
#include "audio_player.h"
#include "utils/natural_sort.h"
#include "utils/directory_watcher.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOTIFICATION_SOUND_ENTRY_LIMIT 256
#define NOTIFICATION_SOUND_SCAN_ENTRY_LIMIT 4096
#define NOTIFICATION_SOUND_SCAN_STOP_TIMEOUT_MS 2000
#define NOTIFICATION_SOUND_SCAN_REFRESH_COOLDOWN_MS 2000
#define NOTIFICATION_SOUND_SCAN_FAILED (-1)

extern wchar_t g_soundFileCache[NOTIFICATION_SOUND_ENTRY_LIMIT][MAX_PATH];
extern int g_soundFileCacheCount;
extern BOOL g_soundFileCacheReady;
extern BOOL g_soundFileCacheFailed;
extern SRWLOCK g_soundFileCacheLock;
extern SRWLOCK g_soundScanThreadLock;
extern SRWLOCK g_soundCacheNotifyLock;
extern HANDLE g_hSoundScanThread;
extern HANDLE g_hRetiredSoundScanThread;
extern DirectoryWatcher g_soundFolderWatcher;
extern HWND g_soundCacheNotifyHwnd;
extern volatile LONG g_soundScanShuttingDown;
extern volatile LONG g_soundScanGeneration;
extern volatile LONG g_soundFileLastScanTick;

BOOL NotificationAudio_IsScanShuttingDown(void);
BOOL NotificationAudio_IsScanCanceled(LONG generation);
BOOL NotificationAudio_IsCurrentProcessWindow(HWND hwnd);
void NotificationAudio_NotifyCacheUpdated(void);
BOOL NotificationAudio_IsCacheRecentlyScanned(DWORD now);
BOOL NotificationAudio_IsSupportedFileName(const wchar_t* fileName);
BOOL NotificationAudio_GetCurrentFileName(
    const char* currentFile, wchar_t* outFileName, size_t outSize);
BOOL NotificationAudio_StoreCache(
    const wchar_t* files, int fileCount, LONG generation);
void NotificationAudio_MarkCacheScanFailed(void);
int NotificationAudio_CopyCache(
    wchar_t files[][MAX_PATH], int capacity, BOOL* cacheReady);

DWORD WINAPI NotificationAudio_ScanThread(LPVOID lpParam);
void NotificationAudio_StartFolderWatcher(void);
void NotificationAudio_StopFolderWatcher(void);

BOOL NotificationAudio_CloseCompletedScanThreadLocked(DWORD waitMs);
BOOL NotificationAudio_CloseRetiredScanThreadLocked(DWORD waitMs);
void NotificationAudio_RequestCacheScanAsync(void);

#endif
