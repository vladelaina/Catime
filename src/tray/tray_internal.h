/**
 * @file tray_internal.h
 * @brief Private tray state and cross-module helpers.
 */

#ifndef TRAY_INTERNAL_H
#define TRAY_INTERNAL_H

#include "tray/tray.h"
#include "tray/tray_hover_cache.h"

#define TOOLTIP_UPDATE_INTERVAL_MS 1000
#define ICON_RECT_CACHE_TIMEOUT_MS 250
#define ICON_RECT_STALE_GRACE_MS 2000
#define TRAY_OPACITY_SAVE_TIMER_ID 42423
#define TRAY_RECREATE_RETRY_TIMER_ID 42430
#define TRAY_OPACITY_SAVE_DELAY_MS 400
#define TRAY_OPACITY_SAVE_MAX_RETRIES 3
#define TRAY_RECREATE_RETRY_DELAY_MS 750
#define TRAY_RECREATE_RETRY_MAX_ATTEMPTS 5
#define TRAY_RECREATE_RETRY_RESET_MS 30000
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
extern HHOOK g_mouseHook;
extern HWND g_mainHwnd;
extern HINSTANCE g_hInstance;
extern BOOL g_trayIconActive;
extern BOOL g_trayBackgroundWorkEnabled;
extern volatile LONG g_trayTooltipActive;
extern BOOL g_trayTipTimerActive;
extern BOOL g_traySystemMonitorActive;
extern DWORD g_lastMouseHookInstallAttemptTick;
extern DWORD g_lastMouseHookInstallWarningTick;
extern DWORD g_lastMouseHookReleaseWarningTick;
extern UINT g_trayRecreateRetryCount;
extern DWORD g_lastTrayRecreateRetryTick;
extern BOOL g_trayRecreateRetryLimitLogged;
extern BOOL g_trayShuttingDown;
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
void CancelTrayRecreateRetry(HWND hwnd);
void ScheduleTrayRecreateRetry(HWND hwnd);

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
void GetSystemMetricsWithWarmup(float* cpu, float* mem);

BOOL IsMouseOverTrayIconCached(POINT pt);
BOOL TryReleaseTrayMouseHook(void);
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
void CALLBACK TrayRecreateRetryTimerProc(HWND hwnd, UINT msg,
                                         UINT_PTR id, DWORD time);

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
