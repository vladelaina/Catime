/**
 * @file drag_scale_internal.h
 * @brief Private state and helpers shared by drag/scale implementation files.
 */

#ifndef DRAG_SCALE_INTERNAL_H
#define DRAG_SCALE_INTERNAL_H

#include "drag_scale.h"

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define SCALE_APPLY_TIMER_ID 42426
#define SCALE_APPLY_INTERVAL_MS 16u
#define SCALE_APPLY_INTERVAL_MEDIUM_MS 24u
#define SCALE_APPLY_INTERVAL_LARGE_MS 33u
#define SCALE_APPLY_INTERVAL_HUGE_MS 42u
#define SCALE_MEDIUM_WINDOW_PIXELS 500000ull
#define SCALE_LARGE_WINDOW_PIXELS 2000000ull
#define SCALE_HUGE_WINDOW_PIXELS 6000000ull
#define SCALE_APPLY_IDLE_STOP_MS 80u
#define SCALE_INITIAL_RESPONSE_MS 8u
#define SCALE_SMOOTH_RESPONSE_MS 28.0
#define SCALE_MAX_BLEND_PER_FRAME 0.52
#define SCALE_FRAME_DELTA_MAX_MS 48u
#define SCALE_SETTLE_ABS_EPSILON 0.0005f
#define SCALE_SETTLE_REL_EPSILON 0.0002f
#define SCALE_DRAG_SUPPRESS_MS 120u
#define SCALE_DRAG_RELEASE_SUPPRESS_MS 120u
#define SCALE_POST_RESIZE_ANCHOR_MS 1200u
#define EDIT_DRAG_APPLY_TIMER_ID 42428
#define EDIT_DRAG_APPLY_INTERVAL_MS 8u

extern BOOL g_editModeForcedTopmost;
extern BOOL g_editModeTopmostOverride;
extern BOOL g_pendingScaleResizeAnchorValid;
extern HWND g_pendingScaleResizeAnchorHwnd;
extern POINT g_pendingScaleResizeAnchor;
extern double g_pendingScaleResizeAnchorRatioX;
extern double g_pendingScaleResizeAnchorRatioY;
extern UINT_PTR g_scaleApplyTimer;
extern HWND g_scaleApplyTimerHwnd;
extern UINT g_scaleApplyIntervalMs;
extern BOOL g_scaleTargetValid;
extern BOOL g_scaleTargetPluginMode;
extern float g_scaleTarget;
extern POINT g_scaleTargetAnchor;
extern double g_scaleGestureAnchorRatioX;
extern double g_scaleGestureAnchorRatioY;
extern DWORD g_scaleGestureSerial;
extern DWORD g_lastScaleWheelTick;
extern DWORD g_lastScaleApplyTick;
extern DWORD g_suppressDragUntilTick;
extern BOOL g_dragBlockedUntilLeftUp;
extern BOOL g_dragBlockNeedsReleaseCooldown;
extern BOOL g_dragAnchorValid;
extern POINT g_dragStartCursorPos;
extern RECT g_dragStartWindowRect;
extern UINT_PTR g_dragApplyTimer;
extern HWND g_dragApplyTimerHwnd;
extern BOOL g_pendingScaleResizeAnchorPostScale;
extern DWORD g_pendingScaleResizeAnchorUntilTick;
extern BOOL g_manualEditPositionValid;
extern HWND g_manualEditPositionHwnd;
extern POINT g_manualEditPosition;
extern UINT_PTR g_configSaveTimer;
extern HWND g_configSaveTimerHwnd;
extern DWORD g_lastDragApplyTick;

DWORD TickElapsedMs(DWORD now, DWORD then);
UINT GetScaleApplyInterval(HWND hwnd);
BOOL IsValidDragScaleWindow(HWND hwnd);
void RefreshWindow(HWND hwnd, BOOL eraseBackground);
void ClearManualEditPosition(void);
void RecordManualEditPosition(HWND hwnd, int x, int y);

void SuppressDragForDuration(DWORD durationMs);
void SuppressDragAfterScale(void);
BOOL IsDragSuppressedAfterScale(void);
BOOL IsLeftButtonPhysicallyDown(void);
void ClearDragBlockUntilLeftUp(void);
BOOL IsDragBlockedUntilLeftUp(void);
void BlockDragUntilLeftUp(HWND hwnd);
void ClearDragAnchor(void);
BOOL SetDragAnchorFromCurrentWindow(HWND hwnd, POINT cursorPos);

void SetPendingScaleResizeAnchorWithRatio(HWND hwnd, POINT anchor,
                                          double ratioX, double ratioY);
void ForceClearPendingScaleResizeAnchor(void);
BOOL IsPostScaleResizeAnchorActive(HWND hwnd);

void ResetDragApplyThrottle(void);
BOOL ShouldApplyDragMoveNow(DWORD now);
BOOL ApplyDragPositionForCursor(HWND hwnd, POINT cursorPos);
void StopDragApplyTimer(HWND hwnd);
BOOL EnsureDragApplyTimer(HWND hwnd);
void FinishDragWindow(HWND hwnd, BOOL saveSettings,
                      BOOL refreshAfterDrag, BOOL applyFinalPosition);
void CancelDragForScale(HWND hwnd);

float ClampScaleFactor(double scale);
double CalculateWheelScaleDelta(int delta, int stepPercent,
                                float currentScale);
double ClampAnchorRatio(double ratio);
float GetActiveScaleFactor(BOOL pluginMode);
void SetActiveScaleFactor(BOOL pluginMode, float scale);
BOOL ApplyScaleToWindow(HWND hwnd, BOOL pluginMode, float newScale,
                        POINT anchor);
void StopScaleApplyTimer(HWND hwnd);
BOOL ApplyPendingScaleTarget(HWND hwnd);
BOOL ApplySmoothedScaleTarget(HWND hwnd, DWORD elapsedMs);
BOOL EnsureScaleApplyTimer(HWND hwnd);
void AdvanceScaleGestureSerial(void);

void RestoreManualTopLeftAfterEditLayout(HWND hwnd, const RECT* manualRect);

#endif /* DRAG_SCALE_INTERNAL_H */
