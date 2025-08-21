/**
 * @file window_events.c
 * @brief Core window event handlers for lifecycle and state management
 * Handles window creation, destruction, resizing, and state transitions
 */
#include <windows.h>
#include "../include/window.h"
#include "../include/tray.h"
#include "../include/config.h"
#include "../include/drag_scale.h"
#include "../include/window_events.h"

/**
 * @brief Handle window creation and initial setup
 * @param hwnd Window handle of the newly created window
 * @return TRUE if initialization succeeded, FALSE on error
 * Configures window properties, loads settings, and sets initial state
 */
BOOL HandleWindowCreate(HWND hwnd) {
    /** Enable parent window if this is a child window */
    HWND hwndParent = GetParent(hwnd);
    if (hwndParent != NULL) {
        EnableWindow(hwndParent, TRUE);
    }
    
    /** Load persisted window settings from configuration */
    LoadWindowSettings(hwnd);
    
    /** Set click-through behavior based on edit mode */
    SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
    
    /** Apply topmost window setting */
    SetWindowTopmost(hwnd, CLOCK_WINDOW_TOPMOST);
    
    return TRUE;
}

/**
 * @brief Handle window destruction and cleanup
 * @param hwnd Window handle being destroyed
 * Saves settings, stops timers, cleans up resources, and initiates application exit
 */
void HandleWindowDestroy(HWND hwnd) {
    /** Save current window settings before destruction */
    SaveWindowSettings(hwnd);
    
    /** Stop main timer to prevent further updates */
    KillTimer(hwnd, 1);
    
    /** Remove system tray icon */
    RemoveTrayIcon();
    
    /** Clean up font resources */
    extern BOOL UnloadCurrentFontResource(void);
    UnloadCurrentFontResource();
    
    /** Clean up background update checking thread */
    extern void CleanupUpdateThread(void);
    CleanupUpdateThread();
    
    /** Signal application to exit */
    PostQuitMessage(0);
}

/**
 * @brief Reset window to default state and ensure visibility
 * @param hwnd Window handle to reset
 * Forces window to topmost state, saves setting, and ensures visibility
 */
void HandleWindowReset(HWND hwnd) {
    /** Force enable topmost behavior */
    CLOCK_WINDOW_TOPMOST = TRUE;
    SetWindowTopmost(hwnd, TRUE);
    WriteConfigTopmost("TRUE");
    
    /** Ensure window is visible */
    ShowWindow(hwnd, SW_SHOW);
}

/**
 * @brief Handle window resizing through mouse wheel scaling
 * @param hwnd Window handle to resize
 * @param delta Mouse wheel delta value for scaling direction and amount
 * @return TRUE if resize was handled, FALSE otherwise
 * Delegates to scaling system for proportional window size adjustment
 */
BOOL HandleWindowResize(HWND hwnd, int delta) {
    return HandleScaleWindow(hwnd, delta);
}

/**
 * @brief Handle window movement through drag operations
 * @param hwnd Window handle being moved
 * @return TRUE if movement was handled, FALSE otherwise
 * Delegates to drag system for smooth window repositioning
 */
BOOL HandleWindowMove(HWND hwnd) {
    return HandleDragWindow(hwnd);
}
