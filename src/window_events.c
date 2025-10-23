/**
 * @file window_events.c
 * @brief Window lifecycle and state management event handlers
 * @version 2.0 - Refactored for improved error handling, logging, and code quality
 * 
 * Provides centralized window event handling with:
 * - Comprehensive logging for all lifecycle events
 * - Robust error handling and validation
 * - Proper resource cleanup sequencing
 * - Eliminated magic numbers and external declarations
 */
#include <windows.h>
#include "../include/window.h"
#include "../include/tray.h"
#include "../include/config.h"
#include "../include/drag_scale.h"
#include "../include/window_events.h"
#include "../include/tray_animation.h"
#include "../include/timer_events.h"
#include "../include/font.h"
#include "../include/async_update_checker.h"
#include "../include/log.h"

/* ============================================================================
 * Window creation and initialization
 * ============================================================================ */

/**
 * @brief Handle window creation and initial setup
 * @param hwnd Window handle of the newly created window
 * @return TRUE if initialization succeeded, FALSE on error
 * 
 * Initialization sequence:
 * 1. Enable parent window (if child window)
 * 2. Set initial position and size
 * 3. Configure click-through behavior
 * 4. Apply topmost setting
 */
BOOL HandleWindowCreate(HWND hwnd) {
    LOG_INFO("Window creation started");
    
    // Enable parent window if this is a child window
    HWND hwndParent = GetParent(hwnd);
    if (hwndParent) {
        EnableWindow(hwndParent, TRUE);
        LOG_INFO("Parent window enabled");
    }
    
    // Set initial window position and size
    AdjustWindowPosition(hwnd, TRUE);
    LOG_INFO("Window position adjusted");
    
    // Set click-through behavior based on edit mode
    SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
    LOG_INFO("Click-through mode set: %s", CLOCK_EDIT_MODE ? "disabled" : "enabled");
    
    // Apply topmost window setting
    SetWindowTopmost(hwnd, CLOCK_WINDOW_TOPMOST);
    LOG_INFO("Window topmost setting applied: %s", CLOCK_WINDOW_TOPMOST ? "yes" : "no");
    
    LOG_INFO("Window creation completed successfully");
    return TRUE;
}

/* ============================================================================
 * Window destruction and cleanup
 * ============================================================================ */

/**
 * @brief Handle window destruction with proper resource cleanup sequence
 * @param hwnd Window handle being destroyed
 * 
 * Cleanup sequence (order is critical to prevent crashes):
 * 1. Save configuration to disk
 * 2. Stop timers to prevent further callbacks
 * 3. Remove UI elements (tray icon, animations)
 * 4. Clean up resources (fonts, threads)
 * 5. Signal application exit
 */
void HandleWindowDestroy(HWND hwnd) {
    LOG_INFO("Window destruction started");
    
    // 1. Save configuration
    SaveWindowSettings(hwnd);
    LOG_INFO("Window settings saved successfully");
    
    // 2. Stop timers
    KillTimer(hwnd, TIMER_ID_MAIN);
    LOG_INFO("Main timer stopped");
    
    // 3. Clean up UI elements
    RemoveTrayIcon();
    LOG_INFO("Tray icon removed");
    
    StopTrayAnimation(hwnd);
    LOG_INFO("Tray animation stopped");
    
    // 4. Clean up resources
    if (!UnloadCurrentFontResource()) {
        LOG_WARNING("Failed to unload font resources");
    } else {
        LOG_INFO("Font resources unloaded");
    }
    
    CleanupUpdateThread();
    LOG_INFO("Update checker thread cleaned up");
    
    // 5. Signal exit
    PostQuitMessage(0);
    LOG_INFO("Window destruction completed, application will exit");
}

/* ============================================================================
 * Window state management
 * ============================================================================ */

/**
 * @brief Reset window to default state and ensure visibility
 * @param hwnd Window handle to reset
 * 
 * Reset operations:
 * - Force enable topmost behavior
 * - Save setting to configuration
 * - Ensure window visibility
 */
void HandleWindowReset(HWND hwnd) {
    LOG_INFO("Window reset initiated");
    
    // Force enable topmost behavior
    CLOCK_WINDOW_TOPMOST = TRUE;
    SetWindowTopmost(hwnd, TRUE);
    WriteConfigTopmost("TRUE");
    LOG_INFO("Window topmost forced to enabled and saved");
    
    // Ensure window is visible
    ShowWindow(hwnd, SW_SHOW);
    LOG_INFO("Window visibility ensured");
    
    LOG_INFO("Window reset completed");
}

/* ============================================================================
 * Window transformation handlers
 * ============================================================================ */

/**
 * @brief Handle window resizing through mouse wheel scaling
 * @param hwnd Window handle to resize
 * @param delta Mouse wheel delta value for scaling direction and amount
 * @return TRUE if resize was handled successfully, FALSE otherwise
 * 
 * Delegates to drag_scale module for proportional window size adjustment
 */
BOOL HandleWindowResize(HWND hwnd, int delta) {
    BOOL result = HandleScaleWindow(hwnd, delta);
    if (result) {
        LOG_INFO("Window resize handled (delta: %d)", delta);
    }
    return result;
}

/**
 * @brief Handle window movement through drag operations
 * @param hwnd Window handle being moved
 * @return TRUE if movement was handled successfully, FALSE otherwise
 * 
 * Delegates to drag_scale module for smooth window repositioning
 */
BOOL HandleWindowMove(HWND hwnd) {
    BOOL result = HandleDragWindow(hwnd);
    if (result) {
        LOG_INFO("Window drag operation handled");
    }
    return result;
}
