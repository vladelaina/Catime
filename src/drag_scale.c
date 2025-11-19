/**
 * @file drag_scale.c
 * @brief Interactive window dragging and scaling with debounced saves
 * 
 * Debounced config saves reduce disk I/O during continuous operations.
 * Centered scaling maintains visual stability during resize.
 */

#include <windows.h>
#include "window.h"
#include "config.h"
#include "drag_scale.h"

BOOL PREVIOUS_TOPMOST_STATE = FALSE;

static UINT_PTR g_configSaveTimer = 0;

static inline void RefreshWindow(HWND hwnd, BOOL eraseBackground) {
    InvalidateRect(hwnd, NULL, eraseBackground);
    UpdateWindow(hwnd);
}

static inline float ClampScaleFactor(float scale) {
    if (scale < MIN_SCALE_FACTOR) return MIN_SCALE_FACTOR;
    if (scale > MAX_SCALE_FACTOR) return MAX_SCALE_FACTOR;
    return scale;
}

static inline int CalculateCenteredPosition(int originalPos, int originalSize, int newSize) {
    return originalPos + (originalSize - newSize) / 2;
}

static VOID CALLBACK ConfigSaveTimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD dwTime) {
    (void)msg;
    (void)dwTime;
    
    if (idEvent == TIMER_ID_CONFIG_SAVE) {
        SaveWindowSettings(hwnd);
        KillTimer(hwnd, TIMER_ID_CONFIG_SAVE);
        g_configSaveTimer = 0;
    }
}

/* Debouncing: Only save after operations stop for CONFIG_SAVE_DELAY_MS */
void ScheduleConfigSave(HWND hwnd) {
    if (g_configSaveTimer != 0) {
        KillTimer(hwnd, TIMER_ID_CONFIG_SAVE);
    }
    
    g_configSaveTimer = SetTimer(hwnd, TIMER_ID_CONFIG_SAVE, 
                                 CONFIG_SAVE_DELAY_MS, 
                                 (TIMERPROC)ConfigSaveTimerProc);
}

void StartDragWindow(HWND hwnd) {
    if (!CLOCK_EDIT_MODE) return;
    
    CLOCK_IS_DRAGGING = TRUE;
    SetCapture(hwnd);
    GetCursorPos(&CLOCK_LAST_MOUSE_POS);
}

void StartEditMode(HWND hwnd) {
    PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
    
    if (!CLOCK_WINDOW_TOPMOST) {
        SetWindowTopmost(hwnd, TRUE);
    }
    
    CLOCK_EDIT_MODE = TRUE;
    
    /* Trigger repaint first with edit mode flag set so FixAlphaChannel runs */
    RefreshWindow(hwnd, TRUE);
    
    /* Blur effect provides visual feedback */
    SetBlurBehind(hwnd, TRUE);
    
    /* Disable click-through to capture mouse events (activates Glass effect) */
    SetClickThrough(hwnd, FALSE);
    
    /* Repaint again after Glass effect is activated to ensure proper rendering */
    RefreshWindow(hwnd, TRUE);
    
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
}

void EndEditMode(HWND hwnd) {
    if (!CLOCK_EDIT_MODE) return;

    CLOCK_EDIT_MODE = FALSE;

    SetBlurBehind(hwnd, FALSE);
    
    SetClickThrough(hwnd, TRUE);
    
    if (!PREVIOUS_TOPMOST_STATE) {
        SetWindowTopmost(hwnd, FALSE);
        
        /* Non-topmost requires full redraw */
        InvalidateRect(hwnd, NULL, TRUE);
        RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
        
        KillTimer(hwnd, TIMER_ID_EDIT_MODE_REFRESH);
        SetTimer(hwnd, TIMER_ID_EDIT_MODE_REFRESH, TIMER_REFRESH_INTERVAL_MS, NULL);
    } else {
        RefreshWindow(hwnd, TRUE);
    }
}

void EndDragWindow(HWND hwnd) {
    if (!CLOCK_EDIT_MODE || !CLOCK_IS_DRAGGING) return;
    
    CLOCK_IS_DRAGGING = FALSE;
    ReleaseCapture();
    
    AdjustWindowPosition(hwnd, FALSE);
    RefreshWindow(hwnd, TRUE);
    ScheduleConfigSave(hwnd);
}

/* SWP_NOREDRAW + UpdateWindow maintains smooth dragging */
BOOL HandleDragWindow(HWND hwnd) {
    if (!CLOCK_EDIT_MODE || !CLOCK_IS_DRAGGING) return FALSE;
    
    POINT currentPos;
    GetCursorPos(&currentPos);
    int deltaX = currentPos.x - CLOCK_LAST_MOUSE_POS.x;
    int deltaY = currentPos.y - CLOCK_LAST_MOUSE_POS.y;
    
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    
    SetWindowPos(hwnd, NULL,
        windowRect.left + deltaX,
        windowRect.top + deltaY,
        width, height,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    
    CLOCK_LAST_MOUSE_POS = currentPos;
    CLOCK_WINDOW_POS_X = windowRect.left + deltaX;
    CLOCK_WINDOW_POS_Y = windowRect.top + deltaY;
    
    UpdateWindow(hwnd);
    ScheduleConfigSave(hwnd);
    
    return TRUE;
}

/* Mouse wheel scaling: ±10% per notch, centered to maintain stability */
BOOL HandleScaleWindow(HWND hwnd, int delta) {
    if (!CLOCK_EDIT_MODE) return FALSE;
    
    float oldScale = CLOCK_FONT_SCALE_FACTOR;
    
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    int oldWidth = windowRect.right - windowRect.left;
    int oldHeight = windowRect.bottom - windowRect.top;
    
    float newScale = (delta > 0) 
        ? oldScale * SCALE_FACTOR_STEP
        : oldScale / SCALE_FACTOR_STEP;
    
    newScale = ClampScaleFactor(newScale);
    
    if (newScale == oldScale) return FALSE;
    
    CLOCK_FONT_SCALE_FACTOR = newScale;
    CLOCK_WINDOW_SCALE = newScale;
    
    float scalingRatio = newScale / oldScale;
    int newWidth = (int)(oldWidth * scalingRatio);
    int newHeight = (int)(oldHeight * scalingRatio);
    
    /* Center scaling to keep window visually stable */
    int newX = CalculateCenteredPosition(windowRect.left, oldWidth, newWidth);
    int newY = CalculateCenteredPosition(windowRect.top, oldHeight, newHeight);
    
    SetWindowPos(hwnd, NULL, 
        newX, newY, newWidth, newHeight,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    
    RefreshWindow(hwnd, FALSE);
    ScheduleConfigSave(hwnd);
    
    return TRUE;
}