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
    

    LoadWindowSettings(hwnd);
    

    SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
    

    SetWindowTopmost(hwnd, CLOCK_WINDOW_TOPMOST);
    
    return TRUE;
}

void HandleWindowDestroy(HWND hwnd) {
    SaveWindowSettings(hwnd);
    KillTimer(hwnd, 1);
    RemoveTrayIcon();
    

    extern void CleanupUpdateThread(void);
    CleanupUpdateThread();
    
    PostQuitMessage(0);
}

void HandleWindowReset(HWND hwnd) {


    CLOCK_WINDOW_TOPMOST = TRUE;
    SetWindowTopmost(hwnd, TRUE);
    WriteConfigTopmost("TRUE");
    

    ShowWindow(hwnd, SW_SHOW);
}


BOOL HandleWindowResize(HWND hwnd, int delta) {
    return HandleScaleWindow(hwnd, delta);
}


BOOL HandleWindowMove(HWND hwnd) {
    return HandleDragWindow(hwnd);
}
