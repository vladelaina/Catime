/**
 * @file drag_scale_edit.c
 * @brief Edit-mode lifecycle and debounced window-position persistence.
 */

#include "drag_scale_internal.h"
#include "config.h"
#include "window/window_desktop_integration.h"
#include "window/window_placement.h"
#include "window_procedure/ole_drop_target.h"
#include "log.h"

void RestoreManualTopLeftAfterEditLayout(HWND hwnd, const RECT* manualRect) {
    if (!manualRect || !IsValidDragScaleWindow(hwnd)) {
        return;
    }

    RECT layoutRect = {0};
    POINT restorePosition = {0};
    if (!GetWindowRect(hwnd, &layoutRect) ||
        !WindowPlacement_GetManualTopLeftRestore(
            manualRect, &layoutRect, &restorePosition)) {
        return;
    }

    LOG_DEBUG("Restoring manual edit position after layout: (%ld, %ld) -> (%ld, %ld)",
              layoutRect.left, layoutRect.top,
              manualRect->left, manualRect->top);
    if (SetWindowPos(hwnd, NULL, restorePosition.x, restorePosition.y,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
        CLOCK_WINDOW_POS_X = restorePosition.x;
        CLOCK_WINDOW_POS_Y = restorePosition.y;
    }
}

static VOID CALLBACK ConfigSaveTimerProc(HWND hwnd, UINT msg,
                                         UINT_PTR idEvent, DWORD dwTime) {
    (void)dwTime;
    if (msg != WM_TIMER ||
        idEvent != TIMER_ID_CONFIG_SAVE ||
        hwnd != g_configSaveTimerHwnd ||
        !IsValidDragScaleWindow(hwnd)) {
        return;
    }

    KillTimer(hwnd, TIMER_ID_CONFIG_SAVE);
    g_configSaveTimer = 0;
    g_configSaveTimerHwnd = NULL;
    SaveWindowSettings(hwnd);
}

/* Debouncing: only save after operations stop for CONFIG_SAVE_DELAY_MS. */
void ScheduleConfigSave(HWND hwnd) {
    if (!IsValidDragScaleWindow(hwnd)) {
        return;
    }

    CLOCK_WINDOW_POSITION_MANUAL = TRUE;
    if (g_configSaveTimer != 0) {
        HWND timerHwnd = g_configSaveTimerHwnd ? g_configSaveTimerHwnd : hwnd;
        if (IsValidDragScaleWindow(timerHwnd)) {
            KillTimer(timerHwnd, TIMER_ID_CONFIG_SAVE);
        }
        g_configSaveTimer = 0;
        g_configSaveTimerHwnd = NULL;
    }

    g_configSaveTimer = SetTimer(hwnd, TIMER_ID_CONFIG_SAVE,
                                 CONFIG_SAVE_DELAY_MS,
                                 (TIMERPROC)ConfigSaveTimerProc);
    if (g_configSaveTimer) {
        g_configSaveTimerHwnd = hwnd;
    } else {
        SaveWindowSettings(hwnd);
    }
}

void CancelScheduledConfigSave(HWND hwnd) {
    if (g_configSaveTimer == 0) {
        return;
    }

    HWND timerHwnd = g_configSaveTimerHwnd ? g_configSaveTimerHwnd : hwnd;
    if (IsValidDragScaleWindow(timerHwnd)) {
        KillTimer(timerHwnd, TIMER_ID_CONFIG_SAVE);
    }
    g_configSaveTimer = 0;
    g_configSaveTimerHwnd = NULL;
}

void StartEditMode(HWND hwnd) {
    ClearPendingSystemPositionRestore();
    EnsureWindowVisibleWithTopmostState(hwnd);
    StopScaleApplyTimer(hwnd);
    ClearDragBlockUntilLeftUp();
    ClearManualEditPosition();

    PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
    g_editModeForcedTopmost = FALSE;
    g_editModeTopmostOverride = FALSE;
    if (!CLOCK_WINDOW_TOPMOST) {
        g_editModeForcedTopmost = TRUE;
        SetWindowTopmostTransient(hwnd, TRUE);
    }

    CLOCK_EDIT_MODE = TRUE;
    RefreshWindow(hwnd, TRUE);
    SetBlurBehind(hwnd, TRUE);
    SetClickThrough(hwnd, FALSE);
    InitializeOleDropTarget(hwnd);
    RefreshWindow(hwnd, TRUE);
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
}

void EndEditMode(HWND hwnd) {
    if (!CLOCK_EDIT_MODE) {
        return;
    }

    BOOL hadActiveDrag = CLOCK_IS_DRAGGING;
    if (hadActiveDrag) {
        /* A missed button-up must not snap to a later right-click point. */
        FinishDragWindow(hwnd, FALSE, FALSE, IsLeftButtonPhysicallyDown());
        CLOCK_WINDOW_POSITION_MANUAL = TRUE;
        MarkWindowSettingsDirty(WINDOW_SETTINGS_DIRTY_POSITION);
    }
    ApplyPendingScaleTarget(hwnd);
    StopScaleApplyTimer(hwnd);
    ConsumePendingScaleResizeAnchor(hwnd);
    ClearDragBlockUntilLeftUp();

    RECT manualRect = {0};
    BOOL hasManualRect = GetWindowRect(hwnd, &manualRect);
    if (hasManualRect && g_manualEditPositionValid &&
        g_manualEditPositionHwnd == hwnd) {
        OffsetRect(&manualRect,
                   g_manualEditPosition.x - manualRect.left,
                   g_manualEditPosition.y - manualRect.top);
    }
    CLOCK_EDIT_MODE = FALSE;

    SetBlurBehind(hwnd, FALSE);
    SetClickThrough(hwnd, TRUE);
    CleanupOleDropTarget(hwnd);
    CancelScheduledConfigSave(hwnd);

    if (g_editModeForcedTopmost && !g_editModeTopmostOverride &&
        !PREVIOUS_TOPMOST_STATE) {
        SetWindowTopmostTransient(hwnd, FALSE);
        KillTimer(hwnd, TIMER_ID_EDIT_MODE_REFRESH);
    }

    RefreshWindow(hwnd, TRUE);
    UpdateWindow(hwnd);
    if (hasManualRect) {
        RestoreManualTopLeftAfterEditLayout(hwnd, &manualRect);
    }
    SaveWindowSettings(hwnd);
    ClearManualEditPosition();
    g_editModeForcedTopmost = FALSE;
    g_editModeTopmostOverride = FALSE;
}

void MarkEditModeTopmostOverride(void) {
    if (CLOCK_EDIT_MODE) {
        g_editModeTopmostOverride = TRUE;
    }
}
