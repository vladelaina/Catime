/**
 * @file tray_opacity.c
 * @brief Tray-wheel opacity preview and debounced persistence.
 */

#include "tray_internal.h"
#include "config.h"
#include "config/config_defaults.h"
#include "language.h"
#include "preview_display.h"
#include "log.h"
#include <stdio.h>

void ClearPendingTrayOpacitySave(void) {
    g_pendingOpacityToSave = -1;
    g_opacityRollbackValue = -1;
    g_pendingOpacitySaveRetryCount = 0;
}

void RollBackPendingTrayOpacitySave(HWND hwnd) {
    if (g_opacityRollbackValue >= 0) {
        CLOCK_WINDOW_OPACITY = g_opacityRollbackValue;
        if (IsValidTrayMainWindow(hwnd)) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
    }
    ClearPendingTrayOpacitySave();
}

void FlushPendingTrayOpacitySave(HWND hwnd) {
    if (g_pendingOpacityToSave < 0) {
        return;
    }

    char configPath[MAX_PATH];
    GetConfigPath(configPath, sizeof(configPath));
    if (!WriteIniInt(INI_SECTION_DISPLAY, "WINDOW_OPACITY",
                     g_pendingOpacityToSave, configPath)) {
        g_pendingOpacitySaveRetryCount++;
        if (g_pendingOpacitySaveRetryCount >=
            TRAY_OPACITY_SAVE_MAX_RETRIES) {
            LOG_WARNING("Failed to save tray opacity after %d attempts; dropping pending value: %d",
                        g_pendingOpacitySaveRetryCount,
                        g_pendingOpacityToSave);
            RollBackPendingTrayOpacitySave(hwnd);
            return;
        }
        LOG_WARNING("Failed to save tray opacity: %d (attempt %d/%d)",
                    g_pendingOpacityToSave,
                    g_pendingOpacitySaveRetryCount,
                    TRAY_OPACITY_SAVE_MAX_RETRIES);
        return;
    }
    ClearPendingTrayOpacitySave();
}

void DiscardPendingTrayOpacitySave(void) {
    ClearPendingTrayOpacitySave();
}

void EndTrayOpacityPreview(HWND hwnd) {
    RestoreWindowVisibility(hwnd);
}

void ReschedulePendingTrayOpacitySave(HWND hwnd) {
    if (g_pendingOpacityToSave < 0 || !IsValidTrayMainWindow(hwnd)) {
        return;
    }
    if (!SetTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID,
                  TRAY_OPACITY_SAVE_DELAY_MS,
                  TrayOpacitySaveTimerProc)) {
        LOG_WARNING("Failed to reschedule pending tray opacity save (error=%lu)",
                    GetLastError());
        RollBackPendingTrayOpacitySave(hwnd);
    }
}

void CompleteTrayOpacityFeedback(HWND hwnd, BOOL refreshTooltip) {
    FlushPendingTrayOpacitySave(hwnd);
    if (g_showingOpacityTip) {
        g_showingOpacityTip = FALSE;
        EndTrayOpacityPreview(hwnd);
        if (refreshTooltip && IsTrayTooltipActive() && hwnd && nid.hWnd) {
            TrayTipTimerProc(hwnd, WM_TIMER, TRAY_TIP_TIMER_ID, 0);
        }
        RefreshTrayBackgroundWorkState();
    }
}

void CALLBACK TrayOpacitySaveTimerProc(HWND hwnd, UINT msg,
                                       UINT_PTR id, DWORD time) {
    (void)msg;
    (void)time;
    if (id != TRAY_OPACITY_SAVE_TIMER_ID ||
        !IsValidTrayMainWindow(hwnd)) {
        return;
    }

    KillTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID);
    CompleteTrayOpacityFeedback(hwnd, TRUE);
    if (g_pendingOpacityToSave >= 0 &&
        !SetTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID,
                  TRAY_OPACITY_SAVE_DELAY_MS,
                  TrayOpacitySaveTimerProc)) {
        LOG_WARNING("Failed to reschedule tray opacity save retry (error=%lu)",
                    GetLastError());
        RollBackPendingTrayOpacitySave(hwnd);
    }
}

void HandleTrayOpacityWheel(HWND hwnd, int wheelDirection,
                            BOOL ctrlPressed) {
    if (!IsValidTrayMainWindow(hwnd)) {
        return;
    }

    extern int ReadConfigOpacityStepNormal(void);
    extern int ReadConfigOpacityStepFast(void);
    int step = ctrlPressed ? ReadConfigOpacityStepFast()
                           : ReadConfigOpacityStepNormal();
    if (step <= 0) step = 1;

    int oldOpacity = CLOCK_WINDOW_OPACITY;
    CLOCK_WINDOW_OPACITY += wheelDirection > 0 ? step : -step;
    if (CLOCK_WINDOW_OPACITY < MIN_VISIBLE_OPACITY) {
        CLOCK_WINDOW_OPACITY = MIN_VISIBLE_OPACITY;
    }
    if (CLOCK_WINDOW_OPACITY > MAX_OPACITY) {
        CLOCK_WINDOW_OPACITY = MAX_OPACITY;
    }

    ShowWindowForPreview(hwnd);
    g_showingOpacityTip = TRUE;
    wchar_t opacityTip[64];
    _snwprintf_s(opacityTip, _countof(opacityTip), _TRUNCATE,
                 L"%s: %d%%",
                 GetLocalizedString(NULL, L"Tray Tooltip Opacity"),
                 CLOCK_WINDOW_OPACITY);
    UpdateTrayTooltip(opacityTip);

    if (CLOCK_WINDOW_OPACITY != oldOpacity) {
        InvalidateRect(hwnd, NULL, FALSE);
        if (g_pendingOpacityToSave < 0) {
            g_opacityRollbackValue = oldOpacity;
        }
        g_pendingOpacityToSave = CLOCK_WINDOW_OPACITY;
        g_pendingOpacitySaveRetryCount = 0;
    }

    if (!SetTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID,
                  TRAY_OPACITY_SAVE_DELAY_MS,
                  TrayOpacitySaveTimerProc)) {
        CompleteTrayOpacityFeedback(hwnd, TRUE);
        if (g_pendingOpacityToSave >= 0) {
            LOG_WARNING("Dropping pending tray opacity save after timer start failure");
            RollBackPendingTrayOpacitySave(hwnd);
        }
    }
}
