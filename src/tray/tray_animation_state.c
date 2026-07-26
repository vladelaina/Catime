/**
 * @file tray_animation_state.c
 * @brief Shared animation, preview, and runtime state.
 */

#include "tray_animation_core_internal.h"

char g_animationName[MAX_PATH] = "__logo__";
char g_previewAnimationName[MAX_PATH] = "";
BOOL g_previewAnimationFromPath = FALSE;
BOOL g_isPreviewActive = FALSE;
HWND g_trayHwnd = NULL;
volatile LONG g_baseFolderInterval = TRAY_ANIMATION_DEFAULT_INTERVAL_MS;
volatile LONG g_userMinIntervalMs = 0;

LoadedAnimation g_mainAnimation;
int g_mainIndex = 0;
LoadedAnimation g_previewAnimation;
int g_previewIndex = 0;

HANDLE g_previewWorkerThread = NULL;
HANDLE g_previewRequestEvent = NULL;
HANDLE g_previewStopEvent = NULL;
HANDLE g_previewCancelEvent = NULL;
volatile LONG g_previewRequestSerial = 0;
char g_pendingPreviewName[MAX_PATH] = "";
BOOL g_pendingPreviewFromPath = FALSE;
BOOL g_previewWorkerRetiring = FALSE;
DWORD g_previewWorkerStartFailureCooldownUntil = 0;
SRWLOCK g_previewWorkerLock = SRWLOCK_INIT;

MemoryPool* g_memoryPool = NULL;
FrameRateController g_frameRateCtrl;
AnimationPlaybackState g_playbackState;
ULONGLONG g_lastAnimationTickMs = 0;
ULONGLONG g_playbackGeneration = 0;
ULONGLONG g_framePresentationSerial = 0;
ULONGLONG g_lastShellPresentationSerial = 0;
int g_lastShellFrameIndex = -1;
int g_lastShellFrameCount = 0;
BOOL g_lastShellAnimatedFrameValid = FALSE;
char g_lastShellAnimationName[MAX_PATH] = {0};
HICON g_transparentTrayIcon = NULL;
BOOL g_mainAnimationPreloaded = FALSE;
int g_preloadedIconCx = 0;
int g_preloadedIconCy = 0;
char g_lastBuiltinIconName[MAX_PATH] = "";
int g_lastBuiltinIconValue = -1;
COLORREF g_lastBuiltinIconTextColor = CLR_INVALID;
COLORREF g_lastBuiltinIconBgColor = CLR_INVALID;
int g_lastBuiltinIconCx = 0;
int g_lastBuiltinIconCy = 0;
char g_lastStablePercentIconName[MAX_PATH] = "";
int g_lastStablePercentIconValue = -1;
char g_startupRetryAnimationName[MAX_PATH] = "";
HWND g_startupRetryHwnd = NULL;
int g_startupRetryAttemptCount = 0;

CRITICAL_SECTION g_animCriticalSection;
volatile LONG g_criticalSectionInitialized = 0;
volatile LONG g_runtimeActive = 0;
volatile LONG g_runtimeUsers = 0;
BOOL g_pendingTrayUpdate = FALSE;
BOOL g_trayFrameDirty = FALSE;
volatile LONG g_speedScaleCacheInvalidation = 0;
SpeedScaleCache g_speedScaleCache = {0};
UINT g_consecutiveUpdateFailures = 0;
DWORD g_lastSuccessfulUpdateTime = 0;
BOOL g_updateFailureReported = FALSE;
DWORD g_lastPreviewShellFailureLogTick = 0;
volatile LONG g_shellUpdateBackoffActive = 0;
volatile LONG g_lastFailedUpdateAttemptTick = 0;
