/**
 * @file window_topmost_apply.c
 * @brief Application of desktop-anchored and topmost window policies
 */
#include "window/window_desktop_integration.h"
#include "window_desktop_integration_internal.h"

#include "config.h"
#include "log.h"
#include "timer/timer.h"
#include "../../resource/resource.h"

typedef struct {
    BOOL ownerApplied;
    BOOL styleApplied;
    BOOL zOrderApplied;
    DWORD zOrderError;
} TopmostApplyStatus;

static void ApplyTopmostPolicy(HWND hwnd, TopmostApplyStatus* status) {
    status->ownerApplied = WindowDesktop_TrySetOwner(hwnd, NULL);
    status->zOrderApplied = SetWindowPos(
        hwnd, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (!status->zOrderApplied) status->zOrderError = GetLastError();
    status->styleApplied = WindowDesktop_TrySetNoActivate(hwnd, FALSE);
}

static void ApplyDesktopPolicy(HWND hwnd, TopmostApplyStatus* status) {
    HWND desktop = WindowDesktop_FindWorkerWindow();

    if (desktop) {
        status->ownerApplied = WindowDesktop_TrySetOwner(hwnd, desktop);
    } else {
        LOG_WARNING("Desktop anchor unavailable while applying normal mode");
        status->ownerApplied = WindowDesktop_TrySetOwner(hwnd, NULL);
    }
    status->zOrderApplied = SetWindowPos(
        hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (!status->zOrderApplied) status->zOrderError = GetLastError();
    if (!SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
        LOG_WARNING("Failed to place desktop-attached window (error=%lu)",
                    GetLastError());
        status->zOrderApplied = FALSE;
    }
    status->styleApplied = WindowDesktop_TrySetNoActivate(hwnd, TRUE);
}

static BOOL RestorePositionAfterOwnerChange(HWND hwnd,
                                            const RECT* previousPosition) {
    RECT currentPosition = {0};

    if (!previousPosition || !GetWindowRect(hwnd, &currentPosition)) {
        return TRUE;
    }
    if (currentPosition.left == previousPosition->left &&
        currentPosition.top == previousPosition->top) {
        return TRUE;
    }
    if (!SetWindowPos(hwnd, NULL, previousPosition->left,
                      previousPosition->top, 0, 0,
                      SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
        LOG_WARNING("Failed to restore position after owner change (error=%lu)",
                    GetLastError());
        return FALSE;
    }
    CLOCK_WINDOW_POS_X = previousPosition->left;
    CLOCK_WINDOW_POS_Y = previousPosition->top;
    return TRUE;
}

void WindowTopmost_LogDiagnostics(HWND hwnd, const char* phase,
                                  BOOL requestedTopmost) {
    LONG style;
    DWORD styleError;
    HWND owner;
    BOOL actualTopmost = FALSE;
    BOOL actualKnown;

    SetLastError(0);
    style = GetWindowLong(hwnd, GWL_EXSTYLE);
    styleError = GetLastError();
    owner = (HWND)GetWindowLongPtr(hwnd, GWLP_HWNDPARENT);
    actualKnown = WindowDesktop_GetTopmostState(hwnd, &actualTopmost);
    LOG_WARNING("Topmost diagnostics [%s]: requested=%d preference=%d runtime=%d actualKnown=%d actual=%d style=0x%08lX styleError=%lu owner=0x%p edit=%d visible=%d iconic=%d",
                phase ? phase : "unknown", requestedTopmost,
                CLOCK_WINDOW_TOPMOST, CLOCK_WINDOW_EFFECTIVE_TOPMOST,
                actualKnown, actualTopmost, (unsigned long)style,
                styleError, owner, CLOCK_EDIT_MODE,
                IsWindowVisible(hwnd), IsIconic(hwnd));
}

BOOL WindowTopmost_ApplyInternal(HWND hwnd, BOOL topmost,
                                 BOOL persistConfig,
                                 BOOL updatePreference,
                                 BOOL updateRuntimeTarget,
                                 BOOL scheduleRetry) {
    TopmostApplyStatus status = {TRUE, TRUE, FALSE, ERROR_SUCCESS};
    RECT previousPosition = {0};
    BOOL hasPreviousPosition;
    BOOL actualTopmost = FALSE;
    BOOL actualKnown;
    BOOL succeeded;

    if (!updatePreference && !persistConfig && !updateRuntimeTarget &&
        WindowTopmostRetry_IsCoolingDown(topmost)) {
        return FALSE;
    }
    if (persistConfig && !WriteConfigTopmost(topmost ? "TRUE" : "FALSE")) {
        LOG_WARNING("Topmost preference was not changed because persistence failed");
        return FALSE;
    }
    if (updatePreference) CLOCK_WINDOW_TOPMOST = topmost;
    if (updateRuntimeTarget) CLOCK_WINDOW_EFFECTIVE_TOPMOST = topmost;
    if (WindowDesktop_IsTopmostStateApplied(hwnd, topmost)) {
        WindowTopmostRetry_Clear(hwnd);
        return TRUE;
    }

    hasPreviousPosition = GetWindowRect(hwnd, &previousPosition);
    if (topmost) {
        ApplyTopmostPolicy(hwnd, &status);
    } else {
        ApplyDesktopPolicy(hwnd, &status);
    }
    if (!SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                      SWP_FRAMECHANGED)) {
        LOG_WARNING("Failed to refresh window frame after Z-order update (error=%lu)",
                    GetLastError());
        status.styleApplied = FALSE;
    }
    if (hasPreviousPosition &&
        !RestorePositionAfterOwnerChange(hwnd, &previousPosition)) {
        status.zOrderApplied = FALSE;
    }

    actualKnown = WindowDesktop_GetTopmostState(hwnd, &actualTopmost);
    if (actualKnown && actualTopmost != topmost) {
        LOG_WARNING("Topmost state mismatch: requested=%d actual=%d",
                    topmost, actualTopmost);
        status.zOrderApplied = FALSE;
    }
    succeeded = status.zOrderApplied && status.styleApplied && actualKnown &&
                actualTopmost == topmost &&
                (topmost || status.ownerApplied);
    if (succeeded) {
        WindowTopmostRetry_Clear(hwnd);
        return TRUE;
    }

    LOG_WARNING("Topmost apply incomplete: requested=%d zOrder=%d owner=%d style=%d actualKnown=%d actual=%d error=%lu",
                topmost, status.zOrderApplied, status.ownerApplied,
                status.styleApplied, actualKnown, actualTopmost,
                status.zOrderError);
    WindowTopmost_LogDiagnostics(hwnd, "apply-failure", topmost);
    if (scheduleRetry) {
        if (updatePreference || persistConfig) {
            WindowTopmostRetry_ResetForRequest();
        }
        WindowTopmostRetry_Schedule(hwnd, topmost);
    }
    return FALSE;
}

BOOL SetWindowTopmost(HWND hwnd, BOOL topmost) {
    if (!WindowDesktop_IsValid(hwnd, "SetWindowTopmost")) return FALSE;
    return WindowTopmost_ApplyInternal(hwnd, topmost, TRUE, TRUE, TRUE, TRUE);
}

BOOL SetWindowTopmostFromConfig(HWND hwnd, BOOL topmost) {
    if (!WindowDesktop_IsValid(hwnd, "SetWindowTopmostFromConfig")) {
        return FALSE;
    }
    return WindowTopmost_ApplyInternal(hwnd, topmost, FALSE, TRUE, TRUE, TRUE);
}

BOOL SetWindowTopmostTransient(HWND hwnd, BOOL topmost) {
    if (!WindowDesktop_IsValid(hwnd, "SetWindowTopmostTransient")) {
        return FALSE;
    }
    return WindowTopmost_ApplyInternal(hwnd, topmost, FALSE, FALSE, TRUE,
                                       TRUE);
}

BOOL RefreshWindowTopmostState(HWND hwnd) {
    if (!WindowDesktop_IsValid(hwnd, "RefreshWindowTopmostState")) {
        return FALSE;
    }
    return WindowTopmost_ApplyInternal(hwnd,
                                       CLOCK_WINDOW_EFFECTIVE_TOPMOST,
                                       FALSE, FALSE, FALSE, TRUE);
}
