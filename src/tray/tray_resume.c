/**
 * @file tray_resume.c
 * @brief Deferred tray recovery after system sleep or hibernation.
 */

#include "tray_internal.h"
#include "tray/tray_animation_core.h"
#include "log.h"
#include <string.h>

static BOOL g_trayResumeRefreshPending = FALSE;

static void CALLBACK TrayResumeRefreshTimerProc(
    HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)time;
    if (msg != WM_TIMER || id != TRAY_RESUME_REFRESH_TIMER_ID) {
        return;
    }

    KillTimer(hwnd, TRAY_RESUME_REFRESH_TIMER_ID);
    g_trayResumeRefreshPending = FALSE;
    if (g_trayShuttingDown || !IsValidTrayMainWindow(hwnd)) {
        return;
    }

    if (!IsTrayIconActiveForWindow(hwnd)) {
        ScheduleTrayRecreateRetry(hwnd);
        return;
    }

    /* Keep the existing resume behavior of reloading animation resources,
     * but perform it after Explorer has had time to rebuild its tray window. */
    char currentAnimation[MAX_PATH] = {0};
    const char* currentAnimationName = GetCurrentAnimationName();
    if (currentAnimationName) {
        strncpy_s(currentAnimation, sizeof(currentAnimation),
                  currentAnimationName, _TRUNCATE);
    }
    TrayAnimation_ClearCurrentName();
    ApplyAnimationPathValueNoPersist(
        currentAnimation[0] ? currentAnimation : "__logo__");

    /* A notification icon normally survives sleep.  Probe it without first
     * deleting it; TaskbarCreated and the health policy own destructive
     * recreation when Explorer has actually lost the registration. */
    StartTrayHealthCheck(hwnd);
    TrayAnimation_RefreshCurrentIcon();
    RefreshTrayBackgroundWorkState();
}

void CancelTrayResumeRefresh(HWND hwnd) {
    if (hwnd) {
        KillTimer(hwnd, TRAY_RESUME_REFRESH_TIMER_ID);
    }
    g_trayResumeRefreshPending = FALSE;
}

void ScheduleTrayResumeRefresh(HWND hwnd) {
    if (g_trayShuttingDown || !IsValidTrayMainWindow(hwnd)) {
        return;
    }

    BOOL wasPending = g_trayResumeRefreshPending;
    if (!SetTimer(hwnd, TRAY_RESUME_REFRESH_TIMER_ID,
                  TRAY_RESUME_REFRESH_DELAY_MS,
                  TrayResumeRefreshTimerProc)) {
        LOG_WARNING("Failed to schedule post-resume tray refresh (error=%lu)",
                    GetLastError());
        g_trayResumeRefreshPending = FALSE;
        if (!IsTrayIconActiveForWindow(hwnd)) {
            ScheduleTrayRecreateRetry(hwnd);
        }
        return;
    }
    g_trayResumeRefreshPending = TRUE;
    if (!wasPending) {
        LOG_INFO("System resumed from sleep/hibernate; tray refresh scheduled");
    }
}
