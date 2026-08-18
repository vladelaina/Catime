/**
 * @file tray_animation_core_internal.h
 * @brief Internal interface shared by split tray-animation modules.
 */

#ifndef TRAY_ANIMATION_CORE_INTERNAL_H
#define TRAY_ANIMATION_CORE_INTERNAL_H

#include "tray/tray_animation_core_types.h"
#include "tray/tray_animation_decoder.h"
#include "tray/tray_animation_loader.h"
#include "tray/tray_animation_playback.h"
#include "tray/tray_animation_timer.h"
#include "tray/tray_animation_percent.h"
#include "tray/tray_animation_menu.h"
#include "tray/tray_icon_lifetime.h"
#include "tray/tray_update_policy.h"
#include "utils/memory_pool.h"
#include "config.h"
#include "system_monitor.h"
#include "timer/timer.h"
#include "tray/tray.h"
#include "log.h"
#include "utils/finite_double.h"
#include "../resource/resource.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <shellapi.h>
#include <string.h>

extern char g_animationName[MAX_PATH];
extern char g_previewAnimationName[MAX_PATH];
extern BOOL g_previewAnimationFromPath;
extern BOOL g_isPreviewActive;
extern HWND g_trayHwnd;
extern volatile LONG g_baseFolderInterval;
extern volatile LONG g_userMinIntervalMs;
extern LoadedAnimation g_mainAnimation;
extern int g_mainIndex;
extern LoadedAnimation g_previewAnimation;
extern int g_previewIndex;

extern HANDLE g_previewWorkerThread;
extern HANDLE g_previewRequestEvent;
extern HANDLE g_previewStopEvent;
extern HANDLE g_previewCancelEvent;
extern volatile LONG g_previewRequestSerial;
extern char g_pendingPreviewName[MAX_PATH];
extern BOOL g_pendingPreviewFromPath;
extern BOOL g_previewWorkerRetiring;
extern DWORD g_previewWorkerStartFailureCooldownUntil;
extern SRWLOCK g_previewWorkerLock;

extern MemoryPool* g_memoryPool;
extern FrameRateController g_frameRateCtrl;
extern AnimationPlaybackState g_playbackState;
extern ULONGLONG g_lastAnimationTickMs;
extern ULONGLONG g_playbackGeneration;
extern ULONGLONG g_framePresentationSerial;
extern ULONGLONG g_lastShellPresentationSerial;
extern int g_lastShellFrameIndex;
extern int g_lastShellFrameCount;
extern BOOL g_lastShellAnimatedFrameValid;
extern char g_lastShellAnimationName[MAX_PATH];
extern HICON g_transparentTrayIcon;
extern BOOL g_mainAnimationPreloaded;
extern int g_preloadedIconCx;
extern int g_preloadedIconCy;
extern char g_lastBuiltinIconName[MAX_PATH];
extern int g_lastBuiltinIconValue;
extern COLORREF g_lastBuiltinIconTextColor;
extern COLORREF g_lastBuiltinIconBgColor;
extern int g_lastBuiltinIconCx;
extern int g_lastBuiltinIconCy;
extern char g_lastStablePercentIconName[MAX_PATH];
extern int g_lastStablePercentIconValue;
extern char g_startupRetryAnimationName[MAX_PATH];
extern HWND g_startupRetryHwnd;
extern int g_startupRetryAttemptCount;

extern CRITICAL_SECTION g_animCriticalSection;
extern volatile LONG g_criticalSectionInitialized;
extern volatile LONG g_runtimeActive;
extern volatile LONG g_runtimeUsers;
extern BOOL g_pendingTrayUpdate;
extern BOOL g_trayFrameDirty;
extern volatile LONG g_speedScaleCacheInvalidation;
extern SpeedScaleCache g_speedScaleCache;
extern UINT g_consecutiveUpdateFailures;
extern DWORD g_lastSuccessfulUpdateTime;
extern BOOL g_updateFailureReported;
extern DWORD g_lastPreviewShellFailureLogTick;
extern volatile LONG g_shellUpdateBackoffActive;
extern volatile LONG g_lastFailedUpdateAttemptTick;

void AnimationBackoffSleep(DWORD* spins);
BOOL WaitForLongToDiffer(volatile LONG* value, LONG expected, DWORD timeoutMs);
BOOL WaitForLongValue(volatile LONG* value, LONG expected, DWORD timeoutMs);
BOOL IsAnimCriticalSectionReady(void);
BOOL AnimationNeedsDecodePool(const char* name);
MemoryPool* GetTemporaryDecodePoolForAnimation(const char* name);
void ReleaseTemporaryDecodePool(void);
BOOL LoadAnimationByNameWithTemporaryPool(const char* name,
                                          LoadedAnimation* anim,
                                          int iconWidth, int iconHeight);
BOOL IsTrayAnimationRuntimeActive(void);
UINT GetBaseFolderIntervalMs(void);
UINT GetUserMinIntervalMs(void);
UINT ClampAnimationIntervalMs(UINT ms);
UINT ClampAnimationMinIntervalMs(UINT ms);
BOOL BeginTrayAnimationRuntimeUse(void);
void EndTrayAnimationRuntimeUse(void);
BOOL QuiesceTrayAnimationRuntime(void);
HICON GetTransparentTrayIcon(void);
void CleanupTransparentTrayIcon(void);
void ClearPendingTrayUpdate(void);
BOOL ClaimPendingTrayUpdate(void);
BOOL HasPendingTrayUpdate(void);
void MarkTrayFrameDirty(void);
void ClearTrayFrameDirty(void);

void ResetBuiltinIconUpdateCache(void);
BOOL IsBuiltinIconUpdateCacheCurrent(const char* name, int value,
                                     COLORREF textColor, COLORREF bgColor,
                                     int iconCx, int iconCy);
BOOL TryGetCachedBuiltinIconValue(const char* name, int* value);
BOOL IsTransientZeroPronePercentIcon(const char* name);
BOOL ShouldPreserveCachedPercentIconValue(const char* name, int sampledValue);
void RecordBuiltinIconUpdateCache(const char* name, int value,
                                  COLORREF textColor, COLORREF bgColor,
                                  int iconCx, int iconCy);
BOOL CopyStringExactA(const char* src, char* out, size_t outSize);
void InvalidateSpeedScaleCache(void);
void ResetFramePlaybackState(void);

BOOL IsValidTrayAnimationWindow(HWND hwnd);
HWND GetValidTrayAnimationWindow(void);
BOOL BuildAnimationConfigPath(const char* name, char* animPath, size_t animPathSize);
BOOL WriteAnimationConfigPathIfChanged(const char* configPath, const char* animPath);
BOOL WriteAnimationNameToConfigIfChanged(const char* name);
void ReadAnimationNameFromConfig(char* name, size_t nameSize, const char* configPath);
BOOL ShouldRunTrayAnimationTimer(void);
void EnsureTrayAnimationTimerState(void);

void SwapLoadedAnimation(LoadedAnimation* target, const LoadedAnimation* source);
void CleanupCompletedPreviewWorkerLocked(void);
void CleanupRetiredPreviewWorkerOnExit(void);
void SignalPreviewDecodeCancelLocked(void);
void WakePreviewWorkerLocked(void);
BOOL IsPreviewWorkerStartFailureCoolingDown(DWORD now);
void MarkPreviewWorkerStartFailure(DWORD now);
BOOL EnsurePreviewWorkerStartedLocked(void);
BOOL IsPreviewWorkerRetiringAfterCleanup(void);
BOOL WaitForPreviewRequestQuiet(HANDLE stopEvent, HANDLE requestEvent);
BOOL ShutdownPreviewWorker(void);
void PostPreviewLoadedMessage(void);
DWORD WINAPI PreviewWorkerThread(LPVOID param);

void NormalizeAnimConfigValue(char* s);
void RecordSuccessfulUpdate(void);
BOOL RecordFailedUpdate(void);
void HandleRepeatedTrayUpdateFailure(void);
void LogPreviewShellFailureThrottled(void);
double ComputeAnimationSpeedScalePercent(AnimationSpeedMetric metric);
BOOL IsSpeedScaleCacheFresh(DWORD now, LONG invalidationSerial);
void RefreshSpeedScaleCache(DWORD now, LONG invalidationSerial);
void GetPlaybackSpeedSnapshot(double* speedMultiplier,
                              UINT* minimumFrameIntervalMs);

void UpdateTrayIconToCurrentFrameInternal(void);
void UpdateTrayIconToCurrentFrame(void);
void UpdateTrayIconToCurrentFrameForPreview(void);
void RequestTrayIconUpdate(void);
void TrayAnimationTimerCallback(void* userData);

BOOL SetCurrentAnimationNameInternal(const char* name, BOOL persistConfig);
void CancelStartupAnimationRetry(void);
void ScheduleStartupAnimationRetry(HWND hwnd, const char* animationName);
void CALLBACK TrayAnimationStartupRetryTimerProc(HWND hwnd, UINT msg,
                                                 UINT_PTR id, DWORD time);
BOOL QueueAnimationPreviewRequest(const char* name, BOOL fromPath);
BOOL UpdatePercentIconIfNeededInternal(
    const SystemMonitorSnapshot* snapshot,
    const wchar_t* synchronizedTooltip);

#endif /* TRAY_ANIMATION_CORE_INTERNAL_H */
