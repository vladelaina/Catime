/**
 * @file tray_internal.h
 * @brief Private tray state and cross-module helpers.
 */

#ifndef TRAY_INTERNAL_H
#define TRAY_INTERNAL_H

#include "tray/tray.h"
#include "tray/tray_hover_cache.h"
#include "tray/tray_recovery_policy.h"
#include "system_monitor.h"
#include "tray/tray_event_protocol.h"

#define TOOLTIP_UPDATE_INTERVAL_MS 1000
#define ICON_RECT_CACHE_TIMEOUT_MS 250
#define ICON_RECT_STALE_GRACE_MS 2000
#define TRAY_OPACITY_SAVE_TIMER_ID 42423
#define TRAY_RECREATE_RETRY_TIMER_ID 42430
#define TRAY_HEALTH_CHECK_TIMER_ID 42434
#define TRAY_RESUME_REFRESH_TIMER_ID 42435
#define TRAY_OPACITY_SAVE_DELAY_MS 400
#define TRAY_OPACITY_SAVE_MAX_RETRIES 3
#define TRAY_RECREATE_RETRY_DELAY_MS 750
#define TRAY_RECREATE_RETRY_MAX_ATTEMPTS 5
#define TRAY_RECREATE_BACKGROUND_RETRY_MS 30000
#define TRAY_HEALTH_CHECK_INTERVAL_MS 30000
#define TRAY_RESUME_REFRESH_DELAY_MS 2500
#define TRAY_MODIFY_FAILURE_THRESHOLD 3
#define TRAY_MODIFY_FAILURE_RESET_MS 120000
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

typedef enum {
    ANIM_TYPE_CUSTOM,
    ANIM_TYPE_LOGO,
    ANIM_TYPE_CPU,
    ANIM_TYPE_MEMORY,
    ANIM_TYPE_BATTERY,
    ANIM_TYPE_CAPSLOCK,
    ANIM_TYPE_NONE
} AnimationType;

extern NOTIFYICONDATAW nid;
extern HWND g_mainHwnd;
extern HINSTANCE g_hInstance;
extern BOOL g_trayIconActive;
extern BOOL g_trayCallbackRecoveryAllowed;
extern BOOL g_trayBackgroundWorkEnabled;
extern volatile LONG g_trayTooltipActive;
extern BOOL g_trayTipTimerActive;
extern BOOL g_traySystemMonitorActive;
extern UINT g_trayRecreateRetryCount;
extern BOOL g_trayRecreateRetryLimitLogged;
extern TrayRecoveryPolicyState g_trayRecoveryPolicyState;
extern BOOL g_trayShuttingDown;
extern UINT g_trayCallbackVersion;
extern TrayHoverRectCache g_trayIconRectCache;
extern volatile LONG g_trayInteractionSuspended;
extern int g_pendingOpacityToSave;
extern int g_opacityRollbackValue;
extern int g_pendingOpacitySaveRetryCount;
extern wchar_t g_lastTrayTooltip[256];
extern BOOL g_showingOpacityTip;

BOOL IsValidTrayMainWindow(HWND hwnd);
HWND GetValidTrayMainWindow(void);
BOOL IsTrayIconActiveForWindow(HWND hwnd);
BOOL TryRestoreTrayIconFromCallback(HWND hwnd);
void CancelTrayRecreateRetry(HWND hwnd);
void ScheduleTrayRecreateRetry(HWND hwnd);
void CancelTrayResumeRefresh(HWND hwnd);
void StartTrayHealthCheck(HWND hwnd);
void StopTrayHealthCheck(HWND hwnd);
void ReportTrayIconModifySuccess(HWND hwnd);
void ReportTrayIconModifyFailure(HWND hwnd);
void CALLBACK TrayHealthCheckTimerProc(HWND hwnd, UINT msg,
                                       UINT_PTR id, DWORD time);

AnimationType GetAnimationType(const char* animName);
BOOL IsPercentIcon(AnimationType type);
BOOL IsStaticImageFile(const char* filename);
BOOL CurrentTrayIconNeedsBackgroundRefresh(void);
BOOL CurrentTrayIconNeedsSystemMonitor(void);
BOOL CurrentAnimationSpeedNeedsSystemMonitor(void);
BOOL ShouldKeepSystemMonitorActive(void);
void EnsureTraySystemMonitorActive(void);
void ReleaseIdleTraySystemMonitor(void);
BOOL ShouldRunTrayBackgroundTimer(HWND hwnd);
BOOL GetSystemMetricsSnapshot(DWORD fields,
                              SystemMonitorSnapshot* snapshot);
const SystemMonitorSnapshot* TrayMetricSync_GetSnapshot(
    BOOL tooltipActive, BOOL iconNeedsSystemMonitor,
    SystemMonitorSnapshot* snapshot);
void TrayMetricSync_UpdateIcon(
    BOOL dynamicIcon, BOOL iconNeedsSystemMonitor,
    const SystemMonitorSnapshot* snapshot);
BOOL TrayMetricSync_UpdateIconAndTooltip(
    BOOL dynamicIcon, BOOL iconNeedsSystemMonitor,
    const SystemMonitorSnapshot* snapshot, const wchar_t* tip);

BOOL IsMouseOverTrayIconCached(POINT pt);
void CALLBACK TrayRecreateRetryTimerProc(HWND hwnd, UINT msg,
                                         UINT_PTR id, DWORD time);
BOOL HandleTrayIconMenuActivation(HWND hwnd, UINT mouseMessage,
                                  const POINT* anchor);
void Tray_LogRejectedCallback(HWND hwnd, BOOL version4, WPARAM wParam, LPARAM lParam,
                              const char* reason);

void ClearPendingTrayOpacitySave(void);
void RollBackPendingTrayOpacitySave(HWND hwnd);
void FlushPendingTrayOpacitySave(HWND hwnd);
void DiscardPendingTrayOpacitySave(void);
void EndTrayOpacityPreview(HWND hwnd);
void ReschedulePendingTrayOpacitySave(HWND hwnd);
void CompleteTrayOpacityFeedback(HWND hwnd, BOOL refreshTooltip);
void CALLBACK TrayOpacitySaveTimerProc(HWND hwnd, UINT msg,
                                       UINT_PTR id, DWORD time);

void InitTrayIconInternal(HWND hwnd, HINSTANCE hInstance,
                          BOOL preloadAnimation,
                          BOOL startBackgroundWork,
                          BOOL useAnimationInitialIcon);
void RemoveTrayIconInternal(BOOL finalCleanup);
void RegisterTaskbarCreatedMessage(void);

#endif /* TRAY_INTERNAL_H */
