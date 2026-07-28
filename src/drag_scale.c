/**
 * @file drag_scale.c
 * @brief Shared state for interactive window dragging and scaling.
 */

#include "drag_scale_internal.h"

BOOL PREVIOUS_TOPMOST_STATE = FALSE;
BOOL g_editModeForcedTopmost = FALSE;
BOOL g_editModeTopmostOverride = FALSE;
BOOL g_pendingScaleResizeAnchorValid = FALSE;
HWND g_pendingScaleResizeAnchorHwnd = NULL;
POINT g_pendingScaleResizeAnchor = {0};
double g_pendingScaleResizeAnchorRatioX = 0.5;
double g_pendingScaleResizeAnchorRatioY = 0.5;
UINT_PTR g_scaleApplyTimer = 0;
HWND g_scaleApplyTimerHwnd = NULL;
UINT g_scaleApplyIntervalMs = 0;
BOOL g_scaleTargetValid = FALSE;
BOOL g_scaleTargetPluginMode = FALSE;
float g_scaleTarget = 1.0f;
POINT g_scaleTargetAnchor = {0};
double g_scaleGestureAnchorRatioX = 0.5;
double g_scaleGestureAnchorRatioY = 0.5;
DWORD g_scaleGestureSerial = 0;
DWORD g_lastScaleWheelTick = 0;
DWORD g_lastScaleApplyTick = 0;
DWORD g_suppressDragUntilTick = 0;
BOOL g_dragBlockedUntilLeftUp = FALSE;
BOOL g_dragBlockNeedsReleaseCooldown = FALSE;
BOOL g_dragAnchorValid = FALSE;
POINT g_dragStartCursorPos = {0};
RECT g_dragStartWindowRect = {0};
BOOL g_pendingScaleResizeAnchorPostScale = FALSE;
DWORD g_pendingScaleResizeAnchorUntilTick = 0;
BOOL g_manualEditPositionValid = FALSE;
HWND g_manualEditPositionHwnd = NULL;
POINT g_manualEditPosition = {0};
UINT_PTR g_configSaveTimer = 0;
HWND g_configSaveTimerHwnd = NULL;
