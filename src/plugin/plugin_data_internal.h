/**
 * @file plugin_data_internal.h
 * @brief Internal interface for the split plugin-data implementation.
 */

#ifndef PLUGIN_DATA_INTERNAL_H
#define PLUGIN_DATA_INTERNAL_H

#include "plugin/plugin_data_types.h"
#include "plugin/plugin_data.h"
#include "plugin/plugin_exit.h"
#include "config.h"
#include "notification.h"
#include "../resource/resource.h"
#include "log.h"
#include "utils/string_convert.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern wchar_t* g_pluginDisplayText;
extern size_t g_pluginDisplayTextLen;
extern BOOL g_hasPluginData;
extern BOOL g_pluginModeActive;
extern volatile LONG g_forceNextUpdate;
#define g_dataCS g_pluginDataCS
extern CRITICAL_SECTION g_pluginDataCS;
extern SRWLOCK g_pluginDataLifecycleLock;
extern BOOL g_pluginDataInitialized;
extern BOOL g_pluginDataLocksInitialized;
extern BOOL g_pluginDataResourcesRetained;

extern HANDLE g_hWatchThread;
extern HANDLE g_hWatchStopEvent;
extern HANDLE g_hWatchWakeEvent;
extern HWND g_hNotifyWnd;
extern CRITICAL_SECTION g_watchCS;
extern CONDITION_VARIABLE g_watchStopCompleted;
extern BOOL g_watchStopInProgress;
extern volatile LONG g_isRunning;

extern DWORD g_lastPluginDataRedrawTick;
extern DWORD g_watchStartFailureCooldownUntil;
extern volatile LONG g_pluginDataRedrawQueued;
extern volatile LONG g_pluginDataRedrawTimerArmed;
extern volatile LONG g_pluginDataTimerRecheckQueued;
extern HWND g_pluginDataRedrawTimerHwnd;

extern char* g_lastContent;
extern size_t g_lastContentSize;
extern size_t g_lastContentCapacity;
extern FILETIME g_lastOutputWriteTime;
extern ULONGLONG g_lastOutputFileSize;
extern BOOL g_hasLastOutputFileState;
extern wchar_t g_pluginOutputDirectory[MAX_PATH];
extern wchar_t g_displaySourcePath[MAX_PATH];
extern volatile LONG g_pollIntervalMs;
extern DWORD g_lastNotifyTime;
extern PendingNotification g_pendingNotify;

void ClearPluginDisplayTextLocked(void);
BOOL EnsurePluginDisplayTextCapacityLocked(size_t requiredChars);
BOOL PluginTextHasCatimeTagW(const wchar_t* text);
BOOL PluginDisplayHasCatimeTagLocked(void);
void QueuePluginDataTimerRecheck(void);
BOOL PluginData_BeginUse(void);
void PluginData_EndUse(void);
void ResetPendingNotificationLocked(void);

BOOL IsValidPluginDataNotifyWindow(HWND hwnd);
void StopPluginDataRedrawTimer(HWND fallbackHwnd);
void RequestPluginDataRedraw(HWND hwnd);

BOOL ParseNonNegativeIntLimitedA(const char* start, const char* end, int* outValue);
BOOL ParseNonNegativeIntLimitedW(const wchar_t* start, const wchar_t* end, int* outValue);
DWORD GetPollIntervalMs(void);
void SetPollIntervalMs(DWORD intervalMs);
void ApplyContentPollInterval(BOOL hasFpsTag, DWORD parsedPollInterval);
BOOL TryParseFpsPollInterval(const char* content, DWORD* intervalOut, int* fpsOut);
size_t ClampUtf8DisplayInputLength(const char* content, size_t contentLen);
void RemoveFpsTagW(wchar_t* text);

void ParseAndShowNotifyTagW(wchar_t* text, HWND hwnd, BOOL showNotification);
BOOL PreviewReplaceExitTagW(wchar_t* text, int* textLen);
PluginParseResult ParseContent(const char* content, size_t contentLen,
                               BOOL suppressSideEffects,
                               BOOL* displayChangedOut,
                               BOOL* timerRecheckOut);

BOOL GetDefaultPluginOutputDirectoryW(wchar_t* buffer, size_t bufferSize);
BOOL SetDefaultPluginOutputDirectoryLocked(void);
BOOL EnsurePluginOutputDirectoryLocked(void);
BOOL GetPluginOutputDirectory(wchar_t* buffer, size_t bufferSize);
BOOL GetPluginOutputPathW(wchar_t* buffer, size_t bufferSize);
void SetDisplaySourcePathLocked(const wchar_t* sourcePath);
BOOL GetDirectoryFromPathW(const wchar_t* path, wchar_t* directory, size_t directorySize);
void EnsureOutputDirExistsW(const wchar_t* filePath);

size_t ChooseLastContentCacheCapacity(size_t requiredSize);
BOOL UpdateLastContentCache(const char* content, DWORD contentSize);
void ClearLastContentCacheLocked(void);
BOOL ClearPluginDisplayDataLocked(void);
void InvalidateLastOutputFileStateLocked(void);
void UpdateLastOutputFileStateLocked(const FILETIME* writeTime, ULONGLONG fileSize);
void FreePluginDataBuffersLocked(void);
void ResetPluginDataStateLocked(void);
BOOL CopyLastOutputFileStateLocked(FILETIME* writeTime, ULONGLONG* fileSize);
BOOL GetPluginOutputFileStateW(const wchar_t* filePath,
                               FILETIME* writeTime,
                               ULONGLONG* fileSize);
BOOL ProcessPluginOutputFile(const wchar_t* filePath, BOOL forceRefresh,
                             FILETIME* lastWriteTime, ULONGLONG* lastFileSize);

BOOL IsWatcherRunning(void);
void SetWatcherRunning(BOOL running);
BOOL IsWatcherStartFailureCoolingDown(DWORD now);
void MarkWatcherStartFailure(DWORD now);
void CloseWatcherEventsIfIdleLocked(void);
void WakeWatcherThreadLocked(void);
void WakeWatcherThread(void);
void CleanupCompletedWatcherThreadLocked(void);
BOOL WaitForWatcherStopGateLocked(DWORD waitMs);
DWORD WINAPI FileWatcherThread(LPVOID lpParam);
BOOL EnsureWatcherEvents(void);
BOOL StartWatcherThreadIfNeeded(void);
BOOL StopWatcherThreadIfIdle(DWORD waitMs);
void EnsurePluginDataLocksInitialized(BOOL* initializedNow);
void DeletePluginDataLocks(void);
BOOL HasRetainedWatcherThread(void);

#endif /* PLUGIN_DATA_INTERNAL_H */
