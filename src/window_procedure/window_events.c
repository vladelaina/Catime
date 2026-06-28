/**
 * @file window_events.c
 * @brief Window lifecycle and state management event handlers
 */
#include <windows.h>
#include "window.h"
#include "tray/tray.h"
#include "config.h"
#include "drag_scale.h"
#include "window_procedure/window_events.h"
#include "timer/main_timer.h"
#include "window/window_visual_effects.h"
#include "window/window_desktop_integration.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/ole_drop_target.h"
#include "audio_player.h"
#include "log.h"
#include "timer/timer.h"
#include "tray/tray_animation_core.h"
#include "async_update_checker.h"
#include "drawing/drawing_render.h"
#include "../resource/resource.h"

/* ============================================================================
 * Window creation and initialization
 * ============================================================================ */

/**
 * Handle WM_CREATE initialization: click-through and topmost mode.
 *
 * @param hwnd Window handle of newly created window
 * @return TRUE if initialization succeeded
 */
BOOL HandleWindowCreate(HWND hwnd) {
    HWND hwndParent = GetParent(hwnd);
    if (hwndParent) {
        EnableWindow(hwndParent, TRUE);
    }

    SetClickThrough(hwnd, !CLOCK_EDIT_MODE);

    /* OLE drag/drop is enabled lazily while edit mode is active. */

    StopDrawingRenderAnimationTimer(hwnd);

    /* Initialize high-precision multimedia timer for smooth milliseconds display */
    if (!MainTimer_Init(hwnd, GetTimerInterval())) {
        LOG_WARNING("Failed to initialize high-precision timer, falling back to SetTimer");
    }

    return TRUE;
}

/* ============================================================================
 * Window destruction and cleanup
 * ============================================================================ */

/**
 * Handle WM_DESTROY with ordered resource cleanup to prevent crashes.
 * Critical order: save config → stop timers → remove UI → cleanup resources.
 * 
 * @param hwnd Window handle being destroyed
 */
void HandleWindowDestroy(HWND hwnd) {
    SaveWindowSettings(hwnd);
    CancelScheduledConfigSave(hwnd);
    
    /* Cleanup OLE drag and drop */
    CleanupOleDropTarget(hwnd);

    /* Stop update check if running */
    CleanupUpdateThread();

    /* Cleanup high-precision timer */
    MainTimer_Cleanup();
    KillTimer(hwnd, TIMER_ID_FONT_VALIDATION);
    StopDrawingRenderAnimationTimer(hwnd);
    StopNotificationSound();
    
    KillTimer(hwnd, TIMER_ID_TOPMOST_ENFORCE);
    CleanupWindowDesktopIntegrationState(hwnd);
    CleanupWindowVisualEffects(hwnd);
    
    StopTrayAnimation(hwnd);

    RemoveTrayIcon();
    
    if (!UnloadCurrentFontResource()) {
        LOG_WARNING("Failed to unload font resources");
    }
    
    PostQuitMessage(0);
}

/* ============================================================================
 * Window state management
 * ============================================================================ */

/**
 * Reset window to default state: re-apply current topmost mode and ensure visibility.
 * Called after timer restart to guarantee window is accessible without changing user preference.
 * 
 * @param hwnd Window handle to reset
 */
void HandleWindowReset(HWND hwnd) {
    EnsureWindowVisibleWithTopmostState(hwnd);
}

/* ============================================================================
 * Window transformation handlers
 * ============================================================================ */

/**
 * Handle mouse wheel scaling for window size adjustment.
 * Delegates to drag_scale module for proportional scaling.
 * 
 * @param hwnd Window handle to resize
 * @param delta Mouse wheel delta (positive=zoom in, negative=zoom out)
 * @return TRUE if resize was handled successfully
 */
BOOL HandleWindowResize(HWND hwnd, int delta) {
    return HandleScaleWindow(hwnd, delta);
}

/**
 * Handle window drag movement in edit mode.
 * Delegates to drag_scale module for smooth repositioning.
 * 
 * @param hwnd Window handle being moved
 * @return TRUE if movement was handled successfully
 */
BOOL HandleWindowMove(HWND hwnd) {
    return HandleDragWindow(hwnd);
}
