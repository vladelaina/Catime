#include <windows.h>
#include "../include/window.h"
#include "../include/tray.h"
#include "../include/config.h"
#include "../include/drag_scale.h"
#include "../include/window_events.h"

BOOL HandleWindowCreate(HWND hwnd) {
    HWND hwndParent = GetParent(hwnd);
    if (hwndParent != NULL) {
        EnableWindow(hwndParent, TRUE);
    }
    
    // Load window settings
    LoadWindowSettings(hwnd);
    
    // Set click-through
    SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
    
    // Ensure window is in topmost state
    SetWindowTopmost(hwnd, CLOCK_WINDOW_TOPMOST);
    
    return TRUE;
}

void HandleWindowDestroy(HWND hwnd) {
    SaveWindowSettings(hwnd);  // Save window settings
    KillTimer(hwnd, 1);
    RemoveTrayIcon();
    
    // Clean up update check thread
    extern void CleanupUpdateThread(void);
    CleanupUpdateThread();
    
    PostQuitMessage(0);
}

void HandleWindowReset(HWND hwnd) {
    // Unconditionally apply topmost setting from configuration
    // Regardless of the current CLOCK_WINDOW_TOPMOST value, force it to TRUE and apply
    CLOCK_WINDOW_TOPMOST = TRUE;
    SetWindowTopmost(hwnd, TRUE);
    WriteConfigTopmost("TRUE");
    
    // Ensure window is always visible - solves the issue of timer not being visible after reset
    ShowWindow(hwnd, SW_SHOW);
}

// This function has been moved to drag_scale.c
BOOL HandleWindowResize(HWND hwnd, int delta) {
    return HandleScaleWindow(hwnd, delta);
}

// This function has been moved to drag_scale.c
BOOL HandleWindowMove(HWND hwnd) {
    return HandleDragWindow(hwnd);
}
