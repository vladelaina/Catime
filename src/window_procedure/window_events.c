/**
 * @file window_events.c
 * @brief Window lifecycle and state management event handlers
 */
#include <windows.h>
#include "window.h"
#include "window/window_multimonitor.h"
#include "tray/tray.h"
#include "config.h"
#include "drag_scale.h"
#include "window_procedure/window_events.h"
#include "window_procedure/window_utils.h"
#include "window_procedure/ole_drop_target.h"
#include "log.h"
#include "timer/timer.h"
#include "tray/tray_animation_core.h"
#include "async_update_checker.h"
#include "../resource/resource.h"

/* ============================================================================
 * Window creation and initialization
 * ============================================================================ */

/**
 * Handle WM_CREATE initialization: position, click-through, and topmost mode.
 *
 * @param hwnd Window handle of newly created window
 * @return TRUE if initialization succeeded
 *
 * @note Position validation is skipped during startup to avoid false positives
 *       when display information may not be fully initialized yet.
 */
BOOL HandleWindowCreate(HWND hwnd) {
    LOG_INFO("Window creation started");

    HWND hwndParent = GetParent(hwnd);
    if (hwndParent) {
        EnableWindow(hwndParent, TRUE);
        LOG_INFO("Parent window enabled");
    }

    /* Validate window position after creation - move to default if off-screen */
    LOG_INFO("Validating window position after creation");
    AdjustWindowPosition(hwnd, TRUE);

    SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
    LOG_INFO("Click-through mode set: %s", CLOCK_EDIT_MODE ? "disabled" : "enabled");

    SetWindowTopmost(hwnd, CLOCK_WINDOW_TOPMOST);
    LOG_INFO("Window topmost setting applied: %s", CLOCK_WINDOW_TOPMOST ? "yes" : "no");

    /* Enable OLE drag and drop for resource import with preview */
    InitializeOleDropTarget(hwnd);
    LOG_INFO("OLE Drag and drop enabled (requires Edit Mode if Click-Through is active)");

    /* Start Animation Timer if effects are active (Fixes startup animation issue) */
    /* Use adaptive interval based on window size to prevent mouse lag */
    if (CLOCK_LIQUID_EFFECT || CLOCK_HOLOGRAPHIC_EFFECT || 
        CLOCK_NEON_EFFECT || CLOCK_GLOW_EFFECT || CLOCK_GLASS_EFFECT) {
        RECT rect;
        GetClientRect(hwnd, &rect);
        int pixels = rect.right * rect.bottom;
        
        /* Holographic effect needs more aggressive throttling */
        UINT interval;
        if (CLOCK_HOLOGRAPHIC_EFFECT) {
            interval = (pixels < 30000) ? 50 : 
                       (pixels < 100000) ? 80 : 
                       (pixels < 300000) ? 120 : 200;
        } else {
            interval = (pixels < 50000) ? 33 : 
                       (pixels < 200000) ? 50 : 
                       (pixels < 500000) ? 80 : 120;
        }
        SetTimer(hwnd, TIMER_ID_RENDER_ANIMATION, interval, NULL); 
        LOG_INFO("Animation render timer started (adaptive interval: %ums)", interval);
    }

    LOG_INFO("Window creation completed successfully");
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
    LOG_INFO("Window destruction started");
    
    SaveWindowSettings(hwnd);
    LOG_INFO("Window settings saved successfully");
    
    /* Cleanup OLE drag and drop */
    CleanupOleDropTarget(hwnd);

    /* Stop update check if running */
    CleanupUpdateThread();

    KillTimer(hwnd, TIMER_ID_MAIN);
    KillTimer(hwnd, TIMER_ID_TOPMOST_ENFORCE);
    extern UINT GetClickThroughTimerId(void);
    KillTimer(hwnd, GetClickThroughTimerId());
    LOG_INFO("Timers stopped");
    
    RemoveTrayIcon();
    LOG_INFO("Tray icon removed");
    
    StopTrayAnimation(hwnd);
    LOG_INFO("Tray animation stopped");
    
    if (!UnloadCurrentFontResource()) {
        LOG_WARNING("Failed to unload font resources");
    } else {
        LOG_INFO("Font resources unloaded");
    }
    
    CleanupUpdateThread();
    LOG_INFO("Update checker thread cleaned up");
    
    PostQuitMessage(0);
    LOG_INFO("Window destruction completed, application will exit");
}

/* ============================================================================
 * Window state management
 * ============================================================================ */

/**
 * Reset window to default state: force topmost and ensure visibility.
 * Called after timer restart to guarantee window is accessible.
 * 
 * @param hwnd Window handle to reset
 */
void HandleWindowReset(HWND hwnd) {
    LOG_INFO("Window reset initiated");
    
    CLOCK_WINDOW_TOPMOST = TRUE;
    SetWindowTopmost(hwnd, TRUE);
    WriteConfigTopmost("TRUE");
    LOG_INFO("Window topmost forced to enabled and saved");
    
    ShowWindow(hwnd, SW_SHOW);
    LOG_INFO("Window visibility ensured");
    
    LOG_INFO("Window reset completed");
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
    BOOL result = HandleScaleWindow(hwnd, delta);
    if (result) {
        LOG_INFO("Window resize handled (delta: %d)", delta);
    }
    return result;
}

/**
 * Handle window drag movement in edit mode.
 * Delegates to drag_scale module for smooth repositioning.
 * 
 * @param hwnd Window handle being moved
 * @return TRUE if movement was handled successfully
 */
BOOL HandleWindowMove(HWND hwnd) {
    BOOL result = HandleDragWindow(hwnd);
    if (result) {
        LOG_INFO("Window drag operation handled");
    }
    return result;
}
